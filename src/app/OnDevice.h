#pragma once

#include <QImage>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantList>

#include "core/translation/Layout.h"

namespace sc {

class Task;

// Translating comics and novels on the computer, with nothing sent anywhere
// (and enlarging downloaded pages):
// ONNX Runtime (Microsoft, MIT), PaddleOCR's PP-OCRv5 models (Apache-2.0)
// to read a page's text and Opus-MT (Helsinki-NLP, CC-BY 4.0) to translate
// it. None of it ships with SCANCORE: one download per language brings what
// reading and translating it needs (the runtime the first time, the
// script's text reader, and the language's model, or two through English),
// each file checked against the revision pinned here.
class OnDevice : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // This computer can have it (ONNX Runtime is built for it).
    Q_PROPERTY(bool available READ available CONSTANT)
    // Bumped when a language is added or removed.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    static OnDevice *instance();
    static OnDevice *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool available() const;
    int revision() const { return m_revision; }

    // The languages that can be translated into `target`: [{language,
    // label, size, installed, through (true when it goes through English)}].
    Q_INVOKABLE QVariantList languages(const QString &target) const;
    // Downloads what translating `language` into `target` needs; the task's
    // partial result is what it is doing ({status}).
    Q_INVOKABLE sc::Task *install(const QString &language, const QString &target);
    Q_INVOKABLE void remove(const QString &language, const QString &target);
    // Anything installed at all.
    Q_INVOKABLE bool any() const;

    // Enlarging downloaded pages twice (waifu2x, about 5 MB with the
    // runtime the first time); see Downloads.
    Q_INVOKABLE bool upscalerInstalled() const;
    Q_INVOKABLE sc::Task *installUpscaler();
    // From a download's thread; throws sc::Error when it is not installed.
    QImage upscale(const QImage &image) const;

    // From the translator's threads. `language` "" guesses from what is
    // installed. Throw sc::Error when the models are missing.
    bool canRead(const QString &language) const;
    QList<translation::TextLine> read(const QImage &image, const QString &language) const;
    bool canTranslate(const QString &from, const QString &to) const;
    QStringList translate(const QStringList &texts, const QString &from, const QString &to) const;

signals:
    void changed();

private:
    explicit OnDevice(QObject *parent = nullptr);
    int m_revision = 0;
};

} // namespace sc
