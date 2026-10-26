#pragma once

#include <QImage>
#include <QString>

#include <memory>

namespace sc::onnx {
class Session;
}

namespace sc::translation {

// Enlarging a comic page twice on the computer with waifu2x (nunif's CUNet
// "art" model with light noise removal, MIT) in ONNX, a tile at a time so
// any page fits in memory.
class Waifu2x {
public:
    // Throws sc::Error when the model is not usable.
    explicit Waifu2x(const QString &model);
    ~Waifu2x();

    // Twice as wide and tall (opaque; transparency is dropped).
    QImage upscale(const QImage &image) const;

private:
    std::unique_ptr<onnx::Session> m_model;
};

} // namespace sc::translation
