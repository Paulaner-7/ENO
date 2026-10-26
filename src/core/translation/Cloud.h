#pragma once

#include "Layout.h"

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QStringList>

namespace sc::translation {

// Translation and text recognition by services, with the reader's own key
// (Settings → Reading → Translate). Blocking (worker threads); they throw
// sc::Error with a message for the reader. Languages are BCP 47 codes ("ko",
// "zh", "pt-BR"); an empty `from` lets the service tell.

// DeepL (deepl.com API; free keys end in ":fx").
QStringList deepl(const QStringList &texts, const QString &from, const QString &to, const QString &key);
// Google Cloud Translation (v2, an API key).
QStringList google(const QStringList &texts, const QString &from, const QString &to, const QString &key);
// Any OpenAI-compatible chat service (OpenAI, OpenRouter, a local Ollama or
// LM Studio): `base` is its address up to /v1.
QStringList chatModel(const QStringList &texts, const QString &from, const QString &to, const QString &base, const QString &key,
                      const QString &model);

// What a model is told besides the texts, so a bubble that goes on from the
// last one (or the page before) is translated as the same scene.
struct ChatContext {
    QString work;                                // the series' or book's title
    QString chapter;
    QString memory;                              // the story so far, as the model last wrote it
    QList<QPair<QString, QString>> glossary;     // names and terms: original, rendering
    QList<QPair<QString, QString>> before;       // earlier text: original, translation ("" when only read)
    QList<QPair<QString, QString>> after;        // text that follows: original
    QByteArray image;                            // the page as JPEG, for models that can see
};
struct ChatAnswer {
    QStringList texts;
    QString summary;                             // the story so far, with these texts
    QList<QPair<QString, QString>> glossary;     // names and terms that are new
};
ChatAnswer chatModel(const QStringList &texts, const QString &from, const QString &to, const QString &base, const QString &key,
                     const QString &model, const ChatContext &context);

// Google Cloud Vision: the page's text as blocks (Vision groups lines into
// paragraphs itself), in the picture's pixels.
QList<TextBlock> googleVision(const QByteArray &image, const QString &language, const QString &key);

// The language's English name for prompts ("Korean").
QString languageForPrompt(const QString &code);

} // namespace sc::translation
