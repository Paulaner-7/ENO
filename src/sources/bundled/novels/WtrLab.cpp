#include "Engines.h"

#include "core/Parallel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

namespace sc::novels {

namespace {

// /en/novel/<raw id>/<slug>[/chapter-<n>]
const QRegularExpression &novelPath() {
    static const QRegularExpression re(QStringLiteral(R"(^(?:/[a-z]{2}(?:-[A-Za-z]+)?)?/novel/(\d+)/([^/]+)(?:/chapter-(\d+))?)"));
    return re;
}

// Titles and blurbs mark names as "%{Shown|base64 of the original}".
QString plain(QString text) {
    static const QRegularExpression marked(QStringLiteral(R"(%\{([^|}]*)\|[^}]*\})"));
    return text.replace(marked, QStringLiteral("\\1")).trimmed();
}

// Chapter text names glossary terms by number ("※3⛬"); the reader shows
// the term itself.
QString withTerms(QString text, const QJsonArray &terms) {
    static const QRegularExpression marker(QStringLiteral("※(\\d+)⛬"));
    if (!text.contains(QChar(0x203B)))
        return text;
    QString out;
    qsizetype last = 0;
    for (auto it = marker.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        out += QStringView(text).mid(last, m.capturedStart() - last);
        const auto term = terms.at(m.captured(1).toInt()).toArray();
        out += term.isEmpty() ? QString() : term.first().toString();
        last = m.capturedEnd();
    }
    out += QStringView(text).mid(last);
    return out;
}

// The page's own data (Next.js), without parsing the HTML around it.
QJsonObject pageProps(const QByteArray &html) {
    const auto at = html.indexOf("id=\"__NEXT_DATA__\"");
    const auto start = at < 0 ? -1 : html.indexOf('>', at);
    const auto end = start < 0 ? -1 : html.indexOf("</script>", start);
    if (end < 0)
        throw Error(QStringLiteral("WTR-LAB returned a page SCANCORE could not read."), 502);
    return QJsonDocument::fromJson(html.mid(start + 1, end - start - 1))
        .object()
        .value(QStringLiteral("props"))
        .toObject()
        .value(QStringLiteral("pageProps"))
        .toObject();
}

} // namespace

QString WtrLab::novelUrl(qint64 rawId, const QString &slug) const {
    return QStringLiteral("%1/en/novel/%2/%3").arg(m_config.base).arg(rawId).arg(slug);
}

Book WtrLab::fromSerie(const QJsonObject &serie) const {
    const auto data = serie.value(QStringLiteral("data")).toObject();
    Book book;
    book.source = key();
    book.url = novelUrl(serie.value(QStringLiteral("raw_id")).toInteger(serie.value(QStringLiteral("id")).toInteger()),
                        serie.value(QStringLiteral("slug")).toString());
    book.title = plain(data.value(QStringLiteral("title")).toString());
    book.author = plain(data.value(QStringLiteral("author")).toString());
    book.cover = data.value(QStringLiteral("image")).toString();
    book.description = plain(data.value(QStringLiteral("description")).toString());
    book.status = serie.value(QStringLiteral("status")).toInt() == 1 ? QStringLiteral("Completed") : QStringLiteral("Ongoing");
    if (const auto rating = serie.value(QStringLiteral("rating")); rating.isDouble())
        book.rating = QString::number(rating.toDouble(), 'f', 1);
    book.chapterCount = serie.value(QStringLiteral("chapter_count")).toInt();
    return book;
}

QList<Book> WtrLab::search(const QString &query) {
    const auto response = Http::postJson(QUrl(m_config.base + QStringLiteral("/api/search")),
                                         QJsonDocument(QJsonObject{{"text", query}}).toJson(QJsonDocument::Compact),
                                         {{"Referer", origin().toUtf8()}, {"Origin", m_config.base.toUtf8()}});
    if (!response.ok())
        throw Error(QStringLiteral("WTR-LAB responded with HTTP %1.").arg(response.status), response.status);
    QList<Book> books;
    for (const auto &item : response.json().object().value(QStringLiteral("data")).toArray())
        if (auto book = fromSerie(item.toObject()); !book.title.isEmpty())
            books.append(book);
    return books;
}

QList<Book> WtrLab::listing(const QString &order, int page) {
    const auto props = pageProps(
        this->page(QStringLiteral("%1/en/novel-list?orderBy=%2&order=desc&page=%3").arg(m_config.base, order).arg(page)).body);
    QList<Book> books;
    for (const auto &item : props.value(QStringLiteral("series")).toArray())
        if (auto book = fromSerie(item.toObject()); !book.title.isEmpty())
            books.append(book);
    return books;
}

QList<Shelf> WtrLab::discover() {
    // The list shows ten a page; two pages per shelf, all at once.
    const QList<QPair<QString, QString>> feeds{{QStringLiteral("view"), QStringLiteral("Popular")},
                                               {QStringLiteral("update"), QStringLiteral("Recently updated")},
                                               {QStringLiteral("date"), QStringLiteral("New arrivals")}};
    QList<std::function<QList<Book>()>> jobs;
    for (const auto &feed : feeds)
        for (int page = 1; page <= 2; ++page)
            jobs.append([this, order = feed.first, page] { return listing(order, page); });
    const auto results = parallel(jobs);
    QList<Shelf> shelves;
    QString firstError;
    for (int i = 0; i < feeds.size(); ++i) {
        Shelf shelf{feeds[i].second, {}};
        for (int page = 0; page < 2; ++page) {
            const auto &result = results[i * 2 + page];
            if (result.ok())
                shelf.books += *result.value;
            else if (firstError.isEmpty())
                firstError = result.error;
        }
        if (!shelf.books.isEmpty())
            shelves.append(shelf);
    }
    if (shelves.isEmpty())
        throw Error(firstError.isEmpty() ? QStringLiteral("WTR-LAB returned no novels.") : firstError);
    return shelves;
}

Book WtrLab::book(const QString &url, QList<ChapterRef> &chapters) {
    const auto m = novelPath().match(QUrl(url).path());
    if (!m.hasMatch())
        throw Error(QStringLiteral("That is not a WTR-LAB novel link."));
    const auto rawId = m.captured(1).toLongLong();
    const auto canonical = novelUrl(rawId, m.captured(2));
    // The page and the chapter list at once.
    const auto results = parallel(QList<std::function<QByteArray()>>{
        [&] { return page(canonical).body; },
        [&] { return page(QStringLiteral("%1/api/chapters/%2").arg(m_config.base).arg(rawId)).body; },
    });
    for (const auto &result : results)
        if (!result.ok())
            throw Error(result.error, 502);

    const auto serie = pageProps(*results[0].value).value(QStringLiteral("serie")).toObject();
    auto data = serie.value(QStringLiteral("serie_data")).toObject();
    if (data.isEmpty())
        throw Error(QStringLiteral("WTR-LAB returned a page SCANCORE could not read."), 502);
    data.insert(QStringLiteral("raw_id"), rawId);
    auto book = fromSerie(data);
    book.url = canonical;

    const auto list = QJsonDocument::fromJson(*results[1].value).object().value(QStringLiteral("chapters")).toArray();
    chapters.reserve(list.size());
    for (const auto &value : list) {
        const auto c = value.toObject();
        const auto order = c.value(QStringLiteral("order")).toInt();
        chapters.append({QStringLiteral("%1/chapter-%2").arg(canonical).arg(order), plain(c.value(QStringLiteral("title")).toString()),
                         QString::number(order)});
    }
    book.chapterCount = int(chapters.size());
    return book;
}

QList<Block> WtrLab::chapter(const QString &url, QString &title) {
    const auto m = novelPath().match(QUrl(url).path());
    if (!m.hasMatch() || m.captured(3).isEmpty())
        throw Error(QStringLiteral("That is not a WTR-LAB chapter link."));
    const Headers headers{{"Referer", url.toUtf8()}, {"Origin", m_config.base.toUtf8()}};
    // The site's own AI translation, which it serves as plain text.
    const QJsonObject request{{"translate", "ai"},      {"language", "en"}, {"raw_id", m.captured(1).toLongLong()},
                              {"chapter_no", m.captured(3).toInt()}, {"retry", false}, {"force_retry", false}};
    const auto response =
        Http::postJson(QUrl(m_config.base + QStringLiteral("/api/reader/get")), QJsonDocument(request).toJson(QJsonDocument::Compact), headers);
    if (response.isChallenge())
        throw Error(QStringLiteral("WTR-LAB asked for a browser check. Open it once in SCANCORE to pass it."), 403, url);
    const auto reply = response.json().object();
    if (!reply.value(QStringLiteral("success")).toBool()) {
        if (reply.value(QStringLiteral("requireTurnstile")).toBool())
            throw Error(QStringLiteral("WTR-LAB asked for a check before this chapter. Open it once in SCANCORE to pass it."), 403, url);
        if (reply.value(QStringLiteral("code")).toString() == QLatin1String("CHAPTER_LOCKED"))
            throw Error(QStringLiteral("This chapter is locked on WTR-LAB."), 403);
        const auto why = reply.value(QStringLiteral("error")).toString(reply.value(QStringLiteral("code")).toString());
        throw Error(why.isEmpty() ? QStringLiteral("WTR-LAB responded with HTTP %1.").arg(response.status) : QStringLiteral("WTR-LAB: %1").arg(why),
                    response.status >= 400 ? response.status : 502);
    }
    title = plain(reply.value(QStringLiteral("chapter")).toObject().value(QStringLiteral("title")).toString());

    auto content = reply.value(QStringLiteral("data")).toObject();
    if (content.isEmpty()) {
        const auto path = reply.value(QStringLiteral("content_url")).toString();
        if (!path.startsWith(QLatin1String("/api/")))
            throw Error(QStringLiteral("WTR-LAB is still translating this chapter. Try again in a minute."), 503);
        const auto body = Http::getJson(QUrl(m_config.base + path), headers).object();
        if (!body.value(QStringLiteral("success")).toBool(true))
            throw Error(QStringLiteral("WTR-LAB could not load this chapter."), 502);
        content = body.value(QStringLiteral("data")).toObject();
    }
    const auto inner = content.value(QStringLiteral("data")).toObject();
    const auto body = inner.value(QStringLiteral("body"));
    if (!body.isArray())
        throw Error(body.isString() ? QStringLiteral("WTR-LAB sent this chapter in a form SCANCORE cannot read.")
                                    : QStringLiteral("WTR-LAB is still translating this chapter. Try again in a minute."),
                    body.isString() ? 502 : 503);
    const auto terms = inner.value(QStringLiteral("glossary_data")).toObject().value(QStringLiteral("terms")).toArray();
    QList<Block> blocks;
    const auto paragraphs = body.toArray();
    blocks.reserve(paragraphs.size());
    for (const auto &value : paragraphs) {
        const auto text = withTerms(value.toString(), terms).trimmed();
        if (text.isEmpty())
            continue;
        blocks.append({QStringLiteral("paragraph"), text, text.toHtmlEscaped()});
    }
    if (blocks.isEmpty())
        throw Error(QStringLiteral("WTR-LAB returned no readable text for this chapter."), 502);
    return blocks;
}

} // namespace sc::novels
