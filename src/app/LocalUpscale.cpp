#include "LocalUpscale.h"

#include "core/Http.h"
#include "core/translation/Onnx.h"

#include <algorithm>
#include <cmath>

namespace sc::translation {

namespace {

// CUNet answers 2 x input - 72: it drops 18 pixels of each side as context.
constexpr int kPad = 18;
// Each tile adds this much of the page (and twice as much to the answer).
constexpr int kCore = 220;

} // namespace

Waifu2x::Waifu2x(const QString &model) : m_model(std::make_unique<onnx::Session>(model)) {}

Waifu2x::~Waifu2x() = default;

QImage Waifu2x::upscale(const QImage &image) const {
    if (image.isNull())
        return {};
    const auto rgb = image.convertToFormat(QImage::Format_RGB888);
    const int width = rgb.width(), height = rgb.height();
    QImage out(width * 2, height * 2, QImage::Format_RGB888);
    for (int top = 0; top < height; top += kCore) {
        for (int left = 0; left < width; left += kCore) {
            const int cw = std::min(kCore, width - left), ch = std::min(kCore, height - top);
            // CUNet takes even sizes only: an odd edge tile gets one more
            // column or row, cut off again from the answer.
            const int iw = cw + 2 * kPad + (cw & 1), ih = ch + 2 * kPad + (ch & 1);
            // The tile with its context; past the page's edge, the edge
            // pixel again.
            std::vector<float> planes(size_t(3) * iw * ih);
            for (int y = 0; y < ih; ++y) {
                const uchar *row = rgb.constScanLine(std::clamp(top + y - kPad, 0, height - 1));
                for (int x = 0; x < iw; ++x) {
                    const uchar *p = row + 3 * std::clamp(left + x - kPad, 0, width - 1);
                    for (int c = 0; c < 3; ++c)
                        planes[size_t(c) * iw * ih + size_t(y) * iw + x] = p[c] / 255.f;
                }
            }
            const auto result = m_model->run({{m_model->inputs().at(0), onnx::Tensor::ofFloats({1, 3, ih, iw}, std::move(planes))}}).at(0);
            const int ow = 2 * iw - 4 * kPad, oh = 2 * ih - 4 * kPad;
            if (result.shape.size() != 4 || result.shape[2] != oh || result.shape[3] != ow)
                throw Error(QStringLiteral("The upscaler answered in an unexpected shape."));
            const float *data = result.floats.data();
            for (int y = 0; y < 2 * ch; ++y) {
                uchar *row = out.scanLine(2 * top + y) + 3 * 2 * left;
                for (int x = 0; x < 2 * cw; ++x)
                    for (int c = 0; c < 3; ++c)
                        row[3 * x + c] = uchar(std::lround(std::clamp(data[size_t(c) * ow * oh + size_t(y) * ow + x], 0.f, 1.f) * 255.f));
            }
        }
    }
    return out;
}

} // namespace sc::translation
