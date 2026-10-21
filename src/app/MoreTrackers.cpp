#include "TrackerCommon.h"

#include "core/Database.h"
#include "core/Http.h"
#include "core/Task.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QRegularExpression>
#include <QThread>
#include <QUrlQuery>

#ifndef SCANCORE_SHIKIMORI_CLIENT_ID
#define SCANCORE_SHIKIMORI_CLIENT_ID ""
#endif
#ifndef SCANCORE_SHIKIMORI_CLIENT_SECRET
#define SCANCORE_SHIKIMORI_CLIENT_SECRET ""
#endif
#ifndef SCANCORE_SIMKL_CLIENT_ID
#define SCANCORE_SIMKL_CLIENT_ID ""
#endif

namespace sc::tracking {

namespace {

// Kitsu's own client for third-party apps (published in its API docs).
const QString kKitsuClient = QStringLiteral("dd031b32d2f56c990b1425efe6c42ad847e7fe3ab46bf1299f05ecd856bdb7dd");
const QString kKitsuSecret = QStringLiteral("54d7307928f63414defd96399fc31ba847961ceaecef3a5fd93144e960c0e151");
const QString kKitsu = QStringLiteral("https://kitsu.app/api");
const QString kMangaUpdates = QStringLiteral("https://api.mangaupdates.com/v1");
const QString kShikimori = QStringLiteral("https://shikimori.io");
const QString kShikimoriId = QStringLiteral(SCANCORE_SHIKIMORI_CLIENT_ID);
const QString kShikimoriSecret = QStringLiteral(SCANCORE_SHIKIMORI_CLIENT_SECRET);
const QString kBangumi = QStringLiteral("https://api.bgm.tv");
const QString kSimkl = QStringLiteral("https://api.simkl.com");
const QString kSimklId = QStringLiteral(SCANCORE_SIMKL_CLIENT_ID);

QMutex g_refreshMutex;

QVariantMap account(const QString &key) { return Database::pref(QStringLiteral("tracker.") + key).toMap(); }
void setAccount(const QString &key, const QVariantMap &value) { Database::setPref(QStringLiteral("tracker.") + key, value); }

Error signedOut(const QString &label) {
    return Error(QStringLiteral("%1 signed SCANCORE out. Connect again in Settings → Connections.").arg(label), 401);
}

// Shikimori and Bangumi ask every app to say who it is.
QByteArray userAgent() {
    return "SCANCORE/" + QCoreApplication::applicationVersion().toUtf8() + " (https://github.com/medium-gh/scancore)";
}

QString encoded(const QString &text) { return QString::fromLatin1(QUrl::toPercentEncoding(text)); }

Response call(const QByteArray &method, const QString &url, Headers headers, const QJsonValue &body = {}) {
    Request request;
    request.method = method;
    request.url = QUrl(url);
    if (!body.isUndefined() && !body.isNull()) {
        request.body = body.isArray() ? QJsonDocument(body.toArray()).toJson(QJsonDocument::Compact)
                                      : QJsonDocument(body.toObject()).toJson(QJsonDocument::Compact);
        bool typed = false;
        for (const auto &[name, value] : headers)
            typed = typed || name.compare("Content-Type", Qt::CaseInsensitive) == 0;
        if (!typed)
            headers.append(qMakePair(QByteArray("Content-Type"), QByteArray("application/json")));
    }
    request.headers = headers;
    return Http::send(request);
}

QJsonObject json(const Response &response) { return response.json().object(); }

QStringList cleanTitles(QStringList titles) {
    titles.removeAll(QString());
    titles.removeDuplicates();
    return titles;
}

QString detailOf(const QString &kind, const QString &year) {
    QStringList parts{kind, year};
    parts.removeAll(QString());
    return parts.join(QStringLiteral(" · "));
}

// ------------------------------------------------------------------ Kitsu

QString kitsuToken() {
    QMutexLocker lock(&g_refreshMutex);
    auto a = account(QStringLiteral("kitsu"));
    if (a.isEmpty())
        return {};
    if (a.value("expires").toLongLong() > Database::now() + 3600)
        return a.value("access").toString();
    const auto response = Http::postForm(QUrl(kKitsu + QStringLiteral("/oauth/token")),
                                         {{"grant_type", "refresh_token"},
                                          {"refresh_token", a.value("refresh").toString()},
                                          {"client_id", kKitsuClient},
                                          {"client_secret", kKitsuSecret}});
    if (!response.ok())
        throw signedOut(QStringLiteral("Kitsu"));
    const auto j = json(response);
    a.insert("access", j.value("access_token").toString());
    a.insert("refresh", j.value("refresh_token").toString());
    a.insert("expires", Database::now() + j.value("expires_in").toInteger(86400));
    setAccount(QStringLiteral("kitsu"), a);
    return a.value("access").toString();
}

Response kitsuCall(const QByteArray &method, const QString &path, const QJsonObject &body = {}) {
    Headers headers{{"Accept", "application/vnd.api+json"}, {"Content-Type", "application/vnd.api+json"}};
    if (const auto token = kitsuToken(); !token.isEmpty())
        headers.append({"Authorization", "Bearer " + token.toUtf8()});
    const auto response = call(method, kKitsu + QStringLiteral("/edge") + path, headers, body.isEmpty() ? QJsonValue() : QJsonValue(body));
    if (response.status == 401)
        throw signedOut(QStringLiteral("Kitsu"));
    return response;
}

QVariantList kitsuSearch(const QString &media, const QString &title) {
    const bool anime = media == QLatin1String("anime");
    const auto response = kitsuCall("GET", QStringLiteral("/%1?filter[text]=%2&page[limit]=10").arg(anime ? "anime" : "manga", encoded(title.left(80))));
    if (!response.ok())
        throw Error(QStringLiteral("Kitsu answered %1.").arg(response.status), response.status);
    QVariantList out;
    for (const auto &value : json(response).value("data").toArray()) {
        const auto item = value.toObject();
        const auto a = item.value("attributes").toObject();
        const auto subtype = a.value("subtype").toString();
        if (!anime && (media == QLatin1String("novel")) != (subtype == QLatin1String("novel")))
            continue;
        const auto t = a.value("titles").toObject();
        QStringList titles{a.value("canonicalTitle").toString(), t.value("en").toString(), t.value("en_jp").toString(),
                           t.value("en_us").toString(), t.value("ja_jp").toString(), t.value("ko_kr").toString()};
        for (const auto &s : a.value("abbreviatedTitles").toArray())
            titles.append(s.toString());
        titles = cleanTitles(titles);
        out.append(QVariantMap{{"id", item.value("id").toString().toInt()},
                               {"title", titles.value(0)},
                               {"titles", titles},
                               {"detail", detailOf(subtype.toUpper(), a.value("startDate").toString().left(4))},
                               {"cover", a.value("posterImage").toObject().value("small").toString()},
                               {"total", a.value(anime ? "episodeCount" : "chapterCount").toInt()}});
    }
    return out;
}

void kitsuPush(const QVariant &id, const QString &media, int progress) {
    const auto a = account(QStringLiteral("kitsu"));
    if (a.isEmpty())
        return;
    const bool anime = media == QLatin1String("anime");
    const auto kind = anime ? QStringLiteral("anime") : QStringLiteral("manga");
    const auto found = kitsuCall("GET", QStringLiteral("/library-entries?filter[userId]=%1&filter[%2Id]=%3&include=%2")
                                            .arg(a.value("userId").toString(), kind, id.toString()));
    if (!found.ok())
        throw Error(QStringLiteral("Kitsu answered %1.").arg(found.status), found.status);
    const auto body = json(found);
    const auto entry = body.value("data").toArray().first().toObject();
    int total = 0;
    for (const auto &inc : body.value("included").toArray())
        total = inc.toObject().value("attributes").toObject().value(anime ? "episodeCount" : "chapterCount").toInt();
    if (!entry.isEmpty() && entry.value("attributes").toObject().value("progress").toInt() >= progress)
        return;
    const QJsonObject attributes{{"progress", progress},
                                 {"status", total > 0 && progress >= total ? "completed" : "current"}};
    if (!entry.isEmpty()) {
        kitsuCall("PATCH", QStringLiteral("/library-entries/") + entry.value("id").toString(),
                  {{"data", QJsonObject{{"id", entry.value("id")}, {"type", "libraryEntries"}, {"attributes", attributes}}}});
        return;
    }
    kitsuCall("POST", QStringLiteral("/library-entries"),
              {{"data", QJsonObject{{"type", "libraryEntries"},
                                    {"attributes", attributes},
                                    {"relationships", QJsonObject{
                                         {"user", QJsonObject{{"data", QJsonObject{{"type", "users"}, {"id", a.value("userId").toString()}}}}},
                                         {kind, QJsonObject{{"data", QJsonObject{{"type", kind}, {"id", id.toString()}}}}}}}}}});
}

QVariantMap kitsuSignIn(const QString &user, const QString &password) {
    const auto response = Http::postForm(QUrl(kKitsu + QStringLiteral("/oauth/token")),
                                         {{"grant_type", "password"},
                                          {"username", user},
                                          {"password", password},
                                          {"client_id", kKitsuClient},
                                          {"client_secret", kKitsuSecret}});
    if (response.status == 400 || response.status == 401)
        throw Error(QStringLiteral("Kitsu did not accept that email and password."));
    if (!response.ok())
        throw Error(QStringLiteral("Kitsu answered %1.").arg(response.status), response.status);
    const auto j = json(response);
    setAccount(QStringLiteral("kitsu"), {{"access", j.value("access_token").toString()},
                                         {"refresh", j.value("refresh_token").toString()},
                                         {"expires", Database::now() + j.value("expires_in").toInteger(86400)}});
    const auto me = json(kitsuCall("GET", QStringLiteral("/users?filter[self]=true"))).value("data").toArray().first().toObject();
    if (me.isEmpty())
        throw Error(QStringLiteral("Kitsu did not say who you are. Try again."));
    auto a = account(QStringLiteral("kitsu"));
    const auto attributes = me.value("attributes").toObject();
    a.insert("userId", me.value("id").toString());
    a.insert("name", attributes.value("name").toString());
    a.insert("avatar", attributes.value("avatar").toObject().value("small").toString());
    setAccount(QStringLiteral("kitsu"), a);
    return {{"name", a.value("name")}};
}

// ---------------------------------------------------------- MangaUpdates

Response muCall(const QByteArray &method, const QString &path, const QJsonValue &body = {}) {
    Headers headers{{"Accept", "application/json"}};
    if (const auto token = account(QStringLiteral("mangaupdates")).value("token").toString(); !token.isEmpty())
        headers.append({"Authorization", "Bearer " + token.toUtf8()});
    const auto response = call(method, kMangaUpdates + path, headers, body);
    if (response.status == 401)
        throw signedOut(QStringLiteral("MangaUpdates"));
    return response;
}

QVariantList muSearch(const QString &media, const QString &title) {
    const auto response = muCall("POST", QStringLiteral("/series/search"), QJsonObject{{"search", title.left(100)}, {"perpage", 15}});
    if (!response.ok())
        throw Error(QStringLiteral("MangaUpdates answered %1.").arg(response.status), response.status);
    QVariantList out;
    for (const auto &value : json(response).value("results").toArray()) {
        const auto hit = value.toObject();
        const auto r = hit.value("record").toObject();
        const auto type = r.value("type").toString();
        const bool novel = type == QLatin1String("Novel");
        if ((media == QLatin1String("novel")) != novel || type == QLatin1String("Drama CD"))
            continue;
        // Novels are named "… (Novel)" there.
        static const QRegularExpression novelSuffix(QStringLiteral("\\s*\\((Novel|Light Novel)\\)$"));
        auto bare = r.value("title").toString();
        bare.remove(novelSuffix);
        const auto titles = cleanTitles({r.value("title").toString(), hit.value("hit_title").toString(), bare});
        out.append(QVariantMap{{"id", QString::number(r.value("series_id").toInteger())},
                               {"title", titles.value(0)},
                               {"titles", titles},
                               {"detail", detailOf(type, r.value("year").toString())},
                               {"cover", r.value("image").toObject().value("url").toObject().value("thumb").toString()},
                               {"total", 0}});
    }
    return out;
}

void muPush(const QVariant &id, const QString &, int progress) {
    if (account(QStringLiteral("mangaupdates")).isEmpty())
        return;
    // Series ids are larger than a double holds exactly as text, so they are
    // kept as text and sent as numbers.
    const auto series = QJsonObject{{"id", id.toString().toLongLong()}};
    const auto current = muCall("GET", QStringLiteral("/lists/series/") + id.toString());
    if (current.status == 404) {
        muCall("POST", QStringLiteral("/lists/series"),
               QJsonArray{QJsonObject{{"series", series}, {"list_id", 0}, {"status", QJsonObject{{"chapter", progress}}}}});
        return;
    }
    if (!current.ok())
        throw Error(QStringLiteral("MangaUpdates answered %1.").arg(current.status), current.status);
    const auto entry = json(current);
    if (entry.value("status").toObject().value("chapter").toInt() >= progress)
        return;
    // Reading stays reading; a wish-listed series moves to reading.
    const int list = entry.value("list_id").toInt() == 1 ? 0 : entry.value("list_id").toInt();
    muCall("POST", QStringLiteral("/lists/series/update"),
           QJsonArray{QJsonObject{{"series", series}, {"list_id", list}, {"status", QJsonObject{{"chapter", progress}}}}});
}

QVariantMap muSignIn(const QString &user, const QString &password) {
    const auto response = call("PUT", kMangaUpdates + QStringLiteral("/account/login"), {{"Accept", "application/json"}},
                               QJsonObject{{"username", user}, {"password", password}});
    if (response.status == 400 || response.status == 401)
        throw Error(QStringLiteral("MangaUpdates did not accept that user name and password."));
    if (!response.ok())
        throw Error(QStringLiteral("MangaUpdates answered %1.").arg(response.status), response.status);
    const auto context = json(response).value("context").toObject();
    setAccount(QStringLiteral("mangaupdates"), {{"token", context.value("session_token").toString()}});
    const auto me = json(muCall("GET", QStringLiteral("/account/profile")));
    setAccount(QStringLiteral("mangaupdates"), {{"token", context.value("session_token").toString()},
                                                {"name", me.value("username").toString(user)},
                                                {"avatar", me.value("avatar").toObject().value("url").toString()}});
    return {{"name", me.value("username").toString(user)}};
}

// -------------------------------------------------------------- Shikimori

void saveShikimori(const QJsonObject &j) {
    auto a = account(QStringLiteral("shikimori"));
    a.insert("access", j.value("access_token").toString());
    a.insert("refresh", j.value("refresh_token").toString());
    a.insert("expires", j.value("created_at").toInteger(Database::now()) + j.value("expires_in").toInteger(86400));
    setAccount(QStringLiteral("shikimori"), a);
}

QString shikimoriToken() {
    QMutexLocker lock(&g_refreshMutex);
    const auto a = account(QStringLiteral("shikimori"));
    if (a.isEmpty())
        return {};
    if (a.value("expires").toLongLong() > Database::now() + 120)
        return a.value("access").toString();
    const auto response = Http::postForm(QUrl(kShikimori + QStringLiteral("/oauth/token")),
                                         {{"grant_type", "refresh_token"},
                                          {"client_id", kShikimoriId},
                                          {"client_secret", kShikimoriSecret},
                                          {"refresh_token", a.value("refresh").toString()}},
                                         {{"User-Agent", userAgent()}});
    if (!response.ok())
        throw signedOut(QStringLiteral("Shikimori"));
    saveShikimori(json(response));
    return account(QStringLiteral("shikimori")).value("access").toString();
}

Response shikimoriCall(const QByteArray &method, const QString &path, const QJsonObject &body = {}) {
    Headers headers{{"Accept", "application/json"}, {"User-Agent", userAgent()}};
    if (const auto token = shikimoriToken(); !token.isEmpty())
        headers.append({"Authorization", "Bearer " + token.toUtf8()});
    const auto response = call(method, kShikimori + QStringLiteral("/api") + path, headers, body.isEmpty() ? QJsonValue() : QJsonValue(body));
    if (response.status == 401)
        throw signedOut(QStringLiteral("Shikimori"));
    return response;
}

QVariantList shikimoriSearch(const QString &media, const QString &title) {
    const auto kind = media == QLatin1String("anime") ? QStringLiteral("animes") : media == QLatin1String("novel") ? QStringLiteral("ranobe") : QStringLiteral("mangas");
    const auto response = shikimoriCall("GET", QStringLiteral("/%1?search=%2&limit=10").arg(kind, encoded(title.left(80))));
    if (!response.ok())
        throw Error(QStringLiteral("Shikimori answered %1.").arg(response.status), response.status);
    QVariantList out;
    for (const auto &value : response.json().array()) {
        const auto m = value.toObject();
        const auto titles = cleanTitles({m.value("name").toString(), m.value("russian").toString()});
        const auto preview = m.value("image").toObject().value("preview").toString();
        out.append(QVariantMap{{"id", m.value("id").toInt()},
                               {"title", titles.value(0)},
                               {"titles", titles},
                               {"detail", detailOf(m.value("kind").toString().toUpper().replace(QLatin1Char('_'), QLatin1Char(' ')),
                                                   m.value("aired_on").toString().left(4))},
                               {"cover", preview.startsWith(QLatin1Char('/')) ? kShikimori + preview : preview},
                               {"total", m.value(media == QLatin1String("anime") ? "episodes" : "chapters").toInt()}});
    }
    return out;
}

void shikimoriPush(const QVariant &id, const QString &media, int progress) {
    const auto a = account(QStringLiteral("shikimori"));
    if (a.isEmpty())
        return;
    const bool anime = media == QLatin1String("anime");
    const auto field = anime ? QStringLiteral("episodes") : QStringLiteral("chapters");
    const auto found = shikimoriCall("GET", QStringLiteral("/v2/user_rates?user_id=%1&target_id=%2&target_type=%3")
                                                .arg(a.value("userId").toString(), id.toString(), anime ? "Anime" : "Manga"));
    if (!found.ok())
        throw Error(QStringLiteral("Shikimori answered %1.").arg(found.status), found.status);
    const auto entry = found.json().array().first().toObject();
    if (!entry.isEmpty() && entry.value(field).toInt() >= progress)
        return;
    const auto info = shikimoriCall("GET", QStringLiteral("/%1/%2").arg(anime ? "animes" : "mangas", id.toString()));
    const int total = info.ok() ? json(info).value(field).toInt() : 0;
    QJsonObject rate{{field, progress}, {"status", total > 0 && progress >= total ? "completed" : "watching"}};
    if (!entry.isEmpty()) {
        shikimoriCall("PATCH", QStringLiteral("/v2/user_rates/%1").arg(entry.value("id").toInteger()), {{"user_rate", rate}});
        return;
    }
    rate.insert("user_id", a.value("userId").toString().toLongLong());
    rate.insert("target_id", id.toString().toLongLong());
    rate.insert("target_type", anime ? "Anime" : "Manga");
    shikimoriCall("POST", QStringLiteral("/v2/user_rates"), {{"user_rate", rate}});
}

// ---------------------------------------------------------------- Bangumi

Response bangumiCall(const QByteArray &method, const QString &path, const QJsonValue &body = {}) {
    Headers headers{{"Accept", "application/json"}, {"User-Agent", userAgent()}};
    if (const auto token = account(QStringLiteral("bangumi")).value("token").toString(); !token.isEmpty())
        headers.append({"Authorization", "Bearer " + token.toUtf8()});
    const auto response = call(method, kBangumi + path, headers, body);
    if (response.status == 401)
        throw signedOut(QStringLiteral("Bangumi"));
    return response;
}

QVariantList bangumiSearch(const QString &media, const QString &title) {
    const bool anime = media == QLatin1String("anime");
    // Subject types: 1 books (comics and novels by platform), 2 anime.
    const auto response = bangumiCall("POST", QStringLiteral("/v0/search/subjects?limit=15"),
                                      QJsonObject{{"keyword", title.left(80)},
                                                  {"sort", "match"},
                                                  {"filter", QJsonObject{{"type", QJsonArray{anime ? 2 : 1}}}}});
    if (!response.ok())
        throw Error(QStringLiteral("Bangumi answered %1.").arg(response.status), response.status);
    QVariantList out;
    for (const auto &value : json(response).value("data").toArray()) {
        const auto s = value.toObject();
        const auto platform = s.value("platform").toString();
        if (media == QLatin1String("novel") && platform != QStringLiteral("小说"))
            continue;
        if (media == QLatin1String("comic") && !platform.isEmpty() && platform != QStringLiteral("漫画"))
            continue;
        const auto titles = cleanTitles({s.value("name").toString(), s.value("name_cn").toString()});
        out.append(QVariantMap{{"id", s.value("id").toInt()},
                               {"title", titles.value(0)},
                               {"titles", titles},
                               {"detail", detailOf(platform, s.value("date").toString().left(4))},
                               {"cover", s.value("images").toObject().value("small").toString()},
                               {"total", s.value("eps").toInt()}});
    }
    return out;
}

void bangumiPush(const QVariant &id, const QString &media, int progress) {
    const auto a = account(QStringLiteral("bangumi"));
    if (a.isEmpty())
        return;
    const auto path = QStringLiteral("/v0/users/-/collections/") + id.toString();
    const auto current = bangumiCall("GET", QStringLiteral("/v0/users/%1/collections/%2").arg(encoded(a.value("user").toString()), id.toString()));
    if (!current.ok() && current.status != 404)
        throw Error(QStringLiteral("Bangumi answered %1.").arg(current.status), current.status);
    const auto entry = current.ok() ? json(current) : QJsonObject();
    const int total = entry.value("subject").toObject().value("eps").toInt();
    // Collection types: 2 done, 3 doing.
    const int type = total > 0 && progress >= total ? 2 : 3;
    if (media != QLatin1String("anime")) {
        if (entry.value("ep_status").toInt() >= progress)
            return;
        bangumiCall(entry.isEmpty() ? "POST" : "PATCH", path, QJsonObject{{"type", type}, {"ep_status", progress}});
        return;
    }
    // Anime progress is per episode: mark the main episodes up to `progress`.
    if (entry.value("ep_status").toInt() >= progress)
        return;
    if (entry.isEmpty() || entry.value("type").toInt() != type)
        bangumiCall(entry.isEmpty() ? "POST" : "PATCH", path, QJsonObject{{"type", type}});
    const auto episodes = bangumiCall("GET", QStringLiteral("/v0/episodes?subject_id=%1&type=0&limit=200").arg(id.toString()));
    if (!episodes.ok())
        return;
    QJsonArray ids;
    for (const auto &value : json(episodes).value("data").toArray()) {
        const auto ep = value.toObject();
        const double number = ep.value("ep").toDouble(ep.value("sort").toDouble());
        if (number >= 1 && number <= progress)
            ids.append(ep.value("id"));
    }
    if (!ids.isEmpty())
        bangumiCall("PATCH", path + QStringLiteral("/episodes"), QJsonObject{{"episode_id", ids}, {"type", 2}});
}

// ------------------------------------------------------------------ Simkl

Response simklCall(const QByteArray &method, const QString &path, const QJsonValue &body = {}) {
    Headers headers{{"Accept", "application/json"}, {"simkl-api-key", kSimklId.toUtf8()}, {"User-Agent", userAgent()}};
    if (const auto token = account(QStringLiteral("simkl")).value("token").toString(); !token.isEmpty())
        headers.append({"Authorization", "Bearer " + token.toUtf8()});
    const auto response = call(method, kSimkl + path, headers, body);
    if (response.status == 401)
        throw signedOut(QStringLiteral("Simkl"));
    return response;
}

QVariantList simklSearch(const QString &, const QString &title) {
    const auto response = simklCall("GET", QStringLiteral("/search/anime?q=%1&extended=full&limit=10&client_id=%2").arg(encoded(title.left(80)), kSimklId));
    if (!response.ok())
        throw Error(QStringLiteral("Simkl answered %1.").arg(response.status), response.status);
    QVariantList out;
    for (const auto &value : response.json().array()) {
        const auto m = value.toObject();
        QStringList titles{m.value("title_romaji").toString(), m.value("title").toString(), m.value("title_en").toString()};
        for (const auto &s : m.value("all_titles").toArray())
            titles.append(s.toString());
        titles = cleanTitles(titles);
        const auto poster = m.value("poster").toString();
        out.append(QVariantMap{{"id", m.value("ids").toObject().value("simkl_id").toInteger()},
                               {"title", titles.value(0)},
                               {"titles", titles},
                               {"detail", detailOf(m.value("anime_type").toString(m.value("type").toString()).toUpper(),
                                                   m.value("year").isDouble() ? QString::number(m.value("year").toInt()) : QString())},
                               {"cover", poster.isEmpty() ? QString() : QStringLiteral("https://simkl.in/posters/%1_m.webp").arg(poster)},
                               {"total", m.value("ep_count").toInt()}});
    }
    return out;
}

void simklPush(const QVariant &id, const QString &, int progress) {
    if (account(QStringLiteral("simkl")).isEmpty())
        return;
    // Simkl keeps watched episodes, not a number: send the ones up to here
    // (it ignores those it has).
    QJsonArray episodes;
    for (int n = 1; n <= std::min(progress, 2000); ++n)
        episodes.append(QJsonObject{{"number", n}});
    const QJsonObject ids{{"simkl", id.toLongLong()}};
    simklCall("POST", QStringLiteral("/sync/history"),
              QJsonObject{{"shows", QJsonArray{QJsonObject{{"ids", ids}, {"seasons", QJsonArray{QJsonObject{{"number", 1}, {"episodes", episodes}}}}}}}});
    simklCall("POST", QStringLiteral("/sync/add-to-list"), QJsonObject{{"shows", QJsonArray{QJsonObject{{"ids", ids}, {"to", "watching"}}}}});
}

} // namespace

const QList<Service> &services() {
    static const QList<Service> list{
        {QStringLiteral("kitsu"), QStringLiteral("Kitsu"), {"comic", "novel", "anime"}, [] { return true; }, kitsuSearch, kitsuPush},
        {QStringLiteral("mangaupdates"), QStringLiteral("MangaUpdates"), {"comic", "novel"}, [] { return true; }, muSearch, muPush},
        {QStringLiteral("shikimori"), QStringLiteral("Shikimori"), {"comic", "novel", "anime"},
         [] { return !kShikimoriId.isEmpty() && !kShikimoriSecret.isEmpty(); }, shikimoriSearch, shikimoriPush},
        {QStringLiteral("bangumi"), QStringLiteral("Bangumi"), {"comic", "novel", "anime"}, [] { return true; }, bangumiSearch, bangumiPush},
        {QStringLiteral("simkl"), QStringLiteral("Simkl"), {"anime"}, [] { return !kSimklId.isEmpty(); }, simklSearch, simklPush},
    };
    return list;
}

const Service *service(const QString &key) {
    for (const auto &s : services())
        if (s.key == key)
            return &s;
    return nullptr;
}

QVariantMap passwordSignIn(const QString &key, const QString &user, const QString &password) {
    if (user.trimmed().isEmpty() || password.isEmpty())
        throw Error(QStringLiteral("Enter your user name and password."));
    if (key == QLatin1String("kitsu"))
        return kitsuSignIn(user.trimmed(), password);
    if (key == QLatin1String("mangaupdates"))
        return muSignIn(user.trimmed(), password);
    throw Error(QStringLiteral("Unknown service."));
}

QVariantMap tokenSignIn(const QString &key, const QString &pasted) {
    const auto token = pasted.trimmed();
    if (key != QLatin1String("bangumi"))
        throw Error(QStringLiteral("Unknown service."));
    if (token.isEmpty())
        throw Error(QStringLiteral("Paste the token Bangumi showed."));
    setAccount(key, {{"token", token}});
    const auto response = bangumiCall("GET", QStringLiteral("/v0/me"));
    if (!response.ok()) {
        setAccount(key, {});
        throw Error(QStringLiteral("Bangumi did not accept that token."));
    }
    const auto me = json(response);
    const auto name = me.value("nickname").toString(me.value("username").toString());
    setAccount(key, {{"token", token},
                     {"user", me.value("username").toString(QString::number(me.value("id").toInt()))},
                     {"name", name},
                     {"avatar", me.value("avatar").toObject().value("medium").toString()}});
    return {{"name", name}};
}

QString shikimoriAuthorizeUrl(const QString &redirect, const QString &state) {
    QUrl url(kShikimori + QStringLiteral("/oauth/authorize"));
    url.setQuery(QUrlQuery{{"client_id", kShikimoriId},
                           {"redirect_uri", redirect},
                           {"response_type", "code"},
                           {"scope", "user_rates"},
                           {"state", state}});
    return url.toString(QUrl::FullyEncoded);
}

QVariantMap shikimoriCode(const QString &code, const QString &redirect) {
    const auto response = Http::postForm(QUrl(kShikimori + QStringLiteral("/oauth/token")),
                                         {{"grant_type", "authorization_code"},
                                          {"client_id", kShikimoriId},
                                          {"client_secret", kShikimoriSecret},
                                          {"code", code},
                                          {"redirect_uri", redirect}},
                                         {{"User-Agent", userAgent()}});
    if (!response.ok())
        throw Error(QStringLiteral("Shikimori did not accept the sign-in (%1).").arg(response.status));
    setAccount(QStringLiteral("shikimori"), {});
    saveShikimori(json(response));
    const auto me = json(shikimoriCall("GET", QStringLiteral("/users/whoami")));
    auto a = account(QStringLiteral("shikimori"));
    a.insert("userId", QString::number(me.value("id").toInteger()));
    a.insert("name", me.value("nickname").toString());
    a.insert("avatar", me.value("avatar").toString());
    setAccount(QStringLiteral("shikimori"), a);
    return {{"name", a.value("name")}};
}

QVariantMap simklPin() {
    const auto response = simklCall("GET", QStringLiteral("/oauth/pin?client_id=%1").arg(kSimklId));
    const auto j = json(response);
    if (!response.ok() || j.value("user_code").toString().isEmpty())
        throw Error(QStringLiteral("Simkl answered %1.").arg(response.status), response.status);
    return {{"code", j.value("user_code").toString()},
            {"url", j.value("verification_url").toString(QStringLiteral("https://simkl.com/pin"))},
            {"seconds", j.value("expires_in").toInt(900)},
            {"interval", j.value("interval").toInt(5)}};
}

QVariantMap simklWait(const QString &userCode, int seconds, int interval) {
    const qint64 until = Database::now() + std::clamp(seconds, 60, 1800);
    interval = std::clamp(interval, 2, 30);
    while (Database::now() < until) {
        for (int i = 0; i < interval * 4; ++i) {
            if (CancelScope::cancelled())
                throw Error(QStringLiteral("Cancelled"), 499);
            QThread::msleep(250);
        }
        const auto response = simklCall("GET", QStringLiteral("/oauth/pin/%1?client_id=%2").arg(encoded(userCode), kSimklId));
        const auto token = json(response).value("access_token").toString();
        if (token.isEmpty())
            continue;
        setAccount(QStringLiteral("simkl"), {{"token", token}});
        const auto me = json(simklCall("GET", QStringLiteral("/users/settings"))).value("user").toObject();
        setAccount(QStringLiteral("simkl"), {{"token", token}, {"name", me.value("name").toString()}, {"avatar", me.value("avatar").toString()}});
        return {{"name", me.value("name").toString()}};
    }
    throw Error(QStringLiteral("The Simkl code ran out. Connect again."));
}

void update(const Service &s, const QString &seriesId, const QString &media, const QString &title, int progress) {
    if (account(s.key).isEmpty() || !s.media.contains(media) || !s.available())
        return;
    auto id = allLinks().value(seriesId).toMap().value(s.key).toMap().value("id");
    if (!hasId(id)) {
        if (missedRecently(seriesId, s.key))
            return;
        const auto match = matchByTitles([&](const QString &t) { return s.search(media, t); }, lookupTitles(seriesId, title));
        saveMiss(seriesId, s.key, match.isEmpty());
        if (match.isEmpty())
            return;
        id = match.value("id");
        saveLink(seriesId, s.key, id, match.value("title").toString());
    }
    s.push(id, media, progress);
}

} // namespace sc::tracking
