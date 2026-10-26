#include "LocalMt.h"

#include "Onnx.h"
#include "core/Http.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sc::translation {

namespace {

const QString kSpace = QStringLiteral("▁");

QJsonObject readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw Error(QStringLiteral("The translation model is missing a file (%1). Download it again.").arg(path.section(QLatin1Char('/'), -1)));
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

Marian::Marian(const QString &dir) {
    // The vocabulary both sides share, with each piece's score: the source
    // side's pieces have theirs; the target's alone are at -100, so they are
    // never chosen for the source (tokenizer.json, as transformers.js reads it).
    const auto tokenizer = readJson(dir + QStringLiteral("/tokenizer.json"));
    const auto model = tokenizer.value(QStringLiteral("model")).toObject();
    if (model.value(QStringLiteral("type")).toString() != QLatin1String("Unigram"))
        throw Error(QStringLiteral("This translation model's tokenizer is not supported."));
    const auto vocab = model.value(QStringLiteral("vocab")).toArray();
    m_vocab = int(vocab.size());
    m_pieces.reserve(m_vocab);
    m_scores.reserve(m_vocab);
    float lowest = 0;
    for (int i = 0; i < vocab.size(); ++i) {
        const auto entry = vocab[i].toArray();
        const auto piece = entry[0].toString();
        const auto score = float(entry[1].toDouble());
        m_pieces.append(piece);
        m_scores.append(score);
        m_ids.insert(piece, i);
        m_longest = std::max(m_longest, int(piece.size()));
        if (score > -100)
            lowest = std::min(lowest, score);
    }
    // SentencePiece's own: an unknown character costs 10 below the worst piece.
    m_unkScore = lowest - 10;
    m_unk = m_ids.value(QStringLiteral("<unk>"), 1);
    const auto config = readJson(dir + QStringLiteral("/config.json"));
    m_eos = config.value(QStringLiteral("eos_token_id")).toInteger(0);
    m_pad = config.value(QStringLiteral("pad_token_id")).toInteger(m_vocab - 1);
    m_start = config.value(QStringLiteral("decoder_start_token_id")).toInteger(m_pad);

    m_encoder = std::make_unique<onnx::Session>(dir + QStringLiteral("/encoder.onnx"));
    m_decoder = std::make_unique<onnx::Session>(dir + QStringLiteral("/decoder.onnx"));
}

Marian::~Marian() = default;

// The best split of one word into pieces (Viterbi over their scores).
QVector<qint64> Marian::encodeWord(const QString &word) const {
    const int n = int(word.size());
    constexpr float none = -std::numeric_limits<float>::infinity();
    QVector<float> best(n + 1, none);
    QVector<int> from(n + 1, -1);
    QVector<qint64> via(n + 1, -1);
    best[0] = 0;
    for (int i = 0; i < n; ++i) {
        if (best[i] == none || word[i].isLowSurrogate())
            continue;
        bool covered = false;
        for (int len = 1; len <= std::min(m_longest, n - i); ++len) {
            if (i + len < n && word[i + len].isLowSurrogate())
                continue;
            const auto it = m_ids.constFind(word.mid(i, len));
            if (it == m_ids.cend())
                continue;
            covered = covered || len == 1 || (len == 2 && word[i].isHighSurrogate());
            const float score = best[i] + m_scores[*it];
            if (score > best[i + len]) {
                best[i + len] = score;
                from[i + len] = i;
                via[i + len] = *it;
            }
        }
        if (!covered) {
            // A character no piece starts with alone: <unk>.
            const int len = word[i].isHighSurrogate() && i + 1 < n ? 2 : 1;
            const float score = best[i] + m_unkScore;
            if (score > best[i + len]) {
                best[i + len] = score;
                from[i + len] = i;
                via[i + len] = m_unk;
            }
        }
    }
    QVector<qint64> out;
    for (int at = n; at > 0 && from[at] >= 0; at = from[at])
        out.prepend(via[at]);
    // Runs of unknown characters are one <unk>, as SentencePiece makes them.
    out.erase(std::unique(out.begin(), out.end(), [this](qint64 a, qint64 b) { return a == m_unk && b == m_unk; }), out.end());
    return out;
}

QVector<qint64> Marian::encode(const QString &text) const {
    QVector<qint64> ids;
    for (const auto &word : text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts))
        ids += encodeWord(kSpace + word);
    ids.append(m_eos);
    return ids;
}

QString Marian::decode(const QVector<qint64> &ids) const {
    QString out;
    for (const auto id : ids) {
        if (id == m_eos || id == m_pad || id == m_unk || id < 0 || id >= m_vocab)
            continue;
        out += m_pieces[id];
    }
    return out.replace(kSpace, QStringLiteral(" ")).trimmed();
}

QString Marian::translate(const QString &text) const {
    const auto source = text.simplified();
    if (source.isEmpty())
        return {};
    // Long texts in sentences' worth: the models were trained on those.
    auto ids = encode(source);
    if (ids.size() > 400) {
        ids.resize(399);
        ids.append(m_eos);
    }
    const auto n = int64_t(ids.size());
    std::vector<int64_t> input(ids.begin(), ids.end());
    std::vector<int64_t> mask(size_t(n), 1);
    const auto hidden = m_encoder->run({{"input_ids", onnx::Tensor::ofInts({1, n}, input)}, {"attention_mask", onnx::Tensor::ofInts({1, n}, mask)}})
                            .at(0);

    // Greedy, one piece at a time (no beam): fast, and good enough for a
    // bubble's worth of text.
    std::vector<int64_t> out{m_start};
    const auto limit = std::min<int64_t>(512, 3 * n + 16);
    while (int64_t(out.size()) < limit) {
        const auto t = int64_t(out.size());
        const auto logits = m_decoder->run({{"encoder_attention_mask", onnx::Tensor::ofInts({1, n}, mask)},
                                            {"input_ids", onnx::Tensor::ofInts({1, t}, out)},
                                            {"encoder_hidden_states", hidden}},
                                           {"logits"})
                                .at(0);
        const auto vocab = logits.shape.back();
        const float *last = logits.floats.data() + (t - 1) * vocab;
        int64_t next = 0;
        float top = -std::numeric_limits<float>::infinity();
        for (int64_t v = 0; v < vocab; ++v)
            if (v != m_pad && last[v] > top) {
                top = last[v];
                next = v;
            }
        if (next == m_eos)
            break;
        out.push_back(next);
    }
    return decode(QVector<qint64>(out.begin() + 1, out.end()));
}

} // namespace sc::translation
