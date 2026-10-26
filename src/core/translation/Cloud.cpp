#include "Cloud.h"

#include "core/Http.h"
#include "core/Text.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

namespace sc::translation {

namespace {

QString base(const QString &code) { return code.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower(); }

QJsonObject answer(const Response &response, const QString &service) {
    const auto json = response.json().object();
    if (response.ok())
        return json;
    auto message = json.value(QStringLiteral("message")).toString();
    if (message.isEmpty())
        message = json.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
    if (response.status == 401 || response.status == 403)
        throw Error(QObject::tr("%1 refused the key. Check it in Settings → Reading → Translate.").arg(service), response.status);
    if (response.status == 456 || response.status == 429)
        throw Error(QObject::tr("%1 says the key's quota is used up for now.").arg(service), response.status);
    if (response.status == 0)
        throw Error(QObject::tr("%1 could not be reached.").arg(service));
    throw Error(message.isEmpty() ? QObject::tr("%1 answered with an error (%2).").arg(service).arg(response.status) : service + QStringLiteral(": ") + message,
                response.status);
}

} // namespace

QString languageForPrompt(const QString &code) {
    const QLocale locale(code);
    const auto name = QLocale::languageToString(locale.language());
    return name.isEmpty() || locale.language() == QLocale::C ? code : name;
}

QStringList deepl(const QStringList &texts, const QString &from, const QString &to, const QString &key) {
    if (key.trimmed().isEmpty())
        throw Error(QObject::tr("DeepL needs your key (Settings → Reading → Translate)."));
    const QUrl url(key.trimmed().endsWith(QLatin1String(":fx")) ? QStringLiteral("https://api-free.deepl.com/v2/translate")
                                                                 : QStringLiteral("https://api.deepl.com/v2/translate"));
    // DeepL wants EN-US or EN-GB, PT-BR or PT-PT; plain EN and PT are gone.
    auto target = to.toUpper().replace(QLatin1Char('_'), QLatin1Char('-'));
    if (target == QLatin1String("EN"))
        target = QStringLiteral("EN-US");
    if (target == QLatin1String("PT"))
        target = QStringLiteral("PT-BR");
    QJsonObject body{{"text", QJsonArray::fromStringList(texts)}, {"target_lang", target}};
    if (!from.isEmpty())
        body.insert(QStringLiteral("source_lang"), base(from).toUpper());
    const auto json = answer(Http::postJson(url, QJsonDocument(body).toJson(QJsonDocument::Compact),
                                            {{"Authorization", "DeepL-Auth-Key " + key.trimmed().toUtf8()}}),
                             QStringLiteral("DeepL"));
    QStringList out;
    for (const auto &t : json.value(QStringLiteral("translations")).toArray())
        out.append(t.toObject().value(QStringLiteral("text")).toString());
    if (out.size() != texts.size())
        throw Error(QObject::tr("DeepL answered with something else than the texts."));
    return out;
}

QStringList google(const QStringList &texts, const QString &from, const QString &to, const QString &key) {
    if (key.trimmed().isEmpty())
        throw Error(QObject::tr("Google Translate needs your API key (Settings → Reading → Translate)."));
    const auto url = withQuery(QUrl(QStringLiteral("https://translation.googleapis.com/language/translate/v2")), {{"key", key.trimmed()}});
    QJsonObject body{{"q", QJsonArray::fromStringList(texts)}, {"target", to}, {"format", "text"}};
    if (!from.isEmpty())
        body.insert(QStringLiteral("source"), from);
    const auto json = answer(Http::postJson(url, QJsonDocument(body).toJson(QJsonDocument::Compact)), QStringLiteral("Google Translate"));
    QStringList out;
    for (const auto &t : json.value(QStringLiteral("data")).toObject().value(QStringLiteral("translations")).toArray())
        out.append(t.toObject().value(QStringLiteral("translatedText")).toString());
    if (out.size() != texts.size())
        throw Error(QObject::tr("Google Translate answered with something else than the texts."));
    return out;
}

namespace {

// The model's answer: {"translations": [...], "summary": "...", "glossary":
// [{"term", "translation"}]}, or just the array. Models like to wrap JSON in
// a code fence or talk around it.
ChatAnswer parseChat(QString content, qsizetype expected) {
    static const QRegularExpression fence(QStringLiteral(R"(^\s*```(?:json)?\s*|\s*```\s*$)"));
    content.replace(fence, QString());
    ChatAnswer out;
    QJsonArray array;
    const auto open = content.indexOf(QLatin1Char('{'));
    const auto close = content.lastIndexOf(QLatin1Char('}'));
    if (open >= 0 && close > open && (content.indexOf(QLatin1Char('[')) < 0 || open < content.indexOf(QLatin1Char('[')))) {
        const auto object = QJsonDocument::fromJson(content.mid(open, close - open + 1).toUtf8()).object();
        array = object.value(QStringLiteral("translations")).toArray();
        out.summary = object.value(QStringLiteral("summary")).toString().trimmed();
        for (const auto &g : object.value(QStringLiteral("glossary")).toArray()) {
            const auto o = g.toObject();
            const auto term = o.value(QStringLiteral("term")).toString().trimmed();
            const auto translation = o.value(QStringLiteral("translation")).toString().trimmed();
            if (!term.isEmpty() && !translation.isEmpty())
                out.glossary.append({term, translation});
        }
    }
    if (array.isEmpty()) {
        const auto start = content.indexOf(QLatin1Char('['));
        const auto end = content.lastIndexOf(QLatin1Char(']'));
        if (start >= 0 && end > start)
            array = QJsonDocument::fromJson(content.mid(start, end - start + 1).toUtf8()).array();
    }
    for (const auto &v : array)
        out.texts.append(v.toString());
    if (out.texts.size() != expected)
        throw Error(QObject::tr("The model did not answer with one translation per text. Try another model."));
    return out;
}

QString pairs(const QList<QPair<QString, QString>> &list, bool arrow) {
    QString out;
    for (const auto &[a, b] : list)
        out += (arrow && !b.isEmpty() ? QStringLiteral("- %1  =>  %2\n").arg(a, b) : QStringLiteral("- %1\n").arg(a));
    return out;
}

} // namespace

ChatAnswer chatModel(const QStringList &texts, const QString &from, const QString &to, const QString &base, const QString &key,
                     const QString &model, const ChatContext &context) {
    if (base.trimmed().isEmpty() || model.trimmed().isEmpty())
        throw Error(QObject::tr("Set the service's address and model in Settings → Reading → Translate."));
    auto root = base.trimmed();
    while (root.endsWith(QLatin1Char('/')))
        root.chop(1);
    const QUrl url(root + QStringLiteral("/chat/completions"));
    const auto source = from.isEmpty() ? QStringLiteral("the original language") : languageForPrompt(from);
    auto prompt = QStringLiteral(
                      "You are a professional translator of comics and novels, translating from %1 into %2. ")
                      .arg(source, languageForPrompt(to));
    if (!context.work.isEmpty())
        prompt += QStringLiteral("The work is \"%1\"%2. ")
                      .arg(context.work, context.chapter.isEmpty() ? QString() : QStringLiteral(", chapter \"%1\"").arg(context.chapter));
    prompt += QStringLiteral(
        "\n\nThe user sends a JSON array of strings: the speech bubbles of one comic page, or the paragraphs of a novel, in "
        "reading order. Translate each into natural, fluent %1 as a reader of the finished work would want it.\n"
        "- The strings are one continuous scene. A sentence is often split over two or more consecutive bubbles; translate "
        "the pieces so they read as one sentence across them, not as unrelated fragments. A reply answers the bubble before "
        "it.\n"
        "- Use the context below to settle who speaks, who is addressed, pronouns, gender, politeness and tone, and to keep "
        "every name, term, nickname and way of speaking consistent with earlier text.\n"
        "- Keep sound effects short. Add no notes and nothing that is not in the text.\n\n"
        "Answer with only a JSON object: {\"translations\": [...], \"summary\": \"...\", \"glossary\": [...]}. "
        "\"translations\" has exactly one string for each string sent, in the same order. \"summary\" is the story so far in at "
        "most 120 words, updated with this text. \"glossary\" lists only the names and terms that are new or changed, each "
        "{\"term\": original, \"translation\": how you rendered it}.")
                  .arg(languageForPrompt(to));
    if (!context.memory.isEmpty())
        prompt += QStringLiteral("\n\nThe story so far:\n") + context.memory;
    if (!context.glossary.isEmpty())
        prompt += QStringLiteral("\n\nGlossary: always render these as given.\n") + pairs(context.glossary, true);
    if (!context.before.isEmpty())
        prompt += QStringLiteral("\n\nThe text just before this (original  =>  your earlier translation), for context only; do not translate it again:\n") +
                  pairs(context.before, true);
    if (!context.after.isEmpty())
        prompt += QStringLiteral("\n\nThe text that follows this (original), for context only; do not translate it:\n") + pairs(context.after, false);
    if (!context.image.isEmpty())
        prompt += QStringLiteral("\n\nThe picture of the page is attached: use it to see who speaks and what happens.");

    const auto text = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(texts)).toJson(QJsonDocument::Compact));
    QJsonValue userContent = text;
    if (!context.image.isEmpty())
        userContent = QJsonArray{QJsonObject{{"type", "image_url"},
                                             {"image_url", QJsonObject{{"url", QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(context.image.toBase64())}}}},
                                 QJsonObject{{"type", "text"}, {"text", text}}};
    const QJsonObject body{{"model", model.trimmed()},
                           {"temperature", 0.2},
                           {"messages", QJsonArray{QJsonObject{{"role", "system"}, {"content", prompt}},
                                                   QJsonObject{{"role", "user"}, {"content", userContent}}}}};
    Headers headers;
    if (!key.trimmed().isEmpty())
        headers.append({"Authorization", "Bearer " + key.trimmed().toUtf8()});
    const auto json = answer(Http::postJson(url, QJsonDocument(body).toJson(QJsonDocument::Compact), headers), QStringLiteral("The model"));
    const auto content = json.value(QStringLiteral("choices")).toArray().first().toObject().value(QStringLiteral("message")).toObject()
                             .value(QStringLiteral("content")).toString();
    return parseChat(content, texts.size());
}

QStringList chatModel(const QStringList &texts, const QString &from, const QString &to, const QString &base, const QString &key,
                      const QString &model) {
    return chatModel(texts, from, to, base, key, model, ChatContext{}).texts;
}

QList<TextBlock> googleVision(const QByteArray &image, const QString &language, const QString &key) {
    if (key.trimmed().isEmpty())
        throw Error(QObject::tr("Google Cloud Vision needs your API key (Settings → Reading → Translate)."));
    const auto url = withQuery(QUrl(QStringLiteral("https://vision.googleapis.com/v1/images:annotate")), {{"key", key.trimmed()}});
    QJsonObject request{{"image", QJsonObject{{"content", QString::fromLatin1(image.toBase64())}}},
                        {"features", QJsonArray{QJsonObject{{"type", "TEXT_DETECTION"}}}}};
    if (!language.isEmpty())
        request.insert(QStringLiteral("imageContext"), QJsonObject{{"languageHints", QJsonArray{base(language)}}});
    const auto json = answer(Http::postJson(url, QJsonDocument(QJsonObject{{"requests", QJsonArray{request}}}).toJson(QJsonDocument::Compact)),
                             QStringLiteral("Google Cloud Vision"));
    const auto first = json.value(QStringLiteral("responses")).toArray().first().toObject();
    if (first.contains(QStringLiteral("error")))
        throw Error(QStringLiteral("Google Cloud Vision: ") + first.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString());
    QList<TextLine> lines;
    const auto box = [](const QJsonObject &poly) {
        double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
        for (const auto &v : poly.value(QStringLiteral("vertices")).toArray()) {
            const auto p = v.toObject();
            const double x = p.value(QStringLiteral("x")).toDouble(), y = p.value(QStringLiteral("y")).toDouble();
            x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
        }
        return QRectF(QPointF(x0, y0), QPointF(x1, y1));
    };
    // Paragraphs, each one line of text for grouping; Vision's own blocks
    // can span several bubbles, its paragraphs rarely do.
    for (const auto &page : first.value(QStringLiteral("fullTextAnnotation")).toObject().value(QStringLiteral("pages")).toArray())
        for (const auto &block : page.toObject().value(QStringLiteral("blocks")).toArray())
            for (const auto &paragraph : block.toObject().value(QStringLiteral("paragraphs")).toArray()) {
                QString text;
                for (const auto &word : paragraph.toObject().value(QStringLiteral("words")).toArray()) {
                    for (const auto &symbol : word.toObject().value(QStringLiteral("symbols")).toArray()) {
                        const auto s = symbol.toObject();
                        text += s.value(QStringLiteral("text")).toString();
                        const auto brk = s.value(QStringLiteral("property")).toObject().value(QStringLiteral("detectedBreak")).toObject()
                                             .value(QStringLiteral("type")).toString();
                        if (brk == QLatin1String("SPACE") || brk == QLatin1String("EOL_SURE_SPACE") || brk == QLatin1String("LINE_BREAK"))
                            text += QLatin1Char(' ');
                    }
                }
                if (!text.trimmed().isEmpty())
                    lines.append({box(paragraph.toObject().value(QStringLiteral("boundingBox")).toObject()), text.simplified()});
            }
    return groupLines(lines, language);
}

} // namespace sc::translation
