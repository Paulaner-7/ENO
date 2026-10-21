#include "Anime.h"

#include "Library.h"
#include "core/Database.h"
#include "core/Html.h"
#include "core/Parallel.h"
#include "sources/Manifest.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <algorithm>

namespace sc {

namespace {

const QUrl kAniList(QStringLiteral("https://graphql.anilist.co/"));

const char *kCardFields = R"(
  id idMal
  title { romaji english native userPreferred }
  format status(version: 2) episodes duration season seasonYear
  startDate { year }
  genres averageScore popularity isAdult bannerImage
  coverImage { extraLarge large medium color }
  nextAiringEpisode { airingAt timeUntilAiring episode }
)";

const char *kMediaFields = R"(
  id idMal
  title { romaji english native userPreferred }
  description(asHtml: false)
  format status(version: 2) episodes duration
  season seasonYear startDate { year month day }
  genres averageScore meanScore popularity isAdult
  bannerImage
  coverImage { extraLarge large medium color }
  trailer { id site thumbnail }
  nextAiringEpisode { airingAt timeUntilAiring episode }
  studios(isMain: true) { nodes { name } }
  relations {
    edges {
      relationType
      node {
        id
        type
        format
        title { romaji english userPreferred }
        coverImage { large }
        startDate { year month day }
        relations {
          edges {
            relationType
            node {
              id
              type
              format
              title { romaji english userPreferred }
              coverImage { large }
              startDate { year month day }
            }
          }
        }
      }
    }
  }
  mediaListEntry { id status score(format: POINT_10_DECIMAL) progress }
)";

QJsonObject graphql(const QString &query, const QJsonObject &variables, const QString &cacheKey, qint64 ttl) {
    if (!cacheKey.isEmpty())
        if (auto cached = Database::cacheGet(cacheKey))
            return QJsonDocument::fromJson(*cached).object();
    Response response;
    Headers headers{{"Accept", "application/json"}};
    const auto account = Database::pref(QStringLiteral("tracker.anilist")).toMap();
    const auto token = account.value("token").toString();
    if (!token.isEmpty())
        headers.append({"Authorization", "Bearer " + token.toUtf8()});
    try {
        response = Http::postJson(kAniList, QJsonDocument(QJsonObject{{"query", query}, {"variables", variables}}).toJson(),
                                  headers);
    } catch (const Error &) {
        if (!cacheKey.isEmpty())
            if (auto stale = Database::cacheGet(cacheKey, true))
                return QJsonDocument::fromJson(*stale).object();
        throw;
    }
    if (response.status == 429) {
        if (!cacheKey.isEmpty())
            if (auto stale = Database::cacheGet(cacheKey, true))
                return QJsonDocument::fromJson(*stale).object();
        throw Error(QStringLiteral("AniList is busy. Try again in a minute."), 429);
    }
    const auto root = QJsonDocument::fromJson(response.body).object();
    const auto data = root.value("data").toObject();
    if (data.isEmpty()) {
        for (const auto &e : root.value("errors").toArray())
            if (e.toObject().value("status").toInt() == 404)
                throw Error(QStringLiteral("Anime not found."), 404);
        throw Error(QStringLiteral("Anime details are unavailable right now."), 502);
    }
    if (!cacheKey.isEmpty())
        Database::cachePut(cacheKey, QJsonDocument(data).toJson(QJsonDocument::Compact), ttl);
    return data;
}

QString title(const QJsonObject &media) {
    const auto t = media.value("title").toObject();
    for (const char *key : {"english", "romaji", "userPreferred", "native"})
        if (auto value = t.value(QLatin1String(key)).toString(); !value.isEmpty())
            return value;
    return QStringLiteral("Untitled");
}

QString formatLabel(const QString &format) {
    static const QHash<QString, QString> labels{{"TV", "TV"},           {"TV_SHORT", "TV Short"}, {"MOVIE", "Movie"},
                                                {"SPECIAL", "Special"}, {"OVA", "OVA"},           {"ONA", "ONA"},
                                                {"MUSIC", "Music"}};
    return labels.value(format, format);
}

QString statusLabel(const QString &status) {
    static const QHash<QString, QString> labels{{"FINISHED", "Finished"},
                                                {"RELEASING", "Airing"},
                                                {"NOT_YET_RELEASED", "Upcoming"},
                                                {"CANCELLED", "Cancelled"},
                                                {"HIATUS", "Hiatus"}};
    return labels.value(status, status);
}

QString decodeEntitiesKeepLines(const QString &text) {
    QStringList lines;
    for (const auto &line : text.split(QLatin1Char('\n')))
        lines.append(decodeEntities(line));
    return lines.join(QLatin1Char('\n')).replace(QRegularExpression(QStringLiteral("\n{3,}")), QStringLiteral("\n\n"));
}

QString plainDescription(const QString &text) {
    auto out = text;
    out.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption),
                QStringLiteral("\n"));
    out.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
    out = decodeEntitiesKeepLines(out);
    return out.trimmed();
}


QVariantMap card(const QJsonObject &m) {
    const auto cover = m.value("coverImage").toObject();
    const auto next = m.value("nextAiringEpisode").toObject();
    QVariantMap map{{"anilistId", m.value("id").toInt()},
                    {"idMal", m.value("idMal").toInt()},
                    {"id", Library::idFor(QStringLiteral("anime"), QStringLiteral("anilist"),
                                          QString::number(m.value("id").toInt()))},
                    {"media", QStringLiteral("anime")},
                    {"title", title(m)},
                    {"romaji", m.value("title").toObject().value("romaji").toString()},
                    {"cover", cover.value("extraLarge").toString(cover.value("large").toString())},
                    {"coverSmall", cover.value("large").toString(cover.value("medium").toString())},
                    {"color", cover.value("color").toString()},
                    {"banner", m.value("bannerImage").toString()},
                    {"format", formatLabel(m.value("format").toString())},
                    {"status", statusLabel(m.value("status").toString())},
                    {"rawStatus", m.value("status").toString()},
                    {"episodes", m.value("episodes").toInt()},
                    {"duration", m.value("duration").toInt()},
                    {"score", m.value("averageScore").toInt()},
                    {"popularity", m.value("popularity").toInt()},
                    {"year", m.value("seasonYear").toInt() ? m.value("seasonYear").toInt()
                                                            : m.value("startDate").toObject().value("year").toInt()},
                    {"season", m.value("season").toString()},
                    {"genres", m.value("genres").toVariant()},
                    {"isAdult", m.value("isAdult").toBool()},
                    {"nextEpisode", next.value("episode").toInt()},
                    {"nextAiringAt", qint64(next.value("airingAt").toDouble())},
                    {"url", QStringLiteral("https://anilist.co/anime/%1").arg(m.value("id").toInt())}};
    const auto entry = m.value("mediaListEntry").toObject();
    if (!entry.isEmpty()) {
        map.insert(QStringLiteral("hasUserEntry"), true);
        map.insert(QStringLiteral("userStatus"), entry.value("status").toString());
        map.insert(QStringLiteral("userScore"), entry.value("score").toDouble());
        map.insert(QStringLiteral("userProgress"), entry.value("progress").toInt());
    }
    return map;
}

QVariantList cards(const QJsonArray &media) {
    QVariantList out;
    for (const auto &m : media)
        if (m.isObject())
            out.append(card(m.toObject()));
    return out;
}

int releasedCount(const QJsonObject &media) {
    const auto next = media.value("nextAiringEpisode").toObject();
    if (!next.isEmpty())
        return std::max(0, next.value("episode").toInt() - 1);
    if (media.value("status").toString() == QLatin1String("NOT_YET_RELEASED"))
        return 0;
    return media.value("episodes").toInt();
}

QJsonObject mediaObject(int id) {
    const auto data = graphql(QStringLiteral("query ($id: Int) { Media(id: $id, type: ANIME) { %1 } }")
                                  .arg(QString::fromLatin1(kMediaFields)),
                              {{"id", id}}, QStringLiteral("anilist:media:v3:%1").arg(id), 6 * 3600);
    const auto media = data.value("Media").toObject();
    if (media.isEmpty())
        throw Error(QStringLiteral("Anime not found."), 404);
    return media;
}

QPair<QString, QString> currentSeason() {
    const auto now = QDateTime::currentDateTimeUtc().date();
    const int m = now.month();
    const auto season = m <= 3 ? "WINTER" : m <= 6 ? "SPRING" : m <= 9 ? "SUMMER" : "FALL";
    return {QString::fromLatin1(season), QString::number(now.year())};
}

} // namespace

bool Anime::webPlayerAvailable() const {
#if defined(SCANCORE_HAVE_WEBENGINE) || defined(SCANCORE_HAVE_WEBVIEW)
    return true;
#else
    return false;
#endif
}

bool Anime::nativePlayerAvailable() const {
#ifdef SCANCORE_HAVE_MPV
    return true;
#else
    return false;
#endif
}

QString Anime::playerEngine() const {
    // Development override: SCANCORE_PLAYER=webengine|system.
    if (const auto forced = qEnvironmentVariable("SCANCORE_PLAYER"); !forced.isEmpty())
        return forced;
#if defined(SCANCORE_HAVE_WEBVIEW) && defined(Q_OS_WIN)
    // The episodes are H.264, which the Qt WebEngine in the official Qt
    // packages cannot decode; WebView2 can. It ships with Windows 11 and is
    // installed with Edge on Windows 10, but may have been removed.
    static const bool webView2 = [] {
        for (const auto *root : {"HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node", "HKEY_LOCAL_MACHINE\\SOFTWARE",
                                 "HKEY_CURRENT_USER\\Software"}) {
            const QSettings key(QStringLiteral("%1\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}")
                                    .arg(QLatin1String(root)),
                                QSettings::NativeFormat);
            const auto version = key.value(QStringLiteral("pv")).toString();
            if (!version.isEmpty() && version != QLatin1String("0.0.0.0"))
                return true;
        }
        return false;
    }();
    if (webView2)
        return QStringLiteral("system");
#elif defined(SCANCORE_HAVE_WEBVIEW)
    return QStringLiteral("system");
#endif
#ifdef SCANCORE_HAVE_WEBENGINE
    return QStringLiteral("webengine");
#else
    return {};
#endif
}

QString Anime::seriesId(int anilistId) const {
    return Library::idFor(QStringLiteral("anime"), QStringLiteral("anilist"), QString::number(anilistId));
}

QStringList Anime::genres() const {
    return {QStringLiteral("Action"),    QStringLiteral("Adventure"),    QStringLiteral("Comedy"),
            QStringLiteral("Drama"),     QStringLiteral("Ecchi"),        QStringLiteral("Fantasy"),
            QStringLiteral("Horror"),    QStringLiteral("Mahou Shoujo"), QStringLiteral("Mecha"),
            QStringLiteral("Music"),     QStringLiteral("Mystery"),      QStringLiteral("Psychological"),
            QStringLiteral("Romance"),   QStringLiteral("Sci-Fi"),       QStringLiteral("Slice of Life"),
            QStringLiteral("Sports"),    QStringLiteral("Supernatural"), QStringLiteral("Thriller")};
}

QVariantMap Anime::seriesFor(const QVariantMap &media) const {
    QVariantMap extra{{"anilistId", media.value("anilistId")}, {"banner", media.value("banner")},
                      {"color", media.value("color")},         {"format", media.value("format")},
                      {"episodes", media.value("episodes")},   {"score", media.value("score")},
                      {"year", media.value("year")}};
    return {{"id", seriesId(media.value("anilistId").toInt())},
            {"media", QStringLiteral("anime")},
            {"source", QStringLiteral("anilist")},
            {"sourceUrl", QStringLiteral("https://anilist.co/anime/%1").arg(media.value("anilistId").toInt())},
            {"title", media.value("title")},
            {"cover", media.value("cover")},
            {"description", media.value("description")},
            {"status", media.value("status")},
            {"genres", media.value("genres")},
            {"extra", extra}};
}

Task *Anime::home() {
    return Task::run([]() -> QVariant {
        const auto [season, year] = currentSeason();
        const auto list = [](const QString &sort, const QString &extraDecl, const QString &extraArgs,
                             const QJsonObject &vars) {
            return [=]() -> QVariant {
                QJsonObject variables = vars;
                variables.insert("sort", QJsonArray::fromStringList(sort.split(',')));
                variables.insert("perPage", 30);
                const auto key = QStringLiteral("anilist:list:%1:%2")
                                     .arg(sort, QString::fromUtf8(QJsonDocument(vars).toJson(QJsonDocument::Compact)));
                const auto data = graphql(QStringLiteral("query ($sort: [MediaSort], $perPage: Int%1) { Page(page: 1, "
                                                         "perPage: $perPage) { media(type: ANIME, isAdult: false, sort: "
                                                         "$sort%2) { %3 } } }")
                                              .arg(extraDecl, extraArgs, QString::fromLatin1(kCardFields)),
                                          variables, key, 3600);
                return cards(data.value("Page").toObject().value("media").toArray());
            };
        };
        QList<std::function<QVariant()>> jobs{
            list(QStringLiteral("TRENDING_DESC,POPULARITY_DESC"), {}, {}, {}),
            list(QStringLiteral("POPULARITY_DESC"), {}, {}, {}),
            list(QStringLiteral("POPULARITY_DESC,SCORE_DESC"),
                 QStringLiteral(", $season: MediaSeason, $seasonYear: Int"),
                 QStringLiteral(", season: $season, seasonYear: $seasonYear"),
                 {{"season", season}, {"seasonYear", year.toInt()}}),
            list(QStringLiteral("SCORE_DESC"), {}, {}, {}),
        };
        const auto results = parallel(jobs);
        if (std::none_of(results.begin(), results.end(), [](auto &r) { return r.ok(); }))
            throw Error(results.first().error);
        auto value = [&](int i) { return results[i].ok() ? *results[i].value : QVariant(QVariantList()); };
        return QVariantMap{{"trending", value(0)}, {"popular", value(1)}, {"season", value(2)}, {"top", value(3)},
                           {"seasonName", QStringLiteral("%1 %2").arg(season.left(1) + season.mid(1).toLower(), year)}};
    });
}

Task *Anime::search(const QVariantMap &filters) {
    return Task::run([filters]() -> QVariant {
        const auto q = filters.value("q").toString().simplified();
        auto sort = filters.value("sort").toString();
        if (sort.isEmpty() || (sort == QLatin1String("SEARCH_MATCH") && q.isEmpty()))
            sort = q.isEmpty() ? QStringLiteral("POPULARITY_DESC") : QStringLiteral("SEARCH_MATCH");
        QJsonObject vars{{"page", std::max(1, filters.value("page").toInt())},
                         {"perPage", 40},
                         {"sort", QJsonArray{sort}},
                         {"isAdult", filters.value("adult").toBool()}};
        if (!q.isEmpty())
            vars.insert("search", q);
        if (const auto g = filters.value("genres").toStringList(); !g.isEmpty())
            vars.insert("genres", QJsonArray::fromStringList(g));
        if (const auto y = filters.value("yearFrom").toInt(); y > 0)
            vars.insert("yearGreater", y * 10000);
        if (const auto y = filters.value("yearTo").toInt(); y > 0)
            vars.insert("yearLesser", (y + 1) * 10000);
        if (const auto s = filters.value("status").toString(); !s.isEmpty())
            vars.insert("status", s);
        if (const auto f = filters.value("format").toString(); !f.isEmpty())
            vars.insert("format", f);
        const auto data = graphql(
            QStringLiteral("query ($search: String, $page: Int, $perPage: Int, $genres: [String], $yearGreater: "
                           "FuzzyDateInt, $yearLesser: FuzzyDateInt, $status: MediaStatus, $format: MediaFormat, $sort: "
                           "[MediaSort], $isAdult: Boolean) { Page(page: $page, perPage: $perPage) { pageInfo { "
                           "hasNextPage } media(search: $search, type: ANIME, sort: $sort, genre_in: $genres, "
                           "startDate_greater: $yearGreater, startDate_lesser: $yearLesser, status: $status, format: "
                           "$format, isAdult: $isAdult) { %1 } } }")
                .arg(QString::fromLatin1(kCardFields)),
            vars, QStringLiteral("anilist:search:") + QJsonDocument(vars).toJson(QJsonDocument::Compact), 3600);
        const auto page = data.value("Page").toObject();
        return QVariantMap{{"results", cards(page.value("media").toArray())},
                           {"hasNextPage", page.value("pageInfo").toObject().value("hasNextPage").toBool()}};
    });
}

Task *Anime::media(int anilistId) {
    return Task::run([anilistId]() -> QVariant {
        const auto m = mediaObject(anilistId);
        auto map = card(m);
        map.insert("description", plainDescription(m.value("description").toString()));
        QStringList studios;
        for (const auto &s : m.value("studios").toObject().value("nodes").toArray())
            studios.append(s.toObject().value("name").toString());
        map.insert("studios", studios);
        map.insert("released", releasedCount(m));
        const auto trailer = m.value("trailer").toObject();
        if (trailer.value("site").toString() == QLatin1String("youtube"))
            map.insert("trailer", QStringLiteral("https://www.youtube.com/watch?v=%1").arg(trailer.value("id").toString()));
        struct RelatedItem {
            int anilistId = 0;
            QString relation;
            QString title;
            QString format;
            QString cover;
            int year = 0;
            qint64 sortKey = 9999999999LL;
        };

        QHash<int, RelatedItem> relatedMap;

        auto processNode = [&](const QJsonObject &node, const QString &relation, bool direct) {
            if (node.value(QStringLiteral("type")).toString() != QLatin1String("ANIME"))
                return;
            const int id = node.value(QStringLiteral("id")).toInt();
            if (id <= 0 || id == anilistId)
                return;
            const auto formatStr = node.value(QStringLiteral("format")).toString();
            if (formatStr == QLatin1String("MUSIC"))
                return;

            const auto dateObj = node.value(QStringLiteral("startDate")).toObject();
            const int y = dateObj.value(QStringLiteral("year")).toInt();
            const int m = dateObj.value(QStringLiteral("month")).toInt();
            const int d = dateObj.value(QStringLiteral("day")).toInt();
            qint64 key = 9999999999LL;
            if (y > 0)
                key = static_cast<qint64>(y) * 10000 + (m > 0 ? m : 1) * 100 + (d > 0 ? d : 1);

            if (!relatedMap.contains(id)) {
                RelatedItem item;
                item.anilistId = id;
                item.relation = direct ? relation : QString();
                item.title = title(node);
                item.format = formatLabel(formatStr);
                item.cover = node.value(QStringLiteral("coverImage")).toObject().value(QStringLiteral("large")).toString();
                item.year = y;
                item.sortKey = key;
                relatedMap.insert(id, item);
            } else if (direct && relatedMap[id].relation.isEmpty()) {
                relatedMap[id].relation = relation;
            }
        };

        for (const auto &e1 : m.value(QStringLiteral("relations")).toObject().value(QStringLiteral("edges")).toArray()) {
            const auto edge1 = e1.toObject();
            const auto node1 = edge1.value(QStringLiteral("node")).toObject();
            const auto relType1 = edge1.value(QStringLiteral("relationType")).toString().replace('_', ' ').toLower();

            processNode(node1, relType1, true);

            for (const auto &e2 : node1.value(QStringLiteral("relations")).toObject().value(QStringLiteral("edges")).toArray()) {
                const auto edge2 = e2.toObject();
                const auto node2 = edge2.value(QStringLiteral("node")).toObject();
                const auto relType2 = edge2.value(QStringLiteral("relationType")).toString().replace('_', ' ').toLower();
                processNode(node2, relType2, false);
            }
        }

        auto sortedList = relatedMap.values();
        std::sort(sortedList.begin(), sortedList.end(), [](const RelatedItem &a, const RelatedItem &b) {
            if (a.sortKey != b.sortKey)
                return a.sortKey < b.sortKey;
            return a.title < b.title;
        });

        QVariantList relations;
        relations.reserve(sortedList.size());
        for (const auto &item : sortedList) {
            relations.append(QVariantMap{
                {QStringLiteral("anilistId"), item.anilistId},
                {QStringLiteral("relation"), item.relation},
                {QStringLiteral("title"), item.title},
                {QStringLiteral("format"), item.format},
                {QStringLiteral("cover"), item.cover},
                {QStringLiteral("year"), item.year > 0 ? QVariant(item.year) : QVariant()},
                {QStringLiteral("sortKey"), item.sortKey}
            });
        }
        map.insert("relations", relations);
        return map;
    });
}

Task *Anime::episodes(int anilistId) {
    return Task::run([anilistId]() -> QVariant {
        const auto m = mediaObject(anilistId);
        QHash<int, QVariantMap> known;
        const auto cacheKey = QStringLiteral("anizip:%1").arg(anilistId);
        QJsonObject zip;
        if (auto cached = Database::cacheGet(cacheKey))
            zip = QJsonDocument::fromJson(*cached).object();
        else {
            try {
                const auto response =
                    Http::get(QUrl(QStringLiteral("https://api.ani.zip/mappings?anilist_id=%1").arg(anilistId)), {}, 10000);
                if (response.ok()) {
                    zip = QJsonDocument::fromJson(response.body).object();
                    Database::cachePut(cacheKey, response.body, 12 * 3600);
                }
            } catch (const Error &) {
                if (auto stale = Database::cacheGet(cacheKey, true))
                    zip = QJsonDocument::fromJson(*stale).object();
            }
        }
        const auto today = QDateTime::currentDateTimeUtc();
        int highest = 0;
        const auto eps = zip.value("episodes").toObject();
        static const QRegularExpression digits(QStringLiteral("^\\d+$"));
        for (auto it = eps.begin(); it != eps.end(); ++it) {
            if (!digits.match(it.key()).hasMatch())
                continue;
            const auto e = it.value().toObject();
            const auto air = e.value("airDate").toString(e.value("airdate").toString());
            if (!air.isEmpty() && QDateTime::fromString(air, Qt::ISODate) > today)
                continue;
            const int number = it.key().toInt();
            const auto titles = e.value("title").toObject();
            auto name = titles.value("en").toString();
            if (name.isEmpty())
                name = titles.value("x-jat").toString();
            known.insert(number, QVariantMap{{"title", name},
                                             {"image", e.value("image").toString()},
                                             {"overview", e.value("overview").toString(e.value("summary").toString())},
                                             {"airDate", air},
                                             {"runtime", e.value("runtime").toInt(e.value("length").toInt())}});
            highest = std::max(highest, number);
        }
        const int count = std::min(5000, std::max(releasedCount(m), highest));
        QVariantList out;
        for (int n = 1; n <= count; ++n) {
            auto row = known.value(n);
            if (row.value("title").toString().isEmpty())
                row.insert("title", QStringLiteral("Episode %1").arg(n));
            row.insert("number", n);
            row.insert("key", QString::number(n));
            out.append(row);
        }
        return out;
    });
}

Task *Anime::recommendations(int anilistId) {
    return Task::run([anilistId]() -> QVariant {
        const auto data = graphql(QStringLiteral("query ($id: Int) { Media(id: $id, type: ANIME) { recommendations(sort: "
                                                 "[RATING_DESC, ID_DESC], page: 1, perPage: 20) { nodes { "
                                                 "mediaRecommendation { %1 } } } } }")
                                      .arg(QString::fromLatin1(kCardFields)),
                                  {{"id", anilistId}}, QStringLiteral("anilist:recs:%1").arg(anilistId), 86400);
        QJsonArray media;
        for (const auto &n : data.value("Media").toObject().value("recommendations").toObject().value("nodes").toArray())
            if (const auto r = n.toObject().value("mediaRecommendation"); r.isObject())
                media.append(r);
        return cards(media);
    });
}

Task *Anime::schedule(qint64 start, qint64 end) {
    return Task::run([start, end]() -> QVariant {
        QVariantList out;
        for (int page = 1; page <= 8; ++page) {
            const auto data = graphql(QStringLiteral("query ($page: Int, $start: Int, $end: Int) { Page(page: $page, "
                                                     "perPage: 50) { pageInfo { hasNextPage } airingSchedules(airingAt_greater: "
                                                     "$start, airingAt_lesser: $end, sort: TIME) { id airingAt episode media "
                                                     "{ %1 } } } }")
                                          .arg(QString::fromLatin1(kCardFields)),
                                      {{"page", page}, {"start", double(start)}, {"end", double(end)}},
                                      QStringLiteral("anilist:schedule:%1:%2:%3").arg(start).arg(end).arg(page), 3600);
            const auto p = data.value("Page").toObject();
            for (const auto &row : p.value("airingSchedules").toArray()) {
                const auto r = row.toObject();
                const auto m = r.value("media").toObject();
                if (m.isEmpty() || m.value("isAdult").toBool())
                    continue;
                auto c = card(m);
                c.insert("airingAt", qint64(r.value("airingAt").toDouble()));
                c.insert("airingEpisode", r.value("episode").toInt());
                out.append(c);
            }
            if (!p.value("pageInfo").toObject().value("hasNextPage").toBool())
                break;
        }
        return out;
    });
}

bool Anime::isEmbedAllowed(const QString &raw) const {
    const QUrl url(raw);
    if (url.scheme() != QLatin1String("https"))
        return false;
    const auto hosts = Manifest::current().value("anime").toObject().value("reanime").toObject().value("embedHosts").toArray();
    for (const auto &h : hosts)
        if (hostMatches(url.host(), h.toString()))
            return true;
    return false;
}

Task *Anime::streams(int anilistId, int episode) {
    return Task::run([this, anilistId, episode]() -> QVariant {
        const auto base = Manifest::current()
                              .value("anime")
                              .toObject()
                              .value("reanime")
                              .toObject()
                              .value("base")
                              .toString(QStringLiteral("https://reanime.to"));
        const auto response =
            Http::get(QUrl(QStringLiteral("%1/api/flix/%2/%3").arg(base).arg(anilistId).arg(episode)),
                      {{"Referer", (base + QLatin1Char('/')).toUtf8()}, {"Accept", "application/json"}}, 12000);
        if (response.status == 404)
            return QVariantMap{{"streams", QVariantList()}, {"message", QStringLiteral("This episode is not available yet.")}};
        if (!response.ok())
            throw Error(QStringLiteral("The player server responded with HTTP %1.").arg(response.status), response.status);
        QVariantList streams;
        QSet<QString> seen;
        for (const auto &s : response.json().object().value("servers").toArray()) {
            const auto server = s.toObject();
            QUrl link(server.value("dataLink").toString());
            if (link.scheme() != QLatin1String("https") || seen.contains(link.toString()) || !isEmbedAllowed(link.toString()))
                continue;
            seen.insert(link.toString());
            QUrlQuery query(link);
            query.removeAllQueryItems(QStringLiteral("autoPlay"));
            query.addQueryItem(QStringLiteral("autoPlay"), QStringLiteral("true"));
            link.setQuery(query);
            const auto name = server.value("serverName").toString();
            streams.append(QVariantMap{{"url", link.toString()},
                                       {"quality", name.isEmpty() ? QStringLiteral("Server %1").arg(streams.size() + 1) : name},
                                       {"type", QStringLiteral("embed")}});
        }
        return QVariantMap{{"streams", streams},
                           {"message", streams.isEmpty() ? QStringLiteral("No servers have this episode yet.") : QString()}};
    });
}

Task *Anime::skipTimes(int anilistId, int episode, int duration) {
    return Task::run([anilistId, episode, duration]() -> QVariant {
        const auto m = mediaObject(anilistId);
        const int mal = m.value("idMal").toInt();
        if (!mal)
            return QVariantList();
        const int length = std::max(1, int(std::round(duration / 10.0)) * 10);
        const auto key = QStringLiteral("aniskip:%1:%2:%3").arg(mal).arg(episode).arg(length);
        QByteArray body;
        if (auto cached = Database::cacheGet(key))
            body = *cached;
        else {
            QUrl url(QStringLiteral("https://api.aniskip.com/v2/skip-times/%1/%2").arg(mal).arg(episode));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("episodeLength"), QString::number(length));
            for (const char *t : {"op", "ed", "mixed-op", "mixed-ed", "recap"})
                q.addQueryItem(QStringLiteral("types"), QString::fromLatin1(t));
            url.setQuery(q);
            const auto response = Http::get(url, {{"Accept", "application/json"}}, 8000);
            body = response.status == 404 ? QByteArray("{}") : response.body;
            Database::cachePut(key, body, 86400);
        }
        QVariantList out;
        const auto root = QJsonDocument::fromJson(body).object();
        if (!root.value("found").toBool())
            return out;
        for (const auto &r : root.value("results").toArray()) {
            const auto row = r.toObject();
            const auto interval = row.value("interval").toObject();
            out.append(QVariantMap{{"type", row.value("skipType").toString()},
                                   {"start", interval.value("startTime").toDouble()},
                                   {"end", interval.value("endTime").toDouble()}});
        }
        return out;
    });
}

} // namespace sc
