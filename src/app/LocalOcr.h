#pragma once

#include "core/translation/Layout.h"

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

namespace sc::onnx {
class Session;
}

namespace sc::translation {

// Reading a page's text on the computer with PaddleOCR's PP-OCRv5 models
// (Apache-2.0) in ONNX: one model finds the lines, one per script reads
// them. The folder holds det.onnx, and rec.onnx with rec.txt (one
// character a line) for each script, as OnDevice downloads them.
class LocalOcr {
public:
    // Throws sc::Error when the models are not usable.
    LocalOcr(const QString &detector, const QString &recognizer, const QString &characters);
    ~LocalOcr();

    // The lines on `image`, in its pixels.
    QList<TextLine> lines(const QImage &image) const;

    // Where the text is, before it is read (tests).
    QList<QRect> find(const QImage &image) const;
    QString read(const QImage &line) const;

private:
    std::unique_ptr<onnx::Session> m_detector, m_recognizer;
    QStringList m_characters;
};

// The characters of a PaddleOCR inference.yml's character_dict, one a line
// (OnDevice stores them so, at install).
QStringList charactersFromYaml(const QByteArray &yaml);

} // namespace sc::translation
