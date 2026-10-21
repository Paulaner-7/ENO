#include "Kitsu.h"

#include "core/Database.h"
#include "core/Http.h"
#include "core/Parallel.h"

#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QSet>
#include <QUrlQuery>

#include <cmath>
#include <functional>

namespace sc::anime::kitsu {

namespace {

const QString kBase = QStringLiteral("https://kitsu.io/api/edge");
// Kitsu gives no more than this many anime per page.
constexpr int kPage = 20;
// What a listing needs of each anime and its AniList id.
const QList<QPair<QString, QString>> kListFields{
    {QStringLiteral("include"), QStringLiteral("categories,mappings")},
    {QStringLiteral("fields[categories]"), QStringLiteral("title")},
    {QStringLiteral("fields[mappings]"), QStringLiteral("externalSite,externalId")}};

// A Kitsu document, from the cache when fresh, else the network (a stale
// copy when that fails).
QJsonObject get(const QString &path, const QList<QPair<QString, QString>> &params, qint64 ttl) {
    QUrl url(kBase + path);
    QUrlQuery query;
    for (const auto &[k, v] : params)
        query.addQueryItem(k, v);
    url.setQuery(query);
    const auto key = QStringLiteral("kitsu:") + url.toString(QUrl::FullyEncoded);
    if (auto cached = Database::cacheGet(key))
        return QJsonDocument::fromJson(*cached).object();
    Response response;
    try {
        response = Http::get(url, {{"Accept", "application/vnd.api+json"}}, 15000);
    } catch (const Error &) {
        if (auto stale = Database::cacheGet(key, true))
            return QJsonDocument::fromJson(*stale).object();
        throw;
    }
    if (!response.ok()) {
        if (auto stale = Database::cacheGet(key, true))
            return QJsonDocument::fromJson(*stale).object();
        throw Error(response.status == 404 ? QStringLiteral("Anime not found.")
                                           : QStringLiteral("Kitsu is unavailable right now (%1).").arg(response.status),
                    response.status == 404 ? 404 : 502);
    }
    Database::cachePut(key, response.body, ttl);
    return QJsonDocument::fromJson(response.body).object();
}

using Included = QHash<QString, QJsonObject>;

Included index(const QJsonArray &included) {
    Included out;
    for (const auto &value : included) {
        const auto o = value.toObject();
        out.insert(o.value("type").toString() + QLatin1Char(':') + o.value("id").toString(), o);
    }
    return out;
}

QList<QJsonObject> related(const QJsonObject &resource, const QString &name, const Included &included) {
    QList<QJsonObject> out;
    const auto data = resource.value("relationships").toObject().value(name).toObject().value("data");
    const auto refs = data.isArray() ? data.toArray() : QJsonArray{data};
    for (const auto &ref : refs) {
        const auto r = ref.toObject();
        if (const auto it = included.constFind(r.value("type").toString() + QLatin1Char(':') + r.value("id").toString());
            it != included.cend())
            out.append(*it);
    }
    return out;
}

// Kitsu's categories that are AniList genres, by AniList's names.
QString genreFor(const QString &category) {
    static const QHash<QString, QString> renamed{{"Science Fiction", "Sci-Fi"}, {"Magical Girl", "Mahou Shoujo"},
                                                 {"Mahou Shoujo", "Mahou Shoujo"}, {"Slice of Life", "Slice of Life"}};
    static const QSet<QString> genres{"Action",  "Adventure", "Comedy",  "Drama",         "Ecchi",        "Fantasy",
                                      "Horror",  "Mecha",     "Music",   "Mystery",       "Psychological", "Romance",
                                      "Sports",  "Supernatural", "Thriller"};
    if (const auto it = renamed.constFind(category); it != renamed.cend())
        return *it;
    return genres.contains(category) ? category : QString();
}

// AniList's genre as a Kitsu category slug.
QString slugFor(const QString &genre) {
    static const QHash<QString, QString> renamed{{"Sci-Fi", "science-fiction"}, {"Mahou Shoujo", "magical-girl"}};
    return renamed.value(genre, genre.toLower().replace(QLatin1Char(' '), QLatin1Char('-')));
}

QJsonObject date(const QString &iso) {
    const auto d = QDate::fromString(iso.left(10), Qt::ISODate);
    if (!d.isValid())
        return {};
    return {{"year", d.year()}, {"month", d.month()}, {"day", d.day()}};
}

QJsonArray converted(const QJsonObject &document) {
    QJsonArray out;
    const auto included = document.value("included").toArray();
    for (const auto &value : document.value("data").toArray())
        if (auto media = toAniList(value.toObject(), included); !media.isEmpty())
            out.append(media);
    return out;
}

QList<QPair<QString, QString>> paged(QList<QPair<QString, QString>> params, int page) {
    params.append({QStringLiteral("page[limit]"), QString::number(kPage)});
    params.append({QStringLiteral("page[offset]"), QString::number((std::max(1, page) - 1) * kPage)});
    return params + kListFields;
}

QPair<QString, int> currentSeason() {
    const auto now = QDateTime::currentDateTimeUtc().date();
    const int m = now.month();
    return {QString::fromLatin1(m <= 3 ? "winter" : m <= 6 ? "spring" : m <= 9 ? "summer" : "fall"), now.year()};
}

} // namespace

QJsonObject toAniList(const QJsonObject &anime, const QJsonArray &includedArray) {
    const auto included = index(includedArray);
    int anilistId = 0;
    int malId = 0;
    for (const auto &mapping : related(anime, QStringLiteral("mappings"), included)) {
        const auto a = mapping.value("attributes").toObject();
        const auto site = a.value("externalSite").toString();
        if (site == QLatin1String("anilist/anime"))
            anilistId = a.value("externalId").toString().toInt();
        else if (site == QLatin1String("myanimelist/anime"))
            malId = a.value("externalId").toString().toInt();
    }
    if (anilistId <= 0)
        return {};
    const auto a = anime.value("attributes").toObject();
    const auto titles = a.value("titles").toObject();
    static const QHash<QString, QString> formats{{"TV", "TV"},           {"movie", "MOVIE"}, {"OVA", "OVA"},
                                                 {"ONA", "ONA"},         {"special", "SPECIAL"}, {"music", "MUSIC"}};
    static const QHash<QString, QString> statuses{{"current", "RELEASING"}, {"finished", "FINISHED"},
                                                  {"upcoming", "NOT_YET_RELEASED"}, {"unreleased", "NOT_YET_RELEASED"},
                                                  {"tba", "NOT_YET_RELEASED"}};
    QJsonArray genres;
    for (const auto &category : related(anime, QStringLiteral("categories"), included))
        if (const auto g = genreFor(category.value("attributes").toObject().value("title").toString());
            !g.isEmpty() && !genres.contains(g))
            genres.append(g);
    const auto poster = a.value("posterImage").toObject();
    const auto cover = a.value("coverImage").toObject();
    const auto start = date(a.value("startDate").toString());
    const int month = start.value("month").toInt();
    QJsonObject media{
        {"id", anilistId},
        {"idMal", malId},
        {"title", QJsonObject{{"romaji", titles.value("en_jp").toString(a.value("canonicalTitle").toString())},
                              {"english", titles.value("en").toString(titles.value("en_us").toString())},
                              {"native", titles.value("ja_jp").toString()},
                              {"userPreferred", a.value("canonicalTitle").toString()}}},
        {"synonyms", a.value("abbreviatedTitles").toArray()},
        {"description", a.value("synopsis").toString()},
        {"format", formats.value(a.value("subtype").toString())},
        {"status", statuses.value(a.value("status").toString())},
        {"episodes", a.value("episodeCount").toInt()},
        {"duration", a.value("episodeLength").toInt()},
        {"season", month <= 0 ? QString() : month <= 3 ? "WINTER" : month <= 6 ? "SPRING" : month <= 9 ? "SUMMER" : "FALL"},
        {"seasonYear", start.value("year").toInt()},
        {"startDate", start},
        {"genres", genres},
        {"averageScore", int(std::lround(a.value("averageRating").toString().toDouble()))},
        {"popularity", a.value("userCount").toInt()},
        {"isAdult", a.value("nsfw").toBool()},
        {"bannerImage", cover.value("small").toString(cover.value("large").toString())},
        {"coverImage", QJsonObject{{"extraLarge", poster.value("large").toString(poster.value("original").toString())},
                                   {"large", poster.value("medium").toString(poster.value("large").toString())},
                                   {"medium", poster.value("small").toString()}}},
    };
    // When the next episode airs; which one it is, Kitsu does not say (0:
    // the episode list then counts what ani.zip has seen air).
    if (const auto next = QDateTime::fromString(a.value("nextRelease").toString(), Qt::ISODateWithMs); next.isValid())
        media.insert("nextAiringEpisode", QJsonObject{{"airingAt", double(next.toSecsSinceEpoch())}, {"episode", 0}});
    if (const auto trailer = a.value("youtubeVideoId").toString(); !trailer.isEmpty())
        media.insert("trailer", QJsonObject{{"id", trailer}, {"site", "youtube"}});
    QJsonArray studios;
    for (const auto &production : related(anime, QStringLiteral("animeProductions"), included)) {
        if (production.value("attributes").toObject().value("role").toString() != QLatin1String("studio"))
            continue;
        for (const auto &producer : related(production, QStringLiteral("producer"), included))
            studios.append(QJsonObject{{"name", producer.value("attributes").toObject().value("name").toString()}});
    }
    media.insert("studios", QJsonObject{{"nodes", studios}});
    media.insert("relations", QJsonObject{{"edges", QJsonArray()}});
    return media;
}

QJsonArray list(const QString &listing) {
    QList<QPair<QString, QString>> params;
    if (listing == QLatin1String("trending")) {
        // AniList's trending is this week's activity; what is airing now
        // and followed most, from the last two years, comes closest.
        const int year = QDate::currentDate().year();
        params = {{"filter[status]", "current"}, {"filter[seasonYear]", QStringLiteral("%1..%2").arg(year - 1).arg(year)},
                  {"sort", "-userCount"}};
    } else if (listing == QLatin1String("season")) {
        const auto [season, year] = currentSeason();
        params = {{"filter[season]", season}, {"filter[seasonYear]", QString::number(year)}, {"sort", "-userCount"}};
    } else if (listing == QLatin1String("top")) {
        params = {{"sort", "ratingRank"}};
    } else {
        params = {{"sort", "-userCount"}};
    }
    // Thirty, as AniList's lists have: two of Kitsu's pages.
    QList<std::function<QJsonArray()>> jobs;
    for (int page = 1; page <= 2; ++page)
        jobs.append([params, page] { return converted(get(QStringLiteral("/anime"), paged(params, page), 3600)); });
    QJsonArray out;
    const auto pages = parallel(jobs);
    if (!pages.first().ok())
        throw Error(pages.first().error, pages.first().status);
    for (const auto &p : pages)
        if (p.ok())
            for (const auto &m : *p.value)
                if (out.size() < 30)
                    out.append(m);
    return out;
}

QJsonObject search(const QVariantMap &filters) {
    QList<QPair<QString, QString>> params;
    const auto q = filters.value("q").toString().simplified();
    if (!q.isEmpty()) {
        // Kitsu orders text matches by relevance only.
        params.append({"filter[text]", q});
    } else {
        static const QHash<QString, QString> sorts{{"SCORE_DESC", "ratingRank"},     {"START_DATE_DESC", "-startDate"},
                                                   {"START_DATE", "startDate"},      {"FAVOURITES_DESC", "-favoritesCount"},
                                                   {"TRENDING_DESC", "-userCount"},  {"POPULARITY_DESC", "-userCount"}};
        params.append({"sort", sorts.value(filters.value("sort").toString(), QStringLiteral("-userCount"))});
    }
    QStringList slugs;
    for (const auto &g : filters.value("genres").toStringList())
        slugs.append(slugFor(g));
    if (!slugs.isEmpty())
        params.append({"filter[categories]", slugs.join(QLatin1Char(','))});
    const int from = filters.value("yearFrom").toInt(), to = filters.value("yearTo").toInt();
    if (from > 0 || to > 0)
        params.append({"filter[seasonYear]", QStringLiteral("%1..%2").arg(from > 0 ? from : 1900).arg(to > 0 ? to : 2100)});
    static const QHash<QString, QString> statuses{{"RELEASING", "current"}, {"FINISHED", "finished"}, {"NOT_YET_RELEASED", "upcoming"}};
    if (const auto s = statuses.value(filters.value("status").toString()); !s.isEmpty())
        params.append({"filter[status]", s});
    static const QHash<QString, QString> formats{{"TV", "TV"},           {"TV_SHORT", "TV"}, {"MOVIE", "movie"}, {"SPECIAL", "special"},
                                                 {"OVA", "OVA"},         {"ONA", "ONA"},     {"MUSIC", "music"}};
    if (const auto f = formats.value(filters.value("format").toString()); !f.isEmpty())
        params.append({"filter[subtype]", f});
    const auto document = get(QStringLiteral("/anime"), paged(params, filters.value("page").toInt()), 3600);
    return {{"media", converted(document)}, {"hasNextPage", document.value("links").toObject().contains("next")}};
}

QJsonObject media(int anilistId) {
    const auto mapping = get(QStringLiteral("/mappings"),
                             {{"filter[externalSite]", "anilist/anime"}, {"filter[externalId]", QString::number(anilistId)}, {"include", "item"}},
                             30 * 86400);
    QString kitsuId;
    for (const auto &m : mapping.value("data").toArray()) {
        const auto item = m.toObject().value("relationships").toObject().value("item").toObject().value("data").toObject();
        if (item.value("type").toString() == QLatin1String("anime"))
            kitsuId = item.value("id").toString();
    }
    if (kitsuId.isEmpty())
        throw Error(QStringLiteral("Anime not found."), 404);
    const auto document = get(QStringLiteral("/anime/") + kitsuId,
                              {{"include", "categories,mappings,animeProductions.producer"}}, 6 * 3600);
    const auto media = toAniList(document.value("data").toObject(), document.value("included").toArray());
    if (media.isEmpty())
        throw Error(QStringLiteral("Anime not found."), 404);
    return media;
}

namespace {

// ani.zip's episode list for an anime (the cache Anime::episodes keeps), or
// an empty one.
QJsonObject anizip(int anilistId) {
    const auto key = QStringLiteral("anizip:%1").arg(anilistId);
    if (auto cached = Database::cacheGet(key))
        return QJsonDocument::fromJson(*cached).object();
    try {
        const auto response = Http::get(QUrl(QStringLiteral("https://api.ani.zip/mappings?anilist_id=%1").arg(anilistId)), {}, 10000);
        if (response.ok()) {
            Database::cachePut(key, response.body, 12 * 3600);
            return QJsonDocument::fromJson(response.body).object();
        }
    } catch (const Error &e) {
        if (e.status() == 499)
            throw;
    }
    if (auto stale = Database::cacheGet(key, true))
        return QJsonDocument::fromJson(*stale).object();
    return {};
}

} // namespace

QJsonArray airing(qint64 start, qint64 end) {
    // The eighty most followed shows airing since last year and the twenty
    // most followed long-running ones (Kitsu has four hundred airing, most
    // of them years-long children's series), each with its episodes' air
    // times from ani.zip.
    const int year = QDate::currentDate().year();
    QList<std::function<QJsonArray()>> pages;
    for (int page = 1; page <= 4; ++page)
        pages.append([page, year] {
            return converted(get(QStringLiteral("/anime"),
                                 paged({{"filter[status]", "current"}, {"filter[seasonYear]", QStringLiteral("%1..%2").arg(year - 1).arg(year)},
                                        {"sort", "-userCount"}},
                                       page),
                                 3600));
        });
    pages.append([] {
        return converted(get(QStringLiteral("/anime"), paged({{"filter[status]", "current"}, {"sort", "-userCount"}}, 1), 3600));
    });
    const auto listed = parallel(pages);
    if (!listed.first().ok())
        throw Error(listed.first().error, listed.first().status);
    QList<QJsonObject> shows;
    QSet<int> seen;
    for (const auto &p : listed)
        if (p.ok())
            for (const auto &value : *p.value)
                if (const auto media = value.toObject(); !seen.contains(media.value("id").toInt())) {
                    seen.insert(media.value("id").toInt());
                    shows.append(media);
                }
    QList<std::function<QList<QJsonObject>()>> jobs;
    for (const auto &media : std::as_const(shows))
        jobs.append([media, start, end] {
            QList<QJsonObject> rows;
            const auto episodes = anizip(media.value("id").toInt()).value("episodes").toObject();
            for (auto it = episodes.begin(); it != episodes.end(); ++it) {
                bool number = false;
                const int episode = it.key().toInt(&number);
                const auto at = QDateTime::fromString(it.value().toObject().value("airDateUtc").toString(), Qt::ISODate);
                if (!number || !at.isValid())
                    continue;
                if (const auto t = at.toSecsSinceEpoch(); t >= start && t < end)
                    rows.append({{"airingAt", double(t)}, {"episode", episode}, {"media", media}});
            }
            // Not on ani.zip: when Kitsu says the next one airs.
            if (episodes.isEmpty())
                if (const auto next = qint64(media.value("nextAiringEpisode").toObject().value("airingAt").toDouble());
                    next >= start && next < end)
                    rows.append({{"airingAt", double(next)}, {"episode", 0}, {"media", media}});
            return rows;
        });
    QList<QJsonObject> rows;
    for (const auto &result : parallel(jobs))
        if (result.ok())
            rows += *result.value;
    std::sort(rows.begin(), rows.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value("airingAt").toDouble() < b.value("airingAt").toDouble();
    });
    QJsonArray out;
    for (const auto &row : rows)
        out.append(row);
    return out;
}

int anilistIdForMal(int malId) {
    try {
        const auto key = QStringLiteral("anizip:mal:%1").arg(malId);
        QByteArray body;
        if (auto cached = Database::cacheGet(key)) {
            body = *cached;
        } else {
            const auto response = Http::get(QUrl(QStringLiteral("https://api.ani.zip/mappings?mal_id=%1").arg(malId)), {}, 10000);
            if (response.ok()) {
                body = response.body;
                Database::cachePut(key, body, 30 * 86400);
            }
        }
        if (const int id = QJsonDocument::fromJson(body).object().value("mappings").toObject().value("anilist_id").toInt(); id > 0)
            return id;
    } catch (const Error &e) {
        if (e.status() == 499)
            throw;
    }
    const auto mapping = get(QStringLiteral("/mappings"),
                             {{"filter[externalSite]", "myanimelist/anime"}, {"filter[externalId]", QString::number(malId)}, {"include", "item"}},
                             30 * 86400);
    for (const auto &m : mapping.value("data").toArray()) {
        const auto item = m.toObject().value("relationships").toObject().value("item").toObject().value("data").toObject();
        if (item.value("type").toString() != QLatin1String("anime"))
            continue;
        const auto document = get(QStringLiteral("/anime/") + item.value("id").toString(), kListFields, 30 * 86400);
        if (const int id = toAniList(document.value("data").toObject(), document.value("included").toArray()).value("id").toInt(); id > 0)
            return id;
    }
    return 0;
}

} // namespace sc::anime::kitsu
