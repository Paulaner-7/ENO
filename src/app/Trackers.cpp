#include "Trackers.h"

#include "App.h"
#include "core/Database.h"
#include "core/Html.h"
#include "core/Text.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QUuid>
#include <QWaitCondition>

#ifndef SCANCORE_ANILIST_CLIENT_ID
#define SCANCORE_ANILIST_CLIENT_ID ""
#endif
#ifndef SCANCORE_MAL_CLIENT_ID
#define SCANCORE_MAL_CLIENT_ID ""
#endif
#ifndef SCANCORE_COMICK_CLIENT_ID
#define SCANCORE_COMICK_CLIENT_ID ""
#endif

namespace sc {

namespace {

const QString kAniListId = QStringLiteral(SCANCORE_ANILIST_CLIENT_ID);
const QString kMalId = QStringLiteral(SCANCORE_MAL_CLIENT_ID);
const QString kComickId = QStringLiteral(SCANCORE_COMICK_CLIENT_ID);
// Sign-in pages come back to SCANCORE on this port.
constexpr quint16 kRedirectPort = 52817;
const QString kMalRedirect = QStringLiteral("http://127.0.0.1:52817/mal");
const QString kComickRedirect = QStringLiteral("http://127.0.0.1:52817/comick");
const QString kComickAuth = QStringLiteral("https://comick.dev/api/auth/oauth2");
const QString kComickApi = QStringLiteral("https://api.comick.dev/integrations/v1");

QMutex g_linksMutex;

// A sign-in page coming back to the local address.
struct Pending {
    QMutex mutex;
    QWaitCondition arrived;
    QString code;
    QString error;
};

// The code from the sign-in page; up to five minutes for the user.
QString waitForCode(const std::shared_ptr<Pending> &pending, const QString &service) {
    QMutexLocker lock(&pending->mutex);
    for (int i = 0; i < 600 && pending->code.isEmpty() && pending->error.isEmpty(); ++i) {
        if (CancelScope::cancelled())
            throw Error(QStringLiteral("Cancelled"), 499);
        pending->arrived.wait(&pending->mutex, 500);
    }
    if (pending->error == QLatin1String("Cancelled"))
        throw Error(QStringLiteral("Cancelled"), 499);
    if (!pending->error.isEmpty() || pending->code.isEmpty())
        throw Error(QStringLiteral("%1 sign-in did not finish.").arg(service));
    return pending->code;
}

QString randomText(int length) {
    static const QString alphabet = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~");
    QString out;
    for (int i = 0; i < length; ++i)
        out += alphabet.at(QRandomGenerator::system()->bounded(alphabet.size()));
    return out;
}

QJsonObject anilistQuery(const QString &query, const QJsonObject &variables, const QString &token = {}) {
    Headers headers{{"Accept", "application/json"}};
    if (!token.isEmpty())
        headers.append({"Authorization", "Bearer " + token.toUtf8()});
    const auto response = Http::postJson(QUrl(QStringLiteral("https://graphql.anilist.co")),
                                         QJsonDocument(QJsonObject{{"query", query}, {"variables", variables}}).toJson(QJsonDocument::Compact),
                                         headers);
    const auto json = response.json().object();
    if (response.status == 401 ||
        (response.status == 400 &&
         json.value("errors").toArray().first().toObject().value("message").toString().contains(QLatin1String("Invalid token"))))
        throw Error(QStringLiteral("AniList signed SCANCORE out. Connect again in Settings → Connections."), 401);
    if (!response.ok()) {
        const auto message = json.value("errors").toArray().first().toObject().value("message").toString();
        throw Error(message.isEmpty() ? QStringLiteral("AniList answered %1.").arg(response.status) : QStringLiteral("AniList: ") + message,
                    response.status);
    }
    return json.value("data").toObject();
}

// MyAnimeList's token, refreshed when it is about to run out.
QString malToken() {
    auto account = Database::pref(QStringLiteral("tracker.mal")).toMap();
    if (account.isEmpty())
        return {};
    if (account.value("expires").toLongLong() > Database::now() + 120)
        return account.value("access").toString();
    const auto response = Http::postForm(QUrl(QStringLiteral("https://myanimelist.net/v1/oauth2/token")),
                                         {{"client_id", kMalId},
                                          {"grant_type", "refresh_token"},
                                          {"refresh_token", account.value("refresh").toString()}});
    if (!response.ok())
        throw Error(QStringLiteral("MyAnimeList signed SCANCORE out. Connect again in Settings → Connections."), 401);
    const auto json = response.json().object();
    account.insert("access", json.value("access_token").toString());
    account.insert("refresh", json.value("refresh_token").toString());
    account.insert("expires", Database::now() + json.value("expires_in").toInteger(3600));
    Database::setPref(QStringLiteral("tracker.mal"), account);
    return account.value("access").toString();
}

Response malCall(const QByteArray &method, const QString &path, const QList<QPair<QString, QString>> &form = {}) {
    Request request;
    request.method = method;
    request.url = QUrl(QStringLiteral("https://api.myanimelist.net/v2") + path);
    request.headers = {{"Authorization", "Bearer " + malToken().toUtf8()}, {"Accept", "application/json"}};
    if (!form.isEmpty()) {
        QUrlQuery query;
        for (const auto &[key, value] : form)
            query.addQueryItem(key, value);
        request.body = query.toString(QUrl::FullyEncoded).toUtf8();
        request.headers.append(qMakePair(QByteArray("Content-Type"), QByteArray("application/x-www-form-urlencoded")));
    }
    const auto response = Http::send(request);
    if (response.status == 401)
        throw Error(QStringLiteral("MyAnimeList signed SCANCORE out. Connect again in Settings → Connections."), 401);
    return response;
}

// Comick's access token (15 minutes), refreshed with the rotating refresh
// token when it has run out.
QMutex g_comickMutex;
QString comickToken() {
    QMutexLocker lock(&g_comickMutex);
    auto account = Database::pref(QStringLiteral("tracker.comick")).toMap();
    if (account.isEmpty())
        return {};
    if (account.value("expires").toLongLong() > Database::now() + 60)
        return account.value("access").toString();
    const auto signedOut = QStringLiteral("Comick signed SCANCORE out. Connect again in Settings → Connections.");
    if (account.value("refresh").toString().isEmpty())
        throw Error(signedOut, 401);
    const auto response = Http::postForm(QUrl(kComickAuth + QStringLiteral("/token")),
                                         {{"grant_type", "refresh_token"},
                                          {"client_id", kComickId},
                                          {"refresh_token", account.value("refresh").toString()},
                                          {"resource", kComickApi}},
                                         {{"Accept", "application/json"}});
    if (!response.ok())
        throw Error(signedOut, 401);
    const auto json = response.json().object();
    account.insert("access", json.value("access_token").toString());
    if (json.contains("refresh_token"))
        account.insert("refresh", json.value("refresh_token").toString());
    account.insert("expires", json.contains("expires_at") ? json.value("expires_at").toInteger()
                                                          : Database::now() + json.value("expires_in").toInteger(900));
    Database::setPref(QStringLiteral("tracker.comick"), account);
    return account.value("access").toString();
}

Response comickCall(const QByteArray &method, const QString &path, const QJsonObject &body = {}) {
    Request request;
    request.method = method;
    request.url = QUrl(kComickApi + path);
    request.headers = {{"Authorization", "Bearer " + comickToken().toUtf8()}, {"Accept", "application/json"}};
    if (!body.isEmpty()) {
        request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
        request.headers.append(qMakePair(QByteArray("Content-Type"), QByteArray("application/json")));
    }
    const auto response = Http::send(request);
    if (response.status == 401)
        throw Error(QStringLiteral("Comick signed SCANCORE out. Connect again in Settings → Connections."), 401);
    if (response.status == 429)
        throw Error(QStringLiteral("Comick is busy. Try again in a minute."), 429);
    return response;
}

QVariantMap allLinks() { return Database::pref(QStringLiteral("tracker.links")).toMap(); }

// Ids are numbers on AniList and MyAnimeList, and short text on Comick.
bool hasId(const QVariant &id) { return !id.toString().isEmpty() && id.toString() != QLatin1String("0"); }

void saveLink(const QString &seriesId, const QString &service, const QVariant &id, const QString &title) {
    QMutexLocker lock(&g_linksMutex);
    auto links = allLinks();
    auto entry = links.value(seriesId).toMap();
    if (hasId(id))
        entry.insert(service, QVariantMap{{"id", id}, {"title", title}});
    else
        entry.remove(service);
    if (entry.isEmpty())
        links.remove(seriesId);
    else
        links.insert(seriesId, entry);
    Database::setPref(QStringLiteral("tracker.links"), links);
}

// AniList titles: [{id, idMal, title, titles: [...], detail, cover, total}].
QVariantList anilistSearch(const QString &media, const QString &title) {
    const auto data = anilistQuery(QStringLiteral(R"(
        query ($search: String, $type: MediaType) {
          Page(perPage: 10) { media(search: $search, type: $type, sort: SEARCH_MATCH) {
            id idMal format episodes chapters startDate { year }
            title { romaji english native } synonyms coverImage { medium } } }
        })"),
                                   {{"search", title}, {"type", media == QLatin1String("anime") ? "ANIME" : "MANGA"}});
    QVariantList out;
    for (const auto &value : data.value("Page").toObject().value("media").toArray()) {
        const auto m = value.toObject();
        const auto format = m.value("format").toString();
        // Novels are AniList "manga" in the NOVEL format; comics are the rest.
        if (media == QLatin1String("novel") ? format != QLatin1String("NOVEL")
                                            : media == QLatin1String("comic") && format == QLatin1String("NOVEL"))
            continue;
        const auto t = m.value("title").toObject();
        QStringList titles{t.value("english").toString(), t.value("romaji").toString(), t.value("native").toString()};
        for (const auto &s : m.value("synonyms").toArray())
            titles.append(s.toString());
        titles.removeAll(QString());
        out.append(QVariantMap{{"id", m.value("id").toInt()},
                               {"idMal", m.value("idMal").toInt()},
                               {"title", titles.value(0)},
                               {"titles", titles},
                               {"detail", QStringList{format, m.value("startDate").toObject().value("year").isNull()
                                                                  ? QString()
                                                                  : QString::number(m.value("startDate").toObject().value("year").toInt())}
                                              .join(QStringLiteral(" · "))},
                               {"cover", m.value("coverImage").toObject().value("medium").toString()},
                               {"total", m.value(media == QLatin1String("anime") ? "episodes" : "chapters").toInt()}});
    }
    return out;
}

QVariantList malSearch(const QString &media, const QString &title) {
    const bool anime = media == QLatin1String("anime");
    const auto response = malCall("GET", QStringLiteral("/%1?q=%2&limit=10&fields=alternative_titles,media_type,%3,start_date,main_picture")
                                             .arg(anime ? QStringLiteral("anime") : QStringLiteral("manga"),
                                                  QString::fromLatin1(QUrl::toPercentEncoding(title.left(60))),
                                                  anime ? QStringLiteral("num_episodes") : QStringLiteral("num_chapters")));
    if (!response.ok())
        throw Error(QStringLiteral("MyAnimeList answered %1.").arg(response.status), response.status);
    QVariantList out;
    for (const auto &value : response.json().object().value("data").toArray()) {
        const auto node = value.toObject().value("node").toObject();
        const auto type = node.value("media_type").toString();
        const bool novel = type == QLatin1String("light_novel") || type == QLatin1String("novel");
        if (!anime && (media == QLatin1String("novel")) != novel)
            continue;
        const auto alt = node.value("alternative_titles").toObject();
        QStringList titles{node.value("title").toString(), alt.value("en").toString(), alt.value("ja").toString()};
        for (const auto &s : alt.value("synonyms").toArray())
            titles.append(s.toString());
        titles.removeAll(QString());
        out.append(QVariantMap{{"id", node.value("id").toInt()},
                               {"title", titles.value(0)},
                               {"titles", titles},
                               {"detail", QStringList{type.toUpper().replace(QLatin1Char('_'), QLatin1Char(' ')),
                                                      node.value("start_date").toString().left(4)}
                                              .join(QStringLiteral(" · "))},
                               {"cover", node.value("main_picture").toObject().value("medium").toString()},
                               {"total", node.value(anime ? "num_episodes" : "num_chapters").toInt()}});
    }
    return out;
}

// Comick titles: [{id (its HID), title, titles, detail, cover, total, anilist}].
QVariantList comickSearch(const QString &media, const QString &title) {
    QUrlQuery query{{"q", title.left(100)}, {"media_type", media == QLatin1String("anime") ? "anime" : "manga"}, {"limit", "20"}};
    const auto response = comickCall("GET", QStringLiteral("/titles?") + query.toString(QUrl::FullyEncoded));
    if (!response.ok())
        throw Error(QStringLiteral("Comick answered %1.").arg(response.status), response.status);
    static const QHash<QString, QString> kinds{{"kr", "Manhwa"}, {"cn", "Manhua"}, {"jp", "Manga"}};
    QVariantList out;
    for (const auto &value : response.json().object().value("data").toArray()) {
        const auto t = value.toObject();
        QStringList titles{t.value("title").toString()};
        for (const auto &alt : t.value("titles").toArray())
            titles.append(alt.toObject().value("title").toString());
        titles.removeAll(QString());
        titles.removeDuplicates();
        const auto year = t.value("year").isDouble() ? QString::number(t.value("year").toInt()) : QString();
        out.append(QVariantMap{{"id", t.value("hid").toString()},
                               {"title", titles.value(0)},
                               {"titles", titles},
                               {"detail", QStringList{media == QLatin1String("anime") ? QStringLiteral("Anime")
                                                                                       : kinds.value(t.value("country").toString(), QStringLiteral("Comic")),
                                                      year}
                                              .join(QStringLiteral(" · "))},
                               {"cover", t.value("cover_url").toString()},
                               {"total", t.value("total_units").toInt()},
                               {"anilist", t.value("external_ids").toObject().value("anilist").toString().toInt()}});
    }
    return out;
}

// The one candidate whose title is the series' title, or none.
QVariantMap exactMatch(const QVariantList &candidates, const QString &title) {
    const auto wanted = titleKey(title);
    if (wanted.isEmpty())
        return {};
    for (const auto &value : candidates) {
        const auto candidate = value.toMap();
        for (const auto &t : candidate.value("titles").toStringList())
            if (titleKey(t) == wanted)
                return candidate;
    }
    return {};
}

QString cleanDescription(const QString &text) {
    auto out = text;
    out.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption),
                QStringLiteral("\n"));
    out.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
    out = decodeEntities(out);
    return out.trimmed();
}

QVariantMap bestMatch(const QVariantList &candidates, const QString &title) {
    const auto exact = exactMatch(candidates, title);
    if (!exact.isEmpty())
        return exact;
    const auto wanted = titleKey(title);
    if (wanted.isEmpty() || candidates.isEmpty())
        return {};
    for (const auto &value : candidates) {
        const auto candidate = value.toMap();
        for (const auto &t : candidate.value("titles").toStringList()) {
            const auto key = titleKey(t);
            if (!key.isEmpty() && (key.startsWith(wanted) || wanted.startsWith(key)))
                return candidate;
        }
    }
    if (candidates.size() == 1)
        return candidates.first().toMap();
    return {};
}

QVariantMap anilistMediaDetails(int id, const QString &token = {}) {
    const QString cacheKey = QStringLiteral("anilist:details:%1").arg(id);
    if (token.isEmpty()) {
        if (auto cached = Database::cacheGet(cacheKey)) {
            return QJsonDocument::fromJson(*cached).toVariant().toMap();
        }
    }

    const auto data = anilistQuery(QStringLiteral(R"(
        query ($id: Int) {
          Media(id: $id) {
            id idMal siteUrl
            title { romaji english native userPreferred }
            description(asHtml: false)
            format status(version: 2)
            averageScore meanScore popularity favourites
            genres
            tags { name rank isMediaSpoiler }
            startDate { year month day }
            chapters volumes episodes duration
            bannerImage
            coverImage { extraLarge large medium color }
            mediaListEntry { id status score(format: POINT_10_DECIMAL) progress progressVolumes }
          }
        })"),
        {{"id", id}}, token);

    const auto m = data.value("Media").toObject();
    if (m.isEmpty())
        return {};

    const auto t = m.value("title").toObject();
    QString title = t.value("english").toString();
    if (title.isEmpty())
        title = t.value("userPreferred").toString();
    if (title.isEmpty())
        title = t.value("romaji").toString();
    if (title.isEmpty())
        title = t.value("native").toString();

    const auto cover = m.value("coverImage").toObject();
    const auto format = m.value("format").toString();
    const auto status = m.value("status").toString();

    const QString desc = cleanDescription(m.value("description").toString());

    QStringList tags;
    for (const auto &tagVal : m.value("tags").toArray()) {
        const auto tagObj = tagVal.toObject();
        if (tagObj.value("rank").toInt() >= 60 && !tagObj.value("isMediaSpoiler").toBool()) {
            tags.append(tagObj.value("name").toString());
            if (tags.size() >= 10)
                break;
        }
    }

    QString formatName = format;
    if (format == QLatin1String("MANGA")) formatName = QStringLiteral("Manga");
    else if (format == QLatin1String("NOVEL")) formatName = QStringLiteral("Light Novel");
    else if (format == QLatin1String("ONE_SHOT")) formatName = QStringLiteral("One Shot");
    else if (format == QLatin1String("TV")) formatName = QStringLiteral("TV");
    else if (format == QLatin1String("MOVIE")) formatName = QStringLiteral("Movie");

    QString statusName = status;
    if (status == QLatin1String("RELEASING")) statusName = QStringLiteral("Releasing");
    else if (status == QLatin1String("FINISHED")) statusName = QStringLiteral("Finished");
    else if (status == QLatin1String("NOT_YET_RELEASED")) statusName = QStringLiteral("Upcoming");
    else if (status == QLatin1String("CANCELLED")) statusName = QStringLiteral("Cancelled");
    else if (status == QLatin1String("HIATUS")) statusName = QStringLiteral("Hiatus");

    const auto entry = m.value("mediaListEntry").toObject();
    QString userStatus = entry.value("status").toString();
    double userScore = entry.value("score").toDouble();
    int userProgress = entry.value("progress").toInt();

    QVariantMap out;
    out.insert(QStringLiteral("id"), m.value("id").toInt());
    out.insert(QStringLiteral("idMal"), m.value("idMal").toInt());
    out.insert(QStringLiteral("url"), m.value("siteUrl").toString(QStringLiteral("https://anilist.co/manga/%1").arg(m.value("id").toInt())));
    out.insert(QStringLiteral("title"), title);
    out.insert(QStringLiteral("romaji"), t.value("romaji").toString());
    out.insert(QStringLiteral("native"), t.value("native").toString());
    out.insert(QStringLiteral("description"), desc);
    out.insert(QStringLiteral("format"), formatName);
    out.insert(QStringLiteral("rawFormat"), format);
    out.insert(QStringLiteral("status"), statusName);
    out.insert(QStringLiteral("rawStatus"), status);
    out.insert(QStringLiteral("score"), m.value("averageScore").toInt());
    out.insert(QStringLiteral("scoreFormatted"), m.value("averageScore").toInt() > 0 ? QStringLiteral("★ %1%").arg(m.value("averageScore").toInt()) : QString());
    out.insert(QStringLiteral("popularity"), m.value("popularity").toInt());
    out.insert(QStringLiteral("favourites"), m.value("favourites").toInt());
    out.insert(QStringLiteral("genres"), m.value("genres").toVariant());
    out.insert(QStringLiteral("tags"), tags);
    out.insert(QStringLiteral("year"), m.value("startDate").toObject().value("year").toInt());
    out.insert(QStringLiteral("chapters"), m.value("chapters").toInt());
    out.insert(QStringLiteral("volumes"), m.value("volumes").toInt());
    out.insert(QStringLiteral("episodes"), m.value("episodes").toInt());
    out.insert(QStringLiteral("banner"), m.value("bannerImage").toString());
    out.insert(QStringLiteral("cover"), cover.value("extraLarge").toString(cover.value("large").toString()));
    out.insert(QStringLiteral("color"), cover.value("color").toString());
    out.insert(QStringLiteral("hasUserEntry"), !entry.isEmpty());
    out.insert(QStringLiteral("userStatus"), userStatus);
    out.insert(QStringLiteral("userScore"), userScore);
    out.insert(QStringLiteral("userProgress"), userProgress);

    if (token.isEmpty() && !out.isEmpty()) {
        Database::cachePut(cacheKey, QJsonDocument(QJsonObject::fromVariantMap(out)).toJson(QJsonDocument::Compact), 86400);
    }
    return out;
}

// What the series' source stored about it: a Comick catalogue entry
// ("hid", "catalogTitle") and AniList and MyAnimeList ids ("anilist",
// "mal") from sites that link them, such as MangaDex.
QJsonObject seriesExtra(const QString &seriesId) {
    return QJsonDocument::fromJson(
               Database::scalar(QStringLiteral("SELECT extra FROM series WHERE id = ?"), {seriesId}).toString().toUtf8())
        .object();
}

// How far the series is: the highest chapter or episode number finished.
int progressOf(const QString &seriesId) {
    int best = 0;
    int count = 0;
    for (const auto &value : Database::query(QStringLiteral("SELECT unit_key, unit_number FROM progress WHERE series_id = ? AND completed = 1"),
                                             {seriesId})) {
        const auto row = value.toMap();
        ++count;
        bool ok = false;
        auto number = row.value("unit_number").toString().toDouble(&ok);
        if (!ok)
            number = row.value("unit_key").toString().toDouble(&ok);
        if (ok && number < 100000)
            best = std::max(best, int(number));
    }
    return best > 0 ? best : count;
}

void updateAniList(const QString &seriesId, const QString &media, const QString &title, int progress) {
    const auto account = Database::pref(QStringLiteral("tracker.anilist")).toMap();
    if (account.isEmpty())
        return;
    const auto token = account.value("token").toString();
    int id = allLinks().value(seriesId).toMap().value("anilist").toMap().value("id").toInt();
    if (id <= 0 && media == QLatin1String("anime"))
        id = seriesId.section(QLatin1Char(':'), 2).toInt();
    if (id <= 0 && media == QLatin1String("comic")) {
        id = seriesExtra(seriesId).value("anilist").toInt();
        if (id > 0)
            saveLink(seriesId, QStringLiteral("anilist"), id, title);
    }
    if (id <= 0) {
        const auto match = exactMatch(anilistSearch(media, title), title);
        if (match.isEmpty())
            return;
        id = match.value("id").toInt();
        saveLink(seriesId, QStringLiteral("anilist"), id, match.value("title").toString());
    }
    const auto current = anilistQuery(QStringLiteral("query ($id: Int) { Media(id: $id) { episodes chapters mediaListEntry { progress status } } }"),
                                      {{"id", id}}, token)
                             .value("Media")
                             .toObject();
    const auto entry = current.value("mediaListEntry").toObject();
    if (entry.value("progress").toInt() >= progress)
        return;
    const int total = current.value(media == QLatin1String("anime") ? "episodes" : "chapters").toInt();
    const auto status = total > 0 && progress >= total ? QStringLiteral("COMPLETED") : QStringLiteral("CURRENT");
    anilistQuery(QStringLiteral("mutation ($id: Int, $progress: Int, $status: MediaListStatus) {"
                                " SaveMediaListEntry(mediaId: $id, progress: $progress, status: $status) { id } }"),
                 {{"id", id}, {"progress", progress}, {"status", status}}, token);
}

void updateMal(const QString &seriesId, const QString &media, const QString &title, int progress) {
    if (Database::pref(QStringLiteral("tracker.mal")).toMap().isEmpty())
        return;
    const bool anime = media == QLatin1String("anime");
    const auto links = allLinks().value(seriesId).toMap();
    int id = links.value("mal").toMap().value("id").toInt();
    const auto extra = media == QLatin1String("comic") ? seriesExtra(seriesId) : QJsonObject();
    if (id <= 0)
        id = extra.value("mal").toInt();
    if (id <= 0) {
        // Through AniList, which knows MyAnimeList's ids.
        int anilistId = links.value("anilist").toMap().value("id").toInt();
        if (anilistId <= 0 && anime)
            anilistId = seriesId.section(QLatin1Char(':'), 2).toInt();
        if (anilistId <= 0)
            anilistId = extra.value("anilist").toInt();
        if (anilistId > 0)
            id = anilistQuery(QStringLiteral("query ($id: Int) { Media(id: $id) { idMal } }"), {{"id", anilistId}})
                     .value("Media").toObject().value("idMal").toInt();
        if (id <= 0) {
            const auto match = exactMatch(malSearch(media, title), title);
            if (match.isEmpty())
                return;
            id = match.value("id").toInt();
        }
    }
    if (!links.contains(QStringLiteral("mal")))
        saveLink(seriesId, QStringLiteral("mal"), id, title);
    const auto kind = anime ? QStringLiteral("anime") : QStringLiteral("manga");
    const auto info = malCall("GET", QStringLiteral("/%1/%2?fields=my_list_status,%3").arg(kind).arg(id).arg(anime ? "num_episodes" : "num_chapters"));
    if (!info.ok())
        return;
    const auto json = info.json().object();
    const auto mine = json.value("my_list_status").toObject();
    if (mine.value(anime ? "num_episodes_watched" : "num_chapters_read").toInt() >= progress)
        return;
    const int total = json.value(anime ? "num_episodes" : "num_chapters").toInt();
    const bool done = total > 0 && progress >= total;
    malCall("PATCH", QStringLiteral("/%1/%2/my_list_status").arg(kind).arg(id),
            {{anime ? "num_watched_episodes" : "num_chapters_read", QString::number(progress)},
             {"status", done ? QStringLiteral("completed") : anime ? QStringLiteral("watching") : QStringLiteral("reading")}});
}

void updateComick(const QString &seriesId, const QString &media, const QString &title, int progress) {
    // Comick lists manga and anime.
    if (Database::pref(QStringLiteral("tracker.comick")).toMap().isEmpty() || media == QLatin1String("novel"))
        return;
    const auto links = allLinks().value(seriesId).toMap();
    auto hid = links.value("comick").toMap().value("id").toString();
    // A comic opened from Comick's catalogue already knows its entry there.
    const auto extra = media == QLatin1String("comic") ? seriesExtra(seriesId) : QJsonObject();
    if (hid.isEmpty() && media == QLatin1String("comic")) {
        hid = extra.value("hid").toString();
        if (!hid.isEmpty())
            saveLink(seriesId, QStringLiteral("comick"), hid, extra.value("catalogTitle").toString());
    }
    if (hid.isEmpty()) {
        int anilistId = links.value("anilist").toMap().value("id").toInt();
        if (anilistId <= 0 && media == QLatin1String("anime"))
            anilistId = seriesId.section(QLatin1Char(':'), 2).toInt();
        if (anilistId <= 0)
            anilistId = extra.value("anilist").toInt();
        const auto candidates = comickSearch(media, title);
        QVariantMap match;
        // By AniList entry when both know it, else by title.
        for (const auto &value : candidates)
            if (anilistId > 0 && value.toMap().value("anilist").toInt() == anilistId)
                match = value.toMap();
        if (match.isEmpty())
            match = exactMatch(candidates, title);
        if (match.isEmpty())
            return;
        hid = match.value("id").toString();
        saveLink(seriesId, QStringLiteral("comick"), hid, match.value("title").toString());
    }
    const auto path = QStringLiteral("/me/library/") + QString::fromLatin1(QUrl::toPercentEncoding(hid));
    const auto current = comickCall("GET", path);
    int status = 1;
    if (current.ok()) {
        const auto entry = current.json().object().value("data").toObject();
        if (entry.value("progress").toObject().value("number").toString().toDouble() >= progress)
            return;
        const int total = entry.value("total_units").toInt();
        status = total > 0 && progress >= total ? 2 : entry.value("status").toInt() == 2 ? 2 : 1;
    } else if (current.status != 404) {
        return;
    }
    comickCall("PUT", path, QJsonObject{{"status", status}, {"progress", QJsonObject{{"number", QString::number(progress)}}}});
}

} // namespace

Trackers::Trackers(QObject *parent) : QObject(parent) {}

Trackers *Trackers::instance() {
    static auto *trackers = [] {
        auto *created = new Trackers;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return trackers;
}

bool Trackers::anilistAvailable() const { return !kAniListId.isEmpty(); }
bool Trackers::malAvailable() const { return !kMalId.isEmpty(); }
bool Trackers::comickAvailable() const { return !kComickId.isEmpty(); }

QVariantMap Trackers::anilist() const {
    const auto account = Database::pref(QStringLiteral("tracker.anilist")).toMap();
    return account.isEmpty() ? QVariantMap() : QVariantMap{{"name", account.value("name")}, {"avatar", account.value("avatar")}};
}

QVariantMap Trackers::mal() const {
    const auto account = Database::pref(QStringLiteral("tracker.mal")).toMap();
    return account.isEmpty() ? QVariantMap() : QVariantMap{{"name", account.value("name")}, {"avatar", account.value("avatar")}};
}

QVariantMap Trackers::comick() const {
    const auto account = Database::pref(QStringLiteral("tracker.comick")).toMap();
    return account.isEmpty() ? QVariantMap() : QVariantMap{{"name", account.value("name")}, {"avatar", account.value("avatar")}};
}

void Trackers::anilistSignIn() const {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://anilist.co/api/v2/oauth/authorize?client_id=%1&response_type=token").arg(kAniListId)));
}

Task *Trackers::anilistUseToken(const QString &pasted) {
    // The whole address bar works too: …#access_token=…&token_type=…
    auto token = pasted.trimmed();
    if (const auto at = token.indexOf(QLatin1String("access_token=")); at >= 0)
        token = token.mid(at + 13).section(QLatin1Char('&'), 0, 0);
    auto *task = Task::run([token]() -> QVariant {
        if (token.isEmpty())
            throw Error(QStringLiteral("Paste the token AniList showed."));
        const auto viewer = anilistQuery(QStringLiteral("query { Viewer { id name avatar { medium } } }"), {}, token).value("Viewer").toObject();
        if (viewer.isEmpty())
            throw Error(QStringLiteral("AniList did not accept that token."));
        Database::setPref(QStringLiteral("tracker.anilist"), QVariantMap{{"token", token},
                                                                         {"userId", viewer.value("id").toInt()},
                                                                         {"name", viewer.value("name").toString()},
                                                                         {"avatar", viewer.value("avatar").toObject().value("medium").toString()}});
        return QVariantMap{{"name", viewer.value("name").toString()}};
    });
    task->then(this, [this](const QVariant &) { emit changed(); });
    return task;
}

std::shared_ptr<void> Trackers::listen(const QString &path, QString *problem) {
    // One sign-in at a time: a new one ends the one still waiting.
    if (auto previous = std::static_pointer_cast<Pending>(m_pending)) {
        QMutexLocker lock(&previous->mutex);
        previous->error = QStringLiteral("Cancelled");
        previous->arrived.wakeAll();
    }
    if (m_redirect)
        m_redirect->deleteLater();
    m_state = QUuid::createUuid().toString(QUuid::Id128);
    auto pending = std::make_shared<Pending>();
    m_pending = pending;
    m_redirect = new QTcpServer(this);
    if (!m_redirect->listen(QHostAddress::LocalHost, kRedirectPort)) {
        *problem = QStringLiteral("Another program is using port %1, which signing in needs.").arg(kRedirectPort);
        return {};
    }
    connect(m_redirect, &QTcpServer::newConnection, this, [this, pending, path, server = m_redirect] {
        while (auto *socket = server->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket, pending, path] {
                const auto line = QString::fromLatin1(socket->readLine(8192));
                const QUrl url(QStringLiteral("http://127.0.0.1") + line.section(QLatin1Char(' '), 1, 1));
                const QUrlQuery query(url);
                const bool ours = url.path() == path && query.queryItemValue(QStringLiteral("state")) == m_state;
                const bool denied = ours && !query.queryItemValue(QStringLiteral("error")).isEmpty();
                const auto page = QByteArray(!ours ? "Nothing to do here."
                                             : denied ? "Sign-in was cancelled. You can go back to SCANCORE."
                                                      : "Signed in. You can go back to SCANCORE.");
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
                              "<!doctype html><meta charset=utf-8><title>SCANCORE</title>"
                              "<body style=\"font:16px system-ui;padding:3em;background:#111;color:#eee\">" +
                              page + "</body>");
                socket->disconnectFromHost();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                if (!ours)
                    return;
                QMutexLocker lock(&pending->mutex);
                pending->code = query.queryItemValue(QStringLiteral("code"));
                pending->error = query.queryItemValue(QStringLiteral("error"));
                pending->arrived.wakeAll();
            });
        }
    });
    return pending;
}

void Trackers::watch(Task *task) {
    connect(task, &Task::changed, this, [this, task] {
        if (task->loading())
            return;
        if (m_redirect)
            m_redirect->deleteLater();
        if (task->ok())
            emit changed();
    });
}

Task *Trackers::malSignIn() {
    QString problem;
    const auto pending = std::static_pointer_cast<Pending>(listen(QStringLiteral("/mal"), &problem));
    if (!pending)
        return Task::run([problem]() -> QVariant { throw Error(problem); });
    // PKCE with the plain method, which is the one MyAnimeList supports.
    const auto verifier = randomText(96);
    QUrl authorize(QStringLiteral("https://myanimelist.net/v1/oauth2/authorize"));
    authorize.setQuery(QUrlQuery{{"response_type", "code"},
                                 {"client_id", kMalId},
                                 {"code_challenge", verifier},
                                 {"code_challenge_method", "plain"},
                                 {"state", m_state},
                                 {"redirect_uri", kMalRedirect}});
    QDesktopServices::openUrl(authorize);

    auto *task = Task::run([pending, verifier]() -> QVariant {
        const auto code = waitForCode(pending, QStringLiteral("MyAnimeList"));
        const auto response = Http::postForm(QUrl(QStringLiteral("https://myanimelist.net/v1/oauth2/token")),
                                             {{"client_id", kMalId},
                                              {"grant_type", "authorization_code"},
                                              {"code", code},
                                              {"code_verifier", verifier},
                                              {"redirect_uri", kMalRedirect}});
        if (!response.ok())
            throw Error(QStringLiteral("MyAnimeList did not accept the sign-in (%1).").arg(response.status));
        const auto json = response.json().object();
        Database::setPref(QStringLiteral("tracker.mal"), QVariantMap{{"access", json.value("access_token").toString()},
                                                                     {"refresh", json.value("refresh_token").toString()},
                                                                     {"expires", Database::now() + json.value("expires_in").toInteger(3600)}});
        const auto me = malCall("GET", QStringLiteral("/users/@me")).json().object();
        auto account = Database::pref(QStringLiteral("tracker.mal")).toMap();
        account.insert("name", me.value("name").toString());
        account.insert("avatar", me.value("picture").toString());
        Database::setPref(QStringLiteral("tracker.mal"), account);
        return QVariantMap{{"name", me.value("name").toString()}};
    });
    watch(task);
    return task;
}

Task *Trackers::comickSignIn() {
    QString problem;
    const auto pending = std::static_pointer_cast<Pending>(listen(QStringLiteral("/comick"), &problem));
    if (!pending)
        return Task::run([problem]() -> QVariant { throw Error(problem); });
    // PKCE with SHA-256, which Comick requires.
    const auto verifier = randomText(64);
    const auto challenge = QString::fromLatin1(QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256)
                                                   .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    QUrl authorize(kComickAuth + QStringLiteral("/authorize"));
    authorize.setQuery(QUrlQuery{{"response_type", "code"},
                                 {"client_id", kComickId},
                                 {"redirect_uri", kComickRedirect},
                                 {"scope", "library:read library:write offline_access"},
                                 {"resource", kComickApi},
                                 {"state", m_state},
                                 {"code_challenge", challenge},
                                 {"code_challenge_method", "S256"}});
    QDesktopServices::openUrl(authorize);

    auto *task = Task::run([pending, verifier]() -> QVariant {
        const auto code = waitForCode(pending, QStringLiteral("Comick"));
        const auto response = Http::postForm(QUrl(kComickAuth + QStringLiteral("/token")),
                                             {{"grant_type", "authorization_code"},
                                              {"client_id", kComickId},
                                              {"code", code},
                                              {"redirect_uri", kComickRedirect},
                                              {"code_verifier", verifier},
                                              {"resource", kComickApi}},
                                             {{"Accept", "application/json"}});
        if (!response.ok())
            throw Error(QStringLiteral("Comick did not accept the sign-in (%1).").arg(response.status));
        const auto json = response.json().object();
        if (!json.value("scope").toString().contains(QLatin1String("library:write")))
            throw Error(QStringLiteral("SCANCORE needs permission to update your Comick library. Connect again and allow it."));
        Database::setPref(QStringLiteral("tracker.comick"),
                          QVariantMap{{"access", json.value("access_token").toString()},
                                      {"refresh", json.value("refresh_token").toString()},
                                      {"expires", json.contains("expires_at") ? json.value("expires_at").toInteger()
                                                                              : Database::now() + json.value("expires_in").toInteger(900)}});
        const auto me = comickCall("GET", QStringLiteral("/me")).json().object();
        auto account = Database::pref(QStringLiteral("tracker.comick")).toMap();
        account.insert("name", me.value("username").toString());
        account.insert("avatar", me.value("avatar_url").toString());
        Database::setPref(QStringLiteral("tracker.comick"), account);
        return QVariantMap{{"name", me.value("username").toString()}};
    });
    watch(task);
    return task;
}

void Trackers::signOut(const QString &service) {
    if (service == QLatin1String("mal") || service == QLatin1String("comick") || service == QLatin1String("anilist"))
        Database::setPref(QStringLiteral("tracker.") + service, QVariantMap());
    emit changed();
}

Task *Trackers::search(const QString &service, const QString &media, const QString &title) {
    return Task::run([service, media, title]() -> QVariant {
        return service == QLatin1String("mal")      ? malSearch(media, title)
               : service == QLatin1String("comick") ? comickSearch(media, title)
                                                    : anilistSearch(media, title);
    });
}

QVariantMap Trackers::links(const QString &seriesId) const {
    auto entry = allLinks().value(seriesId).toMap();
    // Anime are on AniList already, by their id.
    if (!entry.contains(QStringLiteral("anilist")) && seriesId.startsWith(QLatin1String("anime:anilist:")))
        entry.insert(QStringLiteral("anilist"), QVariantMap{{"id", seriesId.section(QLatin1Char(':'), 2).toInt()}, {"auto", true}});
    return entry;
}

void Trackers::link(const QString &seriesId, const QString &service, const QVariant &remoteId, const QString &title) {
    saveLink(seriesId, service, remoteId, title);
    emit changed();
    unitFinished(seriesId);
}

void Trackers::unlink(const QString &seriesId, const QString &service) {
    saveLink(seriesId, service, QVariant(), {});
    emit changed();
}

Task *Trackers::anilistDetails(const QString &seriesId, const QString &media, const QString &title) {
    return Task::run([seriesId, media, title]() -> QVariant {
        int id = allLinks().value(seriesId).toMap().value("anilist").toMap().value("id").toInt();
        if (id <= 0 && media == QLatin1String("anime")) {
            if (seriesId.startsWith(QLatin1String("anime:anilist:")))
                id = seriesId.section(QLatin1Char(':'), 2).toInt();
        }
        if (id <= 0 && media == QLatin1String("comic")) {
            id = seriesExtra(seriesId).value("anilist").toInt();
            if (id > 0)
                saveLink(seriesId, QStringLiteral("anilist"), id, title);
        }
        if (id <= 0 && !title.trimmed().isEmpty()) {
            try {
                const auto candidates = anilistSearch(media, title);
                const auto match = bestMatch(candidates, title);
                if (!match.isEmpty() && match.value("id").toInt() > 0) {
                    id = match.value("id").toInt();
                    saveLink(seriesId, QStringLiteral("anilist"), id, match.value("title").toString());
                }
            } catch (const Error &e) {
                qWarning().noquote() << "AniList lookup error:" << e.message();
            }
        }
        if (id <= 0)
            return QVariantMap{};

        const auto account = Database::pref(QStringLiteral("tracker.anilist")).toMap();
        const auto token = account.value("token").toString();
        try {
            return anilistMediaDetails(id, token);
        } catch (const Error &e) {
            qWarning().noquote() << "AniList details error:" << e.message();
            return QVariantMap{};
        }
    });
}

QVariantList Trackers::comickLibrary() {
    QVariantList out;
    QString cursor;
    do {
        QUrlQuery query{{"limit", "100"}};
        if (!cursor.isEmpty())
            query.addQueryItem(QStringLiteral("cursor"), cursor);
        const auto response = comickCall("GET", QStringLiteral("/me/library?") + query.toString(QUrl::FullyEncoded));
        if (!response.ok())
            throw Error(QStringLiteral("Comick answered %1.").arg(response.status), response.status);
        const auto page = response.json().object();
        for (const auto &value : page.value("data").toArray())
            out.append(value.toObject().toVariantMap());
        cursor = page.value("next_cursor").toString();
    } while (!cursor.isEmpty() && !CancelScope::cancelled());
    return out;
}

int Trackers::comickAniListId(const QString &hid) {
    const auto response = comickCall("GET", QStringLiteral("/titles/") + QString::fromLatin1(QUrl::toPercentEncoding(hid)));
    return response.ok() ? response.json().object().value("data").toObject().value("external_ids").toObject().value("anilist").toString().toInt() : 0;
}

void Trackers::remember(const QString &seriesId, const QString &service, const QVariant &remoteId, const QString &title) {
    saveLink(seriesId, service, remoteId, title);
}

void Trackers::unitFinished(const QString &seriesId) {
    if (Database::pref(QStringLiteral("tracker.anilist")).toMap().isEmpty() &&
        Database::pref(QStringLiteral("tracker.mal")).toMap().isEmpty() &&
        Database::pref(QStringLiteral("tracker.comick")).toMap().isEmpty())
        return;
    runInBackground([seriesId] {
        const auto row = Database::one(QStringLiteral("SELECT media, title FROM series WHERE id = ?"), {seriesId});
        const auto media = row.value("media").toString();
        if (media.isEmpty() || seriesId.contains(QLatin1String(":local:")))
            return;
        const int progress = progressOf(seriesId);
        if (progress <= 0)
            return;
        const auto title = row.value("title").toString();
        try {
            updateAniList(seriesId, media, title, progress);
        } catch (const Error &e) {
            qWarning().noquote() << "AniList:" << e.message();
        }
        try {
            updateMal(seriesId, media, title, progress);
        } catch (const Error &e) {
            qWarning().noquote() << "MyAnimeList:" << e.message();
        }
        try {
            updateComick(seriesId, media, title, progress);
        } catch (const Error &e) {
            qWarning().noquote() << "Comick:" << e.message();
        }
        QMetaObject::invokeMethod(instance(), [] { emit instance()->changed(); });
    });
}

} // namespace sc
