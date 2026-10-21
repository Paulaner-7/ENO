// Comic sources read from web pages: Webtoon, Flame Comics, the WordPress
// manga themes (MangaReader, Madara and MangaVerse), InManga and LeerCapitulo.

#include "Engines.h"

#include "core/Html.h"
#include "core/Parallel.h"

#include <QDateTime>
#include <QTimeZone>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace sc::comics {

namespace {

QUrl queryUrl(const QString &base, const QList<QPair<QString, QString>> &params) {
    QUrl url(base);
    QUrlQuery query(url);
    for (const auto &[key, value] : params)
        query.addQueryItem(QString::fromUtf8(QUrl::toPercentEncoding(key)),
                           QString::fromUtf8(QUrl::toPercentEncoding(value)));
    url.setQuery(query);
    return url;
}

QString metaContent(const HtmlDocument &doc, const QString &property) {
    auto node = doc.first(QStringLiteral("meta[property=\"%1\"]").arg(property));
    if (!node)
        node = doc.first(QStringLiteral("meta[name=\"%1\"]").arg(property));
    return node.attr("content").trimmed();
}

QString formatViews(qint64 count) {
    if (count >= 1000000000)
        return QStringLiteral("%1B views").arg(double(count) / 1e9, 0, 'f', 1);
    if (count >= 1000000)
        return QStringLiteral("%1M views").arg(double(count) / 1e6, 0, 'f', 1);
    if (count >= 1000)
        return QStringLiteral("%1K views").arg(qRound64(double(count) / 1e3));
    return count ? QStringLiteral("%1 views").arg(count) : QString();
}

} // namespace

// ---------------------------------------------------------------- Webtoon

namespace {
const QString kThumbCdn = QStringLiteral("https://webtoon-phinf.pstatic.net");
const QString kWebtoonsBase = QStringLiteral("https://www.webtoons.com");
const QString kWebtoonsMobile = QStringLiteral("https://m.webtoons.com");

QString webtoonEpisodeSlug(const QString &url) {
    static const QRegularExpression re(QStringLiteral(R"(webtoons\.com/en/[^/]+/[^/]+/([^/]+)/viewer)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(url);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString authors(const QJsonObject &t) {
    QStringList list{t.value("writingAuthorName").toString(), t.value("pictureAuthorName").toString()};
    list.removeAll(QString());
    list.removeDuplicates();
    return list.join(QStringLiteral(" / "));
}
} // namespace

QList<Summary> WebtoonSource::search(const QString &query) {
    const auto doc = Http::getJson(queryUrl(kWebtoonsMobile + QStringLiteral("/undefined/search/result"),
                                            {{"keyword", query}, {"searchType", "ALL"}, {"start", "1"}}),
                                   {{"Origin", kWebtoonsBase.toUtf8()}, {"Referer", "https://www.webtoons.com/en/"}});
    const auto root = doc.object();
    if (!root.value("success").toBool())
        throw Error(QStringLiteral("Webtoon search failed."));
    const auto result = root.value("result").toObject();
    QList<Summary> out;
    for (const auto &row : result.value("webtoonResult").toObject().value("titleList").toArray()) {
        const auto t = row.toObject();
        const auto group = t.value("titleGroupName").toString();
        if (group.isEmpty())
            continue;
        const auto genre = t.value("representGenre").toString().toLower();
        Summary item;
        item.title = t.value("title").toString();
        item.url = QStringLiteral("%1/en/%2/%3/list?title_no=%4").arg(kWebtoonsBase, genre, group, str(t.value("titleNo")));
        item.type = QStringLiteral("Webtoon");
        item.author = authors(t);
        item.views = formatViews(qint64(t.value("readCount").toDouble()));
        const auto thumb = t.value("thumbnailMobile").toString();
        item.cover = thumb.isEmpty() ? QString() : kThumbCdn + thumb;
        item.genres = genre;
        item.source = key();
        out.append(item);
    }
    for (const auto &row : result.value("challengeResult").toObject().value("titleList").toArray()) {
        const auto t = row.toObject();
        const auto group = t.value("titleGroupName").toString();
        if (group.isEmpty())
            continue;
        Summary item;
        item.title = t.value("title").toString();
        item.url = QStringLiteral("%1/en/canvas/%2/list?title_no=%3").arg(kWebtoonsBase, group, str(t.value("titleNo")));
        item.type = QStringLiteral("Canvas");
        item.author = authors(t);
        item.views = formatViews(qint64(t.value("readCount").toDouble()));
        const auto thumb = t.value("thumbnailMobile").toString();
        item.cover = thumb.isEmpty() ? QString() : kThumbCdn + thumb;
        item.genres = QStringLiteral("canvas");
        item.source = key();
        out.append(item);
    }
    return out;
}

QList<Chapter> WebtoonSource::chapters(const QString &seriesUrl) {
    const QUrl url(seriesUrl);
    QUrlQuery query(url);
    auto titleNo = query.queryItemValue(QStringLiteral("title_no"));
    if (titleNo.isEmpty())
        titleNo = query.queryItemValue(QStringLiteral("titleNo"));
    if (titleNo.isEmpty())
        throw Error(QStringLiteral("That is not a Webtoon series link."));
    const auto type = pathSegment(url, 1) == QLatin1String("canvas") ? QStringLiteral("canvas") : QStringLiteral("webtoon");

    QList<Chapter> out;
    try {
        const auto doc = Http::getJson(
            queryUrl(QStringLiteral("%1/api/v1/%2/%3/episodes").arg(kWebtoonsMobile, type, titleNo), {{"pageSize", "99999"}}),
            {{"Referer", "https://m.webtoons.com/"}, {"Cookie", "ageGatePass=true; needGDPR=false; locale=en"}}, 20000);
        for (const auto &row : doc.object().value("result").toObject().value("episodeList").toArray()) {
            const auto e = row.toObject();
            const auto title = e.value("episodeTitle").toString().trimmed();
            auto link = e.value("viewerLink").toString();
            if (title.isEmpty() || link.isEmpty())
                continue;
            if (!link.startsWith(QLatin1String("http")))
                link = kWebtoonsBase + (link.startsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + link;
            Chapter ch;
            ch.title = title;
            ch.number = str(e.value("episodeNo"));
            if (ch.number.isEmpty())
                ch.number = chapterNumberFrom(title);
            ch.url = link;
            ch.slug = webtoonEpisodeSlug(link);
            ch.thumbnail = e.value("thumbnail").toString();
            if (!ch.thumbnail.isEmpty() && ch.thumbnail.startsWith(QLatin1Char('/')))
                ch.thumbnail = kThumbCdn + ch.thumbnail;
            const auto date = e.value("exposureDateMillis").toDouble();
            if (date > 0)
                ch.released = QDateTime::fromMSecsSinceEpoch(qint64(date), QTimeZone::UTC).toString(Qt::ISODate);
            else
                ch.released = str(e.value("serviceDate"));
            out.append(ch);
        }
    } catch (const Error &) {
        // Fall back to the list page below.
    }
    if (!out.isEmpty()) {
        sortChapters(out);
        return out;
    }

    const auto page = Http::getOk(url, htmlHeaders(), 20000);
    HtmlDocument doc(page.body);
    for (const auto &li : doc.select(QStringLiteral("#_listUl li._episodeItem"))) {
        const auto a = li.first(QStringLiteral("a"));
        const auto href = resolveUrl(kWebtoonsBase, a.attr("href"));
        auto number = li.attr("data-episode-no");
        if (href.isEmpty() || number.isEmpty())
            continue;
        Chapter ch;
        ch.number = number;
        ch.url = href;
        ch.slug = webtoonEpisodeSlug(href);
        const auto img = li.first(QStringLiteral("img"));
        ch.thumbnail = img.attr("data-url").isEmpty() ? img.attr("src") : img.attr("data-url");
        ch.title = li.first(QStringLiteral(".subj span, .subj, .tit")).textTrim();
        if (ch.title.isEmpty())
            ch.title = QStringLiteral("Episode %1").arg(number);
        out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Panel> WebtoonSource::panels(const QString &chapterUrl) {
    const auto page = Http::getOk(QUrl(chapterUrl), htmlHeaders(), 20000);
    HtmlDocument doc(page.body);
    QList<Panel> out;
    for (const auto &img : doc.select(QStringLiteral("#_imageList img._images"))) {
        auto url = img.attr("data-url");
        if (url.isEmpty())
            url = img.attr("src");
        if (url.isEmpty() || url.contains(QLatin1String("bg_transparency")))
            continue;
        out.append({url.trimmed(), int(img.attr("width").toDouble()), int(img.attr("height").toDouble())});
    }
    return out;
}

std::optional<Details> WebtoonSource::details(const QString &seriesUrl) {
    const auto page = Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000);
    HtmlDocument doc(page.body);
    Details d;
    d.title = doc.first(QStringLiteral(".detail_header .subj, h1.subj")).textTrim();
    if (d.title.isEmpty())
        d.title = metaContent(doc, QStringLiteral("og:title"));
    d.description = doc.first(QStringLiteral("p.summary")).textTrim();
    if (d.description.isEmpty())
        d.description = metaContent(doc, QStringLiteral("og:description"));
    d.cover = metaContent(doc, QStringLiteral("og:image"));
    d.author = doc.first(QStringLiteral(".detail_header .author_area, .author_area")).textTrim();
    d.author.remove(QStringLiteral("author info"), Qt::CaseInsensitive);
    d.author = d.author.trimmed();
    if (const auto genre = doc.first(QStringLiteral(".detail_header .genre, h2.genre")).textTrim(); !genre.isEmpty())
        d.genres.append(genre);
    d.status = doc.first(QStringLiteral(".day_info")).textTrim();
    return d;
}

// ---------------------------------------------------------------- Flame

namespace {
QJsonObject flamePageProps(const QByteArray &html) {
    HtmlDocument doc(html);
    const auto raw = doc.first(QStringLiteral("#__NEXT_DATA__")).text();
    if (raw.isEmpty())
        throw Error(QStringLiteral("Flame Comics changed its page layout."));
    const auto json = QJsonDocument::fromJson(raw.toUtf8());
    return json.object().value("props").toObject().value("pageProps").toObject();
}

QMutex g_flameMutex;
QList<Summary> g_flameCatalog;
qint64 g_flameFetchedAt = 0;
} // namespace

QList<Summary> FlameSource::search(const QString &query) {
    QList<Summary> catalog;
    {
        QMutexLocker lock(&g_flameMutex);
        if (QDateTime::currentSecsSinceEpoch() - g_flameFetchedAt < 300)
            catalog = g_flameCatalog;
    }
    if (catalog.isEmpty()) {
        const auto props = flamePageProps(Http::getOk(QUrl(m_config.base + QStringLiteral("/browse")), htmlHeaders()).body);
        const auto cdn = m_config.options.value("cdn").toString(QStringLiteral("https://cdn.flamecomics.xyz"));
        for (const auto &row : props.value("series").toArray()) {
            const auto s = row.toObject();
            const auto id = str(s.value("series_id"));
            Summary item;
            item.title = str(s.value("title"));
            item.url = QStringLiteral("%1/series/%2").arg(m_config.base, id);
            item.type = str(s.value("type"));
            if (item.type.isEmpty())
                item.type = QStringLiteral("Manhwa");
            QStringList authorList;
            for (const auto &a : s.value("author").toArray())
                authorList.append(str(a));
            item.author = authorList.join(QStringLiteral(", "));
            item.cover = QStringLiteral("%1/uploads/images/series/%2/%3")
                             .arg(cdn, id, QString::fromUtf8(QUrl::toPercentEncoding(str(s.value("cover")))));
            QStringList tags;
            for (const auto &t : s.value("tags").toArray())
                tags.append(str(t));
            item.genres = tags.join(QStringLiteral(", "));
            item.source = key();
            catalog.append(item);
        }
        if (catalog.isEmpty())
            throw Error(QStringLiteral("Flame Comics returned an empty catalog."));
        QMutexLocker lock(&g_flameMutex);
        g_flameCatalog = catalog;
        g_flameFetchedAt = QDateTime::currentSecsSinceEpoch();
    }
    const auto terms = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QList<Summary> out;
    for (const auto &item : std::as_const(catalog)) {
        const auto title = item.title.toLower();
        if (std::all_of(terms.begin(), terms.end(), [&](const QString &t) { return title.contains(t); }))
            out.append(item);
        if (out.size() >= 40)
            break;
    }
    return out;
}

QList<Chapter> FlameSource::chapters(const QString &seriesUrl) {
    const QUrl url(seriesUrl);
    const auto id = pathSegment(url, 1);
    if (pathSegment(url, 0) != QLatin1String("series") || id.isEmpty())
        throw Error(QStringLiteral("That is not a Flame Comics series link."));
    const auto props =
        flamePageProps(Http::getOk(QUrl(QStringLiteral("%1/series/%2").arg(m_config.base, id)), htmlHeaders()).body);
    QList<Chapter> out;
    for (const auto &row : props.value("chapters").toArray()) {
        const auto c = row.toObject();
        const auto token = str(c.value("token"));
        bool numeric = false;
        const auto number = str(c.value("chapter")).toDouble(&numeric);
        // `notice` marks announcement posts; it is 0 or empty for chapters.
        if (token.isEmpty() || c.value("notice").toVariant().toBool() || !numeric)
            continue;
        Chapter ch;
        ch.number = QString::number(number);
        ch.title = str(c.value("title"));
        if (ch.title.isEmpty())
            ch.title = chapterLabel(ch.number);
        ch.url = QStringLiteral("%1/series/%2/%3").arg(m_config.base, str(c.value("series_id")), token);
        ch.slug = token;
        const auto released = c.value("release_date").toDouble();
        if (released > 0)
            ch.released = QDateTime::fromSecsSinceEpoch(qint64(released), QTimeZone::UTC).toString(Qt::ISODate);
        out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Panel> FlameSource::panels(const QString &chapterUrl) {
    const QUrl url(chapterUrl);
    const auto html = Http::getOk(QUrl(m_config.base + url.path()), htmlHeaders()).body;
    HtmlDocument doc(html);
    QStringList rendered;
    for (const auto &img : doc.select(QStringLiteral("img[src]")))
        rendered.append(img.attr("src"));
    const auto props = flamePageProps(html);
    const auto chapter = props.value("chapter").toObject();
    const auto images = chapter.value("images").toObject();
    QList<std::pair<int, QJsonObject>> ordered;
    for (auto it = images.begin(); it != images.end(); ++it)
        ordered.append({it.key().toInt(), it.value().toObject()});
    std::sort(ordered.begin(), ordered.end(), [](auto &a, auto &b) { return a.first < b.first; });
    const auto cdn = m_config.options.value("cdn").toString(QStringLiteral("https://cdn.flamecomics.xyz"));
    QList<Panel> out;
    for (const auto &[index, image] : ordered) {
        const auto prefix = QStringLiteral("%1/uploads/images/series/%2/%3/%4")
                                .arg(cdn, str(chapter.value("series_id")), str(props.value("token")),
                                     str(image.value("name")));
        // Only images the page itself renders, so hidden or locked pages are
        // never requested.
        auto match = std::find_if(rendered.begin(), rendered.end(), [&](const QString &candidate) {
            return candidate == prefix || candidate.startsWith(prefix + QLatin1Char('?'));
        });
        if (match != rendered.end())
            out.append({*match, image.value("width").toInt(), image.value("height").toInt()});
    }
    return out;
}

std::optional<Details> FlameSource::details(const QString &seriesUrl) {
    const QUrl url(seriesUrl);
    const auto props =
        flamePageProps(Http::getOk(QUrl(QStringLiteral("%1/series/%2").arg(m_config.base, pathSegment(url, 1))),
                                   htmlHeaders())
                           .body);
    const auto s = props.value("series").toObject();
    if (s.isEmpty())
        return std::nullopt;
    Details d;
    d.title = str(s.value("title"));
    auto description = str(s.value("description"));
    d.description = description.contains(QLatin1Char('<')) ? HtmlDocument(description).root().textTrim()
                                                            : description.trimmed();
    d.status = str(s.value("status"));
    for (const auto &t : s.value("tags").toArray())
        d.genres.append(str(t));
    QStringList authorList;
    for (const auto &a : s.value("author").toArray())
        authorList.append(str(a));
    d.author = authorList.join(QStringLiteral(", "));
    const auto cdn = m_config.options.value("cdn").toString(QStringLiteral("https://cdn.flamecomics.xyz"));
    if (!str(s.value("cover")).isEmpty())
        d.cover = QStringLiteral("%1/uploads/images/series/%2/%3")
                      .arg(cdn, str(s.value("series_id")), QString::fromUtf8(QUrl::toPercentEncoding(str(s.value("cover")))));
    return d;
}

// ---------------------------------------------------------------- WordPress

QList<Summary> WpSource::parseSearch(const QByteArray &html) const {
    HtmlDocument doc(html);
    const QUrl base(m_config.base);
    QSet<QString> seen;
    QList<Summary> out;
    static const QRegularExpression seriesPath(QStringLiteral(R"(^/(manga|comics|series)/[^/]+/?$)"));
    static const QRegularExpression excluded(QStringLiteral(R"(/(manga|comics|series)/(chapter|volume|tag|genre|feed)s?/)"),
                                             QRegularExpression::CaseInsensitiveOption);
    const QString anchorSelector =
        QStringLiteral("a[href*=\"/manga/\"], a[href*=\"/comics/\"], a[href*=\"/series/\"]");

    auto push = [&](const HtmlNode &node) {
        const auto anchor = node.matches(anchorSelector) ? node : node.first(anchorSelector);
        if (!anchor)
            return;
        const QUrl url = base.resolved(QUrl(anchor.attr("href").trimmed()));
        if (url.host() != base.host() || !seriesPath.match(url.path()).hasMatch())
            return;
        const auto clean = QStringLiteral("%1://%2%3").arg(url.scheme(), url.host(), url.path());
        if (seen.contains(clean) || excluded.match(clean).hasMatch())
            return;
        auto title = node.first(QStringLiteral(".post-title a, .tt, .h4 a, h2 a, h3 a, h4 a, h5 a")).textTrim();
        if (title.isEmpty())
            title = anchor.attr("title").trimmed();
        if (title.isEmpty())
            title = anchor.textTrim();
        if (title.isEmpty())
            return;
        const auto img = node.first(QStringLiteral("img"));
        auto thumb = img.attr("data-src");
        if (thumb.isEmpty())
            thumb = img.attr("data-lazy-src");
        if (thumb.isEmpty())
            thumb = img.attr("src");
        seen.insert(clean);
        Summary item;
        item.title = title;
        item.url = clean;
        item.cover = resolveUrl(m_config.base, thumb);
        item.type = node.first(QStringLiteral(".type")).textTrim();
        item.source = key();
        out.append(item);
    };

    for (const auto &node : doc.select(QStringLiteral(
             ".c-tabs-item__content, .page-item-detail, .row.c-tabs-item, .bsx, .listupd .bs, article")))
        push(node);
    if (out.isEmpty())
        for (const auto &node : doc.select(anchorSelector))
            push(node);
    return out;
}

QList<Summary> WpSource::searchHtml(const QString &query) {
    const auto url = queryUrl(m_config.base + QStringLiteral("/"), {{"s", query}, {"post_type", "wp-manga"}});
    return parseSearch(Http::getOk(url, htmlHeaders(), 20000).body);
}

QList<Summary> WpSource::search(const QString &query) {
    if (m_config.options.value("htmlSearchOnly").toBool(true))
        return searchHtml(query);
    try {
        const auto doc = Http::getJson(
            queryUrl(m_config.base + QStringLiteral("/wp-json/wp/v2/search"), {{"search", query}, {"per_page", "12"}}));
        QList<Summary> out;
        for (const auto &row : doc.array()) {
            const auto item = row.toObject();
            const auto subtype = str(item.value("subtype"));
            if (subtype != QLatin1String("manga") && subtype != QLatin1String("wp-manga"))
                continue;
            Summary s;
            s.title = decodeEntities(str(item.value("title")));
            s.url = str(item.value("url"));
            s.source = key();
            out.append(s);
        }
        if (!out.isEmpty())
            return out;
    } catch (const Error &) {
    }
    return searchHtml(query);
}

QList<Chapter> WpSource::parseChapterList(const QByteArray &html, const QString &pageUrl) const {
    HtmlDocument doc(html);
    QList<Chapter> out;
    QSet<QString> seen;
    for (const auto &a : doc.select(QStringLiteral(
             "#chapterlist li a, li.wp-manga-chapter a, .listing-chapters_wrap a, .eplister li a"))) {
        const auto url = resolveUrl(pageUrl, a.attr("href"));
        if (url.isEmpty() || seen.contains(url))
            continue;
        auto title = a.first(QStringLiteral(".chapternum")).textTrim();
        if (title.isEmpty())
            title = a.textTrim();
        if (title.isEmpty())
            continue;
        seen.insert(url);
        Chapter ch;
        ch.title = title;
        ch.number = chapterNumberFrom(title);
        ch.url = url;
        ch.slug = pathSegment(QUrl(url), -1);
        const auto li = a.closest(QStringLiteral("li"));
        ch.released = li.first(QStringLiteral(".chapterdate, .chapter-release-date, .chapter-date")).textTrim();
        out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Chapter> WpSource::chapters(const QString &seriesUrl) {
    auto list = parseChapterList(Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body, seriesUrl);
    if (!list.isEmpty())
        return list;
    // Some sites load the list lazily; the REST search index still has posts.
    const auto slug = pathSegment(QUrl(seriesUrl), -1);
    const auto doc = Http::getJson(queryUrl(m_config.base + QStringLiteral("/wp-json/wp/v2/search"),
                                            {{"search", QString(slug).replace(QLatin1Char('-'), QLatin1Char(' '))},
                                             {"per_page", "100"},
                                             {"type", "post"}}));
    static const QRegularExpression marker(QStringLiteral(R"(/(chapter|chap|ch|episode|ep)([-_/]|\d))"),
                                           QRegularExpression::CaseInsensitiveOption);
    for (const auto &row : doc.array()) {
        const auto item = row.toObject();
        const auto url = str(item.value("url")).trimmed();
        const auto path = QUrl(url).path().toLower();
        if (url.isEmpty() || !path.contains(slug.toLower()) || !marker.match(path).hasMatch())
            continue;
        Chapter ch;
        ch.title = decodeEntities(str(item.value("title")));
        ch.number = chapterNumberFrom(ch.title);
        ch.url = url;
        ch.slug = pathSegment(QUrl(url), -1);
        list.append(ch);
    }
    sortChapters(list);
    return list;
}

QList<Panel> WpSource::panels(const QString &chapterUrl) {
    const auto html = Http::getOk(QUrl(chapterUrl), htmlHeaders(), 20000).body;
    HtmlDocument doc(html);
    QList<Panel> out;
    QSet<QString> seen;
    auto add = [&](const QString &raw, int width = 0, int height = 0) {
        const auto url = resolveUrl(chapterUrl, raw);
        if (url.isEmpty() || !url.startsWith(QLatin1String("https:")) || url.endsWith(QLatin1String(".svg")) ||
            seen.contains(url))
            return;
        seen.insert(url);
        out.append({url, width, height});
    };

    // MangaReader themes embed the page list as ts_reader.run({...}).
    static const QRegularExpression reader(QStringLiteral(R"(ts_reader\.run\(\s*(\{[\s\S]*?\})\s*\);?)"));
    for (const auto &script : doc.select(QStringLiteral("script"))) {
        const auto m = reader.match(script.text());
        if (!m.hasMatch())
            continue;
        const auto config = QJsonDocument::fromJson(m.captured(1).toUtf8()).object();
        for (const auto &source : config.value("sources").toArray()) {
            const auto images = source.toObject().value("images").toArray();
            if (images.isEmpty())
                continue;
            for (const auto &image : images)
                add(str(image));
            break;
        }
    }
    if (!out.isEmpty())
        return out;

    auto images = doc.select(QStringLiteral("#readerarea img, .reading-content img"));
    if (images.isEmpty())
        images = doc.select(QStringLiteral(".entry-content img"));
    for (const auto &img : images) {
        const auto srcset = img.attr("data-srcset").isEmpty() ? img.attr("srcset") : img.attr("data-srcset");
        const QStringList candidates{img.attr("data-src"), img.attr("data-lazy-src"), img.attr("data-original"),
                                     srcset.split(QLatin1Char(',')).value(0).trimmed().split(QLatin1Char(' ')).value(0),
                                     img.attr("src")};
        for (const auto &candidate : candidates) {
            const auto value = candidate.trimmed();
            if (!value.isEmpty() && !value.startsWith(QLatin1String("data:"))) {
                add(value, img.attr("width").toInt(), img.attr("height").toInt());
                break;
            }
        }
    }
    return out;
}

std::optional<Details> WpSource::details(const QString &seriesUrl) {
    const auto html = Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body;
    HtmlDocument doc(html);
    Details d;
    d.title = doc.first(QStringLiteral("h1.entry-title, .post-title h1, h1")).textTrim();
    d.cover = metaContent(doc, QStringLiteral("og:image"));
    if (const auto thumb = doc.first(QStringLiteral(".thumb img, .summary_image img")); thumb) {
        auto src = thumb.attr("data-src");
        if (src.isEmpty())
            src = thumb.attr("src");
        if (const auto resolved = resolveUrl(seriesUrl, src); !resolved.isEmpty())
            d.cover = resolved;
    }
    auto description = doc.first(QStringLiteral(
        ".entry-content[itemprop=\"description\"], .summary__content, .description-summary, .synp .entry-content"));
    if (description) {
        QStringList paragraphs;
        for (const auto &p : description.select(QStringLiteral("p")))
            if (auto text = p.textTrim(); !text.isEmpty())
                paragraphs.append(text);
        d.description = paragraphs.isEmpty() ? description.textTrim() : paragraphs.join(QStringLiteral("\n\n"));
    }
    if (d.description.isEmpty())
        d.description = metaContent(doc, QStringLiteral("og:description"));
    for (const auto &g : doc.select(QStringLiteral(".mgen a, .genres-content a, .wd-full .mgen a")))
        if (auto text = g.textTrim(); !text.isEmpty() && !d.genres.contains(text))
            d.genres.append(text);
    for (const auto &row : doc.select(QStringLiteral(".imptdt, .post-content_item, .tsinfo .imptdt"))) {
        const auto text = row.textTrim();
        if (text.startsWith(QLatin1String("Status"), Qt::CaseInsensitive))
            d.status = text.mid(6).trimmed();
        else if (text.startsWith(QLatin1String("Author"), Qt::CaseInsensitive))
            d.author = text.section(QLatin1Char(' '), 1).trimmed();
    }
    return d;
}

// ---------------------------------------------------------------- Madara

QList<Summary> MadaraSource::search(const QString &query) {
    const QList<QPair<QString, QString>> form{{"action", "madara_load_more"},
                                              {"page", "0"},
                                              {"template", "madara-core/content/content-search"},
                                              {"vars[paged]", "1"},
                                              {"vars[template]", "archive"},
                                              {"vars[sidebar]", "right"},
                                              {"vars[post_type]", "wp-manga"},
                                              {"vars[post_status]", "publish"},
                                              {"vars[manga_archives_item_layout]", "big_thumbnail"},
                                              {"vars[s]", query}};
    try {
        const auto response = Http::postForm(QUrl(m_config.base + QStringLiteral("/wp-admin/admin-ajax.php")), form,
                                             {{"X-Requested-With", "XMLHttpRequest"},
                                              {"Referer", (m_config.base + QLatin1Char('/')).toUtf8()}});
        const auto html = response.body.trimmed();
        if (response.ok() && !html.isEmpty() && html != "0")
            if (auto parsed = parseSearch(html); !parsed.isEmpty())
                return parsed;
    } catch (const Error &) {
    }
    return searchHtml(query);
}

QList<Chapter> MadaraSource::chapters(const QString &seriesUrl) {
    auto normalized = seriesUrl;
    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);
    const Headers ajax{{"X-Requested-With", "XMLHttpRequest"}, {"Referer", (normalized + QLatin1Char('/')).toUtf8()}};
    QByteArray html;
    try {
        Request request;
        request.url = QUrl(normalized + QStringLiteral("/ajax/chapters/"));
        request.method = "POST";
        request.headers = ajax;
        html = Http::send(request).body;
    } catch (const Error &) {
    }
    if (!html.contains("wp-manga-chapter")) {
        const auto page = Http::getOk(QUrl(normalized + QLatin1Char('/')), htmlHeaders(), 20000).body;
        HtmlDocument doc(page);
        const auto mangaId = doc.first(QStringLiteral("div[id^=\"manga-chapters-holder\"]")).attr("data-id");
        if (!mangaId.isEmpty())
            html = Http::postForm(QUrl(m_config.base + QStringLiteral("/wp-admin/admin-ajax.php")),
                                  {{"action", "manga_get_chapters"}, {"manga", mangaId}}, ajax)
                       .body;
        else
            html = page;
    }
    return parseChapterList(html, normalized + QLatin1Char('/'));
}

// ---------------------------------------------------------------- MangaVerse

namespace {
// The MangaVerse theme's settings, printed into every page as JSON.
QString themeSetting(const QByteArray &html, const char *name) {
    const QRegularExpression re(QStringLiteral("\"%1\":\"([^\"]*)\"").arg(QLatin1String(name)));
    return re.match(QString::fromUtf8(html)).captured(1).replace(QStringLiteral("\\/"), QStringLiteral("/"));
}

// "Solo Leveling ES, Capitulo 201" -> "Capitulo 201".
QString mangaverseChapterTitle(const QString &title) {
    static const QRegularExpression digit(QStringLiteral("\\d"));
    const auto last = title.section(QStringLiteral(", "), -1).trimmed();
    return digit.match(last).hasMatch() ? last : title;
}
} // namespace

QString MangaverseSource::cleanTitle(QString title, const QString &suffix) {
    // Series names carry the edition ("Solo Leveling ES").
    title = title.trimmed();
    if (!suffix.isEmpty() && title.endsWith(QLatin1Char(' ') + suffix))
        title.chop(suffix.size() + 1);
    return title.trimmed();
}

QList<Summary> MangaverseSource::parseCards(const QByteArray &html, const QString &base, const QString &titleSuffix) {
    HtmlDocument doc(html);
    static const QRegularExpression background(QStringLiteral(R"(url\(['"]?([^'")]+)['"]?\))"));
    QList<Summary> out;
    QSet<QString> seen;
    for (const auto &card : doc.select(QStringLiteral(".series-card"))) {
        const auto link = card.first(QStringLiteral("a.series-card-link, a"));
        const auto url = resolveUrl(base + QLatin1Char('/'), link.attr("href"));
        const auto title = cleanTitle(card.first(QStringLiteral(".series-card-title")).textTrim(), titleSuffix);
        if (url.isEmpty() || title.isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        Summary item;
        item.title = title;
        item.url = url;
        item.cover = background.match(card.first(QStringLiteral(".series-card-thumb")).attr("style")).captured(1);
        out.append(item);
    }
    return out;
}

QList<Chapter> MangaverseSource::parseChapters(const QByteArray &html, const QString &pageUrl) {
    HtmlDocument doc(html);
    QList<Chapter> out;
    for (const auto &a : doc.select(QStringLiteral("a.chapter-link"))) {
        const auto url = resolveUrl(pageUrl, a.attr("href"));
        if (url.isEmpty())
            continue;
        auto title = a.first(QStringLiteral(".chapter-title")).textTrim();
        if (title.isEmpty())
            title = a.textTrim();
        Chapter ch;
        ch.title = mangaverseChapterTitle(title);
        ch.number = chapterNumberFrom(ch.title);
        ch.url = url;
        ch.slug = pathSegment(QUrl(url), -1);
        out.append(ch);
    }
    return out;
}

QList<Summary> MangaverseSource::search(const QString &query) {
    const auto html = Http::getOk(queryUrl(m_config.base + QStringLiteral("/"), {{"s", query}}), htmlHeaders(), 20000).body;
    auto out = parseCards(html, m_config.base, m_config.options.value("titleSuffix").toString());
    for (auto &item : out)
        item.source = key();
    return out;
}

QList<Chapter> MangaverseSource::chapters(const QString &seriesUrl) {
    const auto html = Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body;
    QList<Chapter> out;
    QSet<QString> seen;
    const auto collect = [&](const QByteArray &part) {
        for (const auto &ch : parseChapters(part, seriesUrl))
            if (!seen.contains(ch.url)) {
                seen.insert(ch.url);
                out.append(ch);
            }
    };
    collect(html);

    // The page shows the newest ten; the rest come ten at a time.
    HtmlDocument doc(html);
    const auto button = doc.first(QStringLiteral("#load-more-series"));
    const auto category = button.attr("data-category");
    const int total = button.attr("data-total").toInt();
    const auto ajax = themeSetting(html, "ajax_url");
    const auto nonce = themeSetting(html, "nonce");
    const auto language = themeSetting(html, "current_lang");
    if (!category.isEmpty() && total > 10 && !ajax.isEmpty()) {
        const Headers headers{{"X-Requested-With", "XMLHttpRequest"}, {"Referer", seriesUrl.toUtf8()}};
        QList<std::function<QByteArray()>> jobs;
        for (int page = 2; page <= (total + 9) / 10 && page <= 200; ++page)
            jobs.append([=] {
                const auto response = Http::postForm(QUrl(ajax),
                                                     {{"action", "mangaverse_load_more"},
                                                      {"nonce", nonce},
                                                      {"page", QString::number(page)},
                                                      {"type", "series"},
                                                      {"category_id", category},
                                                      {"order", "desc"},
                                                      {"lang", language}},
                                                     headers);
                return QJsonDocument::fromJson(response.body).object().value("data").toObject().value("html").toString().toUtf8();
            });
        for (const auto &result : parallel(jobs))
            if (result.ok())
                collect(*result.value);
    }
    sortChapters(out);
    return out;
}

std::optional<Details> MangaverseSource::details(const QString &seriesUrl) {
    HtmlDocument doc(Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body);
    Details d;
    d.title = cleanTitle(doc.first(QStringLiteral(".series-title")).textTrim(), m_config.options.value("titleSuffix").toString());
    d.cover = resolveUrl(seriesUrl, doc.first(QStringLiteral(".series-header-thumbnail img")).attr("src"));
    if (d.cover.isEmpty())
        d.cover = metaContent(doc, QStringLiteral("og:image"));
    // Paragraphs of the synopsis; the last line lists the genres, and
    // paragraphs with links recommend other series.
    const auto paragraphs = doc.select(QStringLiteral(".series-description p"));
    QStringList text;
    for (int i = 0; i < paragraphs.size(); ++i) {
        const auto line = paragraphs[i].textTrim();
        if (line.isEmpty() || paragraphs[i].first(QStringLiteral("a")))
            continue;
        if (i == paragraphs.size() - 1 && line.count(QLatin1Char(',')) >= 2 && !line.contains(QLatin1Char('.'))) {
            for (const auto &genre : line.split(QLatin1Char(',')))
                if (const auto g = genre.trimmed(); !g.isEmpty())
                    d.genres.append(g);
            continue;
        }
        text.append(line);
    }
    d.description = text.join(QStringLiteral("\n\n"));
    if (d.description.isEmpty())
        d.description = metaContent(doc, QStringLiteral("description"));
    return d;
}

// ---------------------------------------------------------------- InManga

namespace {
// Series: https://inmanga.com/ver/manga/<Name>/<id>; chapters:
// https://inmanga.com/ver/manga/<Name>/<number>/<chapter id>.
QString inMangaNumber(double number) { return QString::number(number, 'g', 10); }
} // namespace

QList<Summary> InMangaSource::parseSearch(const QByteArray &html, const QString &base) {
    HtmlDocument doc(html);
    QList<Summary> out;
    for (const auto &a : doc.select(QStringLiteral("a.manga-result"))) {
        Summary item;
        item.url = resolveUrl(base + QLatin1Char('/'), a.attr("href"));
        item.title = a.first(QStringLiteral(".ellipsed-text")).textTrim();
        item.cover = resolveUrl(base + QLatin1Char('/'), a.first(QStringLiteral("img")).attr("data-src"));
        item.type = QStringLiteral("Manga");
        if (!item.url.isEmpty() && !item.title.isEmpty())
            out.append(item);
    }
    return out;
}

QList<Panel> InMangaSource::parsePages(const QByteArray &html, const QString &imageHost, const QString &chapterId) {
    HtmlDocument doc(html);
    const auto mangaId = doc.first(QStringLiteral("#MangaIdentification")).attr("value").trimmed().toLower();
    QList<Panel> out;
    if (mangaId.isEmpty())
        return out;
    QSet<QString> seen;
    // The page picker lists every page by id (it appears twice).
    for (const auto &option : doc.select(QStringLiteral("#PageList option"))) {
        const auto page = option.attr("value").trimmed().toLower();
        if (page.isEmpty() || seen.contains(page))
            continue;
        seen.insert(page);
        out.append({QStringLiteral("%1/i/m/%2/c/%3/o/%4.jpg").arg(imageHost, mangaId, chapterId.toLower(), page)});
    }
    return out;
}

QList<Summary> InMangaSource::search(const QString &query) {
    const auto response = Http::postForm(QUrl(m_config.base + QStringLiteral("/manga/getMangasConsultResult")),
                                         {{"filter[generes][]", "-1"},
                                          {"filter[queryString]", query},
                                          {"filter[skip]", "0"},
                                          {"filter[take]", "24"},
                                          {"filter[sortby]", "1"},
                                          {"filter[broadcastStatus]", "0"},
                                          {"filter[onlyFavorites]", "false"},
                                          {"d", ""}},
                                         {{"X-Requested-With", "XMLHttpRequest"},
                                          {"Referer", (m_config.base + QStringLiteral("/manga/consult")).toUtf8()}});
    if (!response.ok())
        throw Error(response.isChallenge() ? QStringLiteral("The site asked for a browser check.")
                                           : QStringLiteral("InManga answered %1.").arg(response.status),
                    response.status);
    auto out = parseSearch(response.body, m_config.base);
    for (auto &item : out)
        item.source = key();
    return out;
}

QList<Chapter> InMangaSource::chapters(const QString &seriesUrl) {
    const QUrl url(seriesUrl);
    const auto name = pathSegment(url, 2);
    const auto id = pathSegment(url, -1);
    if (pathSegment(url, 0) != QLatin1String("ver") || name.isEmpty() || id.isEmpty())
        throw Error(QStringLiteral("That is not an InManga series link."));
    const auto wrapper = Http::getJson(queryUrl(m_config.base + QStringLiteral("/chapter/getall"), {{"mangaIdentification", id}}),
                                       {{"X-Requested-With", "XMLHttpRequest"}, {"Referer", seriesUrl.toUtf8()}});
    // The list is JSON inside a string.
    const auto data = QJsonDocument::fromJson(wrapper.object().value("data").toString().toUtf8()).object();
    if (!data.value("success").toBool())
        throw Error(QStringLiteral("InManga has no chapters for this series."));
    QList<Chapter> out;
    for (const auto &row : data.value("result").toArray()) {
        const auto c = row.toObject();
        const auto chapterId = str(c.value("Identification"));
        if (chapterId.isEmpty() || !c.value("PagesCount").toInt())
            continue;
        Chapter ch;
        ch.number = inMangaNumber(c.value("Number").toDouble());
        ch.title = chapterLabel(ch.number);
        auto path = str(c.value("FriendlyChapterNumberUrl"));
        if (path.isEmpty())
            path = ch.number;
        ch.url = QStringLiteral("%1/ver/manga/%2/%3/%4").arg(m_config.base, name, path, chapterId);
        ch.slug = chapterId;
        ch.released = str(c.value("RegistrationDate")).left(19);
        out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Panel> InMangaSource::panels(const QString &chapterUrl) {
    const auto chapterId = pathSegment(QUrl(chapterUrl), -1);
    const auto html = Http::getOk(queryUrl(m_config.base + QStringLiteral("/chapter/chapterIndexControls"), {{"identification", chapterId}}),
                                  {{"X-Requested-With", "XMLHttpRequest"}, {"Referer", chapterUrl.toUtf8()}}, 20000)
                          .body;
    const auto out = parsePages(html, m_config.options.value("imageHost").toString(QStringLiteral("https://cdn1.intomanga.com")),
                                chapterId);
    if (out.isEmpty())
        throw Error(QStringLiteral("This InManga chapter is unavailable."));
    // InManga lost the pages of many older chapters (its own reader shows
    // nothing either); say so instead of opening a column of broken images.
    Request probe;
    probe.url = QUrl(out.first().url);
    probe.method = "HEAD";
    probe.timeoutMs = 10000;
    if (Http::send(probe).status == 404)
        throw Error(QStringLiteral("InManga no longer has the pages of this chapter."), 404);
    return out;
}

std::optional<Details> InMangaSource::details(const QString &seriesUrl) {
    HtmlDocument doc(Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body);
    Details d;
    const auto heading = doc.first(QStringLiteral("h1"));
    d.title = heading.textTrim();
    d.cover = resolveUrl(seriesUrl, doc.first(QStringLiteral("img[src*=\"/i/m/\"]")).attr("src"));
    if (const auto panel = heading.closest(QStringLiteral(".panel")); panel)
        d.description = panel.first(QStringLiteral(".panel-body")).textTrim();
    for (const auto &row : doc.select(QStringLiteral(".list-group-item"))) {
        const auto text = row.textTrim();
        if (text.endsWith(QLatin1String("Estado")))
            d.status = row.first(QStringLiteral(".label")).textTrim();
    }
    return d;
}

// ---------------------------------------------------------------- LeerCapitulo

QList<Summary> LeerCapituloSource::parseSearch(const QByteArray &html, const QString &base) {
    HtmlDocument doc(html);
    QList<Summary> out;
    QSet<QString> seen;
    for (const auto &card : doc.select(QStringLiteral("article.lc-card"))) {
        const auto name = card.first(QStringLiteral("a.lc-card-name"));
        const auto url = resolveUrl(base + QLatin1Char('/'), name.attr("href"));
        if (url.isEmpty() || name.textTrim().isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        Summary item;
        item.title = name.textTrim();
        item.url = url;
        item.cover = resolveUrl(base + QLatin1Char('/'), card.first(QStringLiteral(".lc-card-cover img")).attr("src"));
        item.type = card.first(QStringLiteral(".lc-card-badge")).textTrim();
        out.append(item);
    }
    return out;
}

QList<Chapter> LeerCapituloSource::parseChapters(const QByteArray &html, const QString &pageUrl) {
    HtmlDocument doc(html);
    QList<Chapter> out;
    QSet<QString> seen;
    for (const auto &a : doc.select(QStringLiteral("#chapterList a.lc-chapter-row"))) {
        const auto url = resolveUrl(pageUrl, a.attr("href"));
        if (url.isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        Chapter ch;
        ch.title = a.first(QStringLiteral(".n")).textTrim();
        if (ch.title.isEmpty())
            ch.title = a.textTrim();
        ch.number = chapterNumberFrom(ch.title);
        // Links end in the chapter number (/leer/<id>/<slug>/12/).
        if (ch.number == QLatin1String("0"))
            ch.number = chapterNumberFrom(pathSegment(QUrl(url), -1));
        ch.url = url;
        ch.slug = pathSegment(QUrl(url), -1);
        ch.released = a.first(QStringLiteral(".d")).textTrim();
        out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Panel> LeerCapituloSource::parsePanels(const QByteArray &html, const QString &pageUrl) {
    HtmlDocument doc(html);
    QList<Panel> out;
    QSet<QString> seen;
    for (const auto &img : doc.select(QStringLiteral("#lcPages img"))) {
        auto src = img.attr("data-src");
        if (src.isEmpty() || src.startsWith(QLatin1String("data:")))
            src = img.attr("src");
        const auto url = resolveUrl(pageUrl, src);
        if (url.isEmpty() || !url.startsWith(QLatin1String("https:")) || seen.contains(url))
            continue;
        seen.insert(url);
        out.append({url});
    }
    return out;
}

QList<Summary> LeerCapituloSource::search(const QString &query) {
    const auto html = Http::getOk(queryUrl(m_config.base + QStringLiteral("/manga/"), {{"q", query}}), htmlHeaders(), 20000).body;
    auto out = parseSearch(html, m_config.base);
    for (auto &item : out)
        item.source = key();
    return out;
}

QList<Chapter> LeerCapituloSource::chapters(const QString &seriesUrl) {
    return parseChapters(Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body, seriesUrl);
}

QList<Panel> LeerCapituloSource::panels(const QString &chapterUrl) {
    return parsePanels(Http::getOk(QUrl(chapterUrl), htmlHeaders(), 20000).body, chapterUrl);
}

std::optional<Details> LeerCapituloSource::details(const QString &seriesUrl) {
    HtmlDocument doc(Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body);
    Details d;
    d.title = doc.first(QStringLiteral("article h1, h1")).textTrim();
    d.cover = resolveUrl(seriesUrl, doc.first(QStringLiteral(".lc-cover-lg img")).attr("src"));
    QStringList paragraphs;
    for (const auto &p : doc.select(QStringLiteral("#sinopsis p")))
        if (const auto text = p.textTrim(); !text.isEmpty())
            paragraphs.append(text);
    d.description = paragraphs.join(QStringLiteral("\n\n"));
    for (const auto &badge : doc.select(QStringLiteral("article a.badge[href*=\"genre=\"]")))
        if (const auto text = badge.textTrim(); !text.isEmpty() && !d.genres.contains(text))
            d.genres.append(text);
    for (const auto &fact : doc.select(QStringLiteral("ul.lc-facts li"))) {
        const auto label = fact.first(QStringLiteral(".k")).textTrim();
        const auto value = fact.textTrim().mid(label.size()).trimmed();
        if (label == QLatin1String("Estado"))
            d.status = value;
        else if (label == QLatin1String("Autor"))
            d.author = value;
    }
    return d;
}

// ---------------------------------------------------------------- MangaWorld

namespace {
// "18 Ottobre 2026" dates on the chapter list.
QString italianDate(const QString &text) {
    if (text.isEmpty())
        return {};
    QLocale locale(QLocale::Italian, QLocale::Italy);
    const auto date = locale.toDate(collapse(text), QStringLiteral("d MMMM yyyy"));
    return date.isValid() ? date.toString(Qt::ISODate) : QString();
}
} // namespace

QList<Summary> MangaWorldSource::parseCards(const QByteArray &html, const QString &base) {
    const HtmlDocument doc(html);
    QList<Summary> out;
    QSet<QString> seen;
    for (const auto &card : doc.select(QStringLiteral("div.entry"))) {
        const auto name = card.first(QStringLiteral("a.manga-title"));
        const auto url = resolveUrl(base + QLatin1Char('/'), name.attr("href"));
        if (url.isEmpty() || name.textTrim().isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        Summary item;
        item.title = name.textTrim();
        item.url = url;
        const auto img = card.first(QStringLiteral("a.thumb img"));
        item.cover = resolveUrl(base + QLatin1Char('/'), img.attr("src").isEmpty() ? img.attr("data-src") : img.attr("src"));
        item.type = card.first(QStringLiteral("div.genre a, div.status a")).textTrim();
        item.author = card.first(QStringLiteral("div.author a")).textTrim();
        QStringList genres;
        for (const auto &g : card.select(QStringLiteral("div.genres a")))
            if (const auto text = g.textTrim(); !text.isEmpty())
                genres.append(text);
        item.genres = genres.join(QStringLiteral(", "));
        out.append(item);
    }
    return out;
}

QList<Summary> MangaWorldSource::search(const QString &query) {
    const auto html = Http::getOk(queryUrl(m_config.base + QStringLiteral("/archive"),
                                           {{"keyword", query}, {"page", "1"}}),
                                  htmlHeaders(), 20000)
                          .body;
    auto out = parseCards(html, m_config.base);
    for (auto &item : out)
        item.source = key();
    return out;
}

QList<Chapter> MangaWorldSource::parseChapters(const QByteArray &html, const QString &pageUrl) {
    const HtmlDocument doc(html);
    QList<Chapter> out;
    QSet<QString> seen;
    for (const auto &a : doc.select(QStringLiteral("#chapterList a.chap"))) {
        const auto url = resolveUrl(pageUrl, a.attr("href"));
        if (url.isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        Chapter ch;
        ch.url = url;
        ch.title = a.first(QStringLiteral("span")).textTrim();
        if (ch.title.isEmpty())
            ch.title = a.attr("title").trimmed();
        ch.number = chapterNumberFrom(ch.title);
        ch.slug = pathSegment(QUrl(url), -1);
        ch.released = italianDate(a.first(QStringLiteral(".chap-date")).textTrim());
        out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Chapter> MangaWorldSource::chapters(const QString &seriesUrl) {
    return parseChapters(Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body, seriesUrl);
}

QList<Panel> MangaWorldSource::parsePanels(const QByteArray &html, const QString &pageUrl) {
    const HtmlDocument doc(html);
    QList<Panel> out;
    QSet<QString> seen;
    for (const auto &img : doc.select(QStringLiteral("img.page-image"))) {
        const auto src = img.attr("src").isEmpty() ? img.attr("data-src") : img.attr("src");
        const auto url = resolveUrl(pageUrl, src);
        if (url.isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        out.append({url});
    }
    return out;
}

QList<Panel> MangaWorldSource::panels(const QString &chapterUrl) {
    // The reader pages one image at a time; ?style=list renders them all.
    QUrl url(chapterUrl);
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("style"), QStringLiteral("list"));
    url.setQuery(query);
    auto out = parsePanels(Http::getOk(url, htmlHeaders(), 20000).body, chapterUrl);
    if (out.isEmpty())
        throw Error(QStringLiteral("MangaWorld did not return any pages for this chapter."));
    return out;
}

std::optional<Details> MangaWorldSource::details(const QString &seriesUrl) {
    const HtmlDocument doc(Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body);
    Details d;
    d.title = doc.first(QStringLiteral("h1.name")).textTrim();
    if (d.title.isEmpty())
        d.title = metaContent(doc, QStringLiteral("og:title"));
    const auto image = doc.first(QStringLiteral(".comic-info .thumb img"));
    d.cover = resolveUrl(seriesUrl, image.attr("src").isEmpty() ? metaContent(doc, QStringLiteral("og:image")) : image.attr("src"));
    d.description = doc.first(QStringLiteral("#noidungm")).textTrim();
    for (const auto &badge : doc.select(QStringLiteral(".meta-data a[href*=\"genre=\"]")))
        if (const auto text = badge.textTrim(); !text.isEmpty() && !d.genres.contains(text))
            d.genres.append(text);
    for (const auto &row : doc.select(QStringLiteral(".meta-data div"))) {
        const auto text = row.textTrim();
        if (text.startsWith(QStringLiteral("Autore:")))
            d.author = row.first(QStringLiteral("a")).textTrim();
        else if (text.startsWith(QStringLiteral("Stato:")))
            d.status = row.first(QStringLiteral("a")).textTrim();
    }
    return d;
}

QList<LatestItem> MangaWorldSource::latest(int limit) {
    const HtmlDocument doc(Http::getOk(QUrl(m_config.base + QLatin1Char('/')), htmlHeaders(), 20000).body);
    QList<LatestItem> out;
    QSet<QString> seen;
    for (const auto &card : doc.select(QStringLiteral("#chapters-slide .entry"))) {
        const auto link = card.first(QStringLiteral("a.thumb"));
        const auto url = resolveUrl(m_config.base + QLatin1Char('/'), link.attr("href"));
        const auto title = card.first(QStringLiteral("a.manga-title")).textTrim();
        if (url.isEmpty() || title.isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        LatestItem item;
        item.source = key();
        item.sourceLabel = label();
        item.title = title;
        item.url = url;
        const auto img = card.first(QStringLiteral("img"));
        item.cover = resolveUrl(m_config.base + QLatin1Char('/'), img.attr("src").isEmpty() ? img.attr("data-src") : img.attr("src"));
        const auto chapter = card.first(QStringLiteral("div.chapter")).textTrim();
        item.chapterNumber = chapterNumberFrom(chapter);
        item.chapterTitle = chapter;
        item.chapterUrl = url;
        out.append(item);
        if (out.size() >= limit)
            break;
    }
    return out;
}

} // namespace sc::comics
