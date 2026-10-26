#include "LocalOcr.h"

#include "core/translation/Onnx.h"
#include "core/Http.h"

#include <QFile>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace sc::translation {

namespace {

// PaddleOCR's own settings (inference.yml, DBPostProcess).
constexpr float kThresh = 0.3f;
constexpr float kBoxThresh = 0.6f;
constexpr double kUnclip = 1.5;
constexpr int kLineHeight = 48;
// Pages are found at this width at most; tall strips keep their scale, so
// their text stays as big as a phone shows it.
constexpr int kDetWidth = 1280;

int round32(double v) { return std::max(32, int(std::lround(v / 32.0)) * 32); }

// The picture as the models want it: BGR planes, each channel normalised.
std::vector<float> planes(const QImage &image, const float mean[3], const float scale[3]) {
    const auto rgb = image.convertToFormat(QImage::Format_RGB888);
    const int w = rgb.width(), h = rgb.height();
    std::vector<float> out(size_t(3) * w * h);
    for (int y = 0; y < h; ++y) {
        const uchar *row = rgb.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const float bgr[3] = {row[3 * x + 2] / 255.f, row[3 * x + 1] / 255.f, row[3 * x] / 255.f};
            for (int c = 0; c < 3; ++c)
                out[size_t(c) * w * h + size_t(y) * w + x] = (bgr[c] - mean[c]) * scale[c];
        }
    }
    return out;
}

} // namespace

QStringList charactersFromYaml(const QByteArray &yaml) {
    QStringList out;
    bool inside = false;
    for (const auto &raw : yaml.split('\n')) {
        const auto line = QString::fromUtf8(raw);
        if (!inside) {
            inside = line.trimmed() == QLatin1String("character_dict:");
            continue;
        }
        if (!line.startsWith(QLatin1String("  - ")))
            break;
        auto value = line.mid(4);
        if (value.size() >= 2 && value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))) {
            value = value.mid(1, value.size() - 2).replace(QLatin1String("''"), QLatin1String("'"));
        } else if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
            const auto inner = value.mid(1, value.size() - 2);
            QString decoded;
            for (int i = 0; i < inner.size(); ++i) {
                if (inner[i] != QLatin1Char('\\') || i + 1 >= inner.size()) {
                    decoded += inner[i];
                    continue;
                }
                const auto e = inner[++i];
                if (e == QLatin1Char('u') && i + 4 < inner.size()) {
                    decoded += QChar(char16_t(inner.mid(i + 1, 4).toUShort(nullptr, 16)));
                    i += 4;
                } else if (e == QLatin1Char('t')) {
                    decoded += QLatin1Char('\t');
                } else {
                    decoded += e;
                }
            }
            value = decoded;
        }
        out.append(value);
    }
    return out;
}

LocalOcr::LocalOcr(const QString &detector, const QString &recognizer, const QString &characters) {
    QFile file(characters);
    if (!file.open(QIODevice::ReadOnly))
        throw Error(QStringLiteral("The text-reading model is missing a file. Download it again."));
    // One character a line; a line may be a space.
    for (const auto &line : file.readAll().split('\n'))
        if (!line.isEmpty())
            m_characters.append(QString::fromUtf8(line));
    m_detector = std::make_unique<onnx::Session>(detector);
    m_recognizer = std::make_unique<onnx::Session>(recognizer);
}

LocalOcr::~LocalOcr() = default;

QList<QRect> LocalOcr::find(const QImage &image) const {
    if (image.isNull())
        return {};
    const double scale = std::min(1.0, double(kDetWidth) / image.width());
    const int w = round32(image.width() * scale), h = round32(image.height() * scale);
    const auto small = image.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float scaleBy[3] = {1 / 0.229f, 1 / 0.224f, 1 / 0.225f};
    const auto map = m_detector->run({{m_detector->inputs().at(0), onnx::Tensor::ofFloats({1, 3, h, w}, planes(small, mean, scaleBy))}}).at(0);
    if (map.shape.size() != 4 || map.shape[2] != h || map.shape[3] != w)
        throw Error(QStringLiteral("The text-finding model answered in an unexpected shape."));
    const float *p = map.floats.data();

    // Regions of likely text (8-connected), each scored by its mean and
    // grown by PaddleOCR's unclip distance (area x ratio / perimeter).
    std::vector<int> label(size_t(w) * h, 0);
    std::vector<int> stack;
    QList<QRect> out;
    int next = 0;
    const double back = 1.0 / scale;
    for (int start = 0; start < w * h; ++start) {
        if (label[start] || p[start] <= kThresh)
            continue;
        ++next;
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        double sum = 0;
        int count = 0;
        stack.assign(1, start);
        label[start] = next;
        while (!stack.empty()) {
            const int at = stack.back();
            stack.pop_back();
            const int x = at % w, y = at / w;
            x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
            sum += p[at];
            ++count;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if ((dx || dy) && nx >= 0 && ny >= 0 && nx < w && ny < h) {
                        const int n = ny * w + nx;
                        if (!label[n] && p[n] > kThresh) {
                            label[n] = next;
                            stack.push_back(n);
                        }
                    }
                }
        }
        const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
        if (std::min(bw, bh) < 3 || sum / count < kBoxThresh)
            continue;
        const double grow = double(bw) * bh * kUnclip / (2.0 * (bw + bh));
        QRectF box(x0 - grow, y0 - grow, bw + 2 * grow, bh + 2 * grow);
        box = QRectF(box.x() * back, box.y() * back, box.width() * back, box.height() * back);
        out.append(box.toAlignedRect().intersected(image.rect()));
    }
    return out;
}

QString LocalOcr::read(const QImage &line) const {
    if (line.isNull() || line.height() < 2)
        return {};
    // Columns (vertical text) are turned to lie down, as PaddleOCR does.
    auto crop = line.height() >= line.width() * 1.5 ? line.transformed(QTransform().rotate(-90)) : line;
    const int width = std::clamp(int(std::ceil(double(kLineHeight) * crop.width() / crop.height())), 8, 3200);
    crop = crop.scaled(width, kLineHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    static const float mean[3] = {0.5f, 0.5f, 0.5f};
    static const float scaleBy[3] = {2.f, 2.f, 2.f};
    const auto scores = m_recognizer->run({{m_recognizer->inputs().at(0), onnx::Tensor::ofFloats({1, 3, kLineHeight, width}, planes(crop, mean, scaleBy))}}).at(0);
    if (scores.shape.size() != 3)
        throw Error(QStringLiteral("The text-reading model answered in an unexpected shape."));
    // CTC: the best class at each step; 0 is "nothing", repeats are one.
    const auto steps = scores.shape[1], classes = scores.shape[2];
    QString text;
    int64_t last = 0;
    for (int64_t t = 0; t < steps; ++t) {
        const float *row = scores.floats.data() + t * classes;
        const auto best = int64_t(std::max_element(row, row + classes) - row);
        if (best != 0 && best != last) {
            // After the dictionary comes the space.
            text += best - 1 < m_characters.size() ? m_characters[best - 1] : QStringLiteral(" ");
        }
        last = best;
    }
    return text.simplified();
}

QList<TextLine> LocalOcr::lines(const QImage &image) const {
    QList<TextLine> out;
    for (const auto &box : find(image)) {
        const auto text = read(image.copy(box));
        if (!text.isEmpty())
            out.append({QRectF(box), text});
    }
    return out;
}

} // namespace sc::translation
