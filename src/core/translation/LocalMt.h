#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

namespace sc::onnx {
class Session;
}

namespace sc::translation {

// Translating on the computer with an Opus-MT model (Helsinki-NLP, CC-BY
// 4.0) in ONNX: a folder with encoder.onnx, decoder.onnx, tokenizer.json and
// config.json, as OnDevice downloads them.
class Marian {
public:
    // Throws sc::Error when the folder is not a usable model.
    explicit Marian(const QString &dir);
    ~Marian();

    // One text (a bubble, a paragraph); "" stays "".
    QString translate(const QString &text) const;

    // The pieces' ids for `text`, ending with </s> (tests).
    QVector<qint64> encode(const QString &text) const;
    QString decode(const QVector<qint64> &ids) const;

private:
    QVector<qint64> encodeWord(const QString &word) const;

    std::unique_ptr<onnx::Session> m_encoder, m_decoder;
    QHash<QString, int> m_ids;
    QVector<QString> m_pieces;
    QVector<float> m_scores;
    int m_longest = 1;
    float m_unkScore = -100;
    qint64 m_unk = 1, m_eos = 0, m_pad = 0, m_start = 0;
    int m_vocab = 0;
};

} // namespace sc::translation
