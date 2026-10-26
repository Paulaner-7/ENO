#include "Music.h"

#include "core/Database.h"
#include "core/Http.h"
#include "sources/Manifest.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace sc {

namespace {

const QString kThemes = QStringLiteral("https://api.animethemes.moe");
const QString kLrclib = QStringLiteral("https://lrclib.net/api");
// Both services ask callers to say who they are.
const Headers kHeaders{{"Accept", "application/json"},
                       {"User-Agent", "SCANCORE (https://github.com/medium-gh/scancore)"}};
const QString kInclude = QStringLiteral("images,resources,animethemes.song.artists,animethemes.animethemeentries.videos.audio");

QJsonObject getJson(const QUrl &url, const QString &cacheKey, qint64 ttl) {
    if (!cacheKey.isEmpty())
        if (const auto hit = Database::cacheGet(cacheKey))
            return QJsonDocument::fromJson(*hit).object();
    const auto response = Http::get(url, kHeaders, 15000);
    if (!response.ok()) {
        if (!cacheKey.isEmpty())
            if (const auto stale = Database::cacheGet(cacheKey, true))
                return QJsonDocument::fromJson(*stale).object();
        if (response.status == 404)
            return {};
        throw Error(QStringLiteral("The songs service is unavailable right now (%1).").arg(response.status), response.status);
    }
    const auto object = response.json().object();
    if (!cacheKey.isEmpty())
        Database::cachePut(cacheKey, QJsonDocument(object).toJson(QJsonDocument::Compact), ttl);
    return object;
}

QString coverOf(const QJsonObject &anime) {
    QString small;
    for (const auto &value : anime.value("images").toArray()) {
        const auto image = value.toObject();
        if (image.value("facet").toString() == QLatin1String("Large Cover"))
            return image.value("link").toString();
        small = image.value("link").toString();
    }
    return small;
}

void ensureAvailable() {
    if (!Music::instance()->animeSongsAvailable())
        throw Error(QStringLiteral("Anime songs are turned off for now."));
}

} // namespace

Music::Music(QObject *parent) : QObject(parent) {}

Music *Music::instance() {
    static auto *music = new Music();
    return music;
}

bool Music::animeSongsAvailable() const {
    return Manifest::current().value("music").toObject().value("animethemes").toBool(true);
}

QVariantList Music::songsOfAnime(const QJsonObject &anime) {
    int anilistId = 0, malId = 0;
    for (const auto &value : anime.value("resources").toArray()) {
        const auto resource = value.toObject();
        const auto site = resource.value("site").toString();
        if (site == QLatin1String("AniList"))
            anilistId = resource.value("external_id").toInt();
        else if (site == QLatin1String("MyAnimeList"))
            malId = resource.value("external_id").toInt();
    }
    const auto cover = coverOf(anime);
    const auto animeName = anime.value("name").toString();
    QVariantList out;
    for (const auto &value : anime.value("animethemes").toArray()) {
        const auto theme = value.toObject();
        const auto song = theme.value("song").toObject();
        QStringList artists;
        for (const auto &artist : song.value("artists").toArray())
            artists.append(artist.toObject().value("name").toString());
        // The first entry is the version the episodes start with; its
        // first video the creditless one where there is one.
        QString audio, video, episodes;
        for (const auto &entryValue : theme.value("animethemeentries").toArray()) {
            const auto entry = entryValue.toObject();
            for (const auto &videoValue : entry.value("videos").toArray()) {
                const auto v = videoValue.toObject();
                if (video.isEmpty() || v.value("nc").toBool()) {
                    video = v.value("link").toString();
                    audio = v.value("audio").toObject().value("link").toString();
                }
                if (v.value("nc").toBool())
                    break;
            }
            episodes = entry.value("episodes").toString();
            if (!video.isEmpty())
                break;
        }
        if (audio.isEmpty() && video.isEmpty())
            continue;
        const auto type = theme.value("type").toString();
        const int sequence = theme.value("sequence").toInt();
        out.append(QVariantMap{{"id", QStringLiteral("animethemes:%1").arg(theme.value("id").toInt())},
                               {"type", type},
                               {"sequence", sequence},
                               {"label", sequence > 0 ? type + QString::number(sequence) : type},
                               {"title", song.value("title").toString()},
                               {"artists", artists},
                               {"episodes", episodes},
                               {"audio", audio},
                               {"video", video},
                               {"anime", animeName},
                               {"cover", cover},
                               {"anilistId", anilistId},
                               {"malId", malId}});
    }
    return out;
}

Task *Music::animeSongs(int anilistId, int malId) {
    return Task::run([anilistId, malId]() -> QVariant {
        ensureAvailable();
        for (const auto &[site, id] : {std::pair{QStringLiteral("AniList"), anilistId}, std::pair{QStringLiteral("MyAnimeList"), malId}}) {
            if (id <= 0)
                continue;
            QUrl url(kThemes + QStringLiteral("/anime"));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("filter[has]"), QStringLiteral("resources"));
            query.addQueryItem(QStringLiteral("filter[site]"), site);
            query.addQueryItem(QStringLiteral("filter[external_id]"), QString::number(id));
            query.addQueryItem(QStringLiteral("include"), kInclude);
            url.setQuery(query);
            const auto data = getJson(url, QStringLiteral("animesongs|%1|%2").arg(site).arg(id), 86400);
            QVariantList songs;
            for (const auto &anime : data.value("anime").toArray())
                songs += songsOfAnime(anime.toObject());
            if (!songs.isEmpty())
                return songs;
        }
        return QVariantList();
    });
}

Task *Music::searchSongs(const QString &query) {
    const auto q = query.simplified().left(100);
    return Task::run([q]() -> QVariant {
        ensureAvailable();
        if (q.isEmpty())
            return QVariantList();
        // Anime matching the words, with their songs; then songs whose own
        // title matches, on anime the first list missed.
        QUrl url(kThemes + QStringLiteral("/search"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("q"), q);
        query.addQueryItem(QStringLiteral("fields[search]"), QStringLiteral("anime,animethemes"));
        query.addQueryItem(QStringLiteral("include[anime]"), kInclude);
        query.addQueryItem(QStringLiteral("include[animetheme]"),
                           QStringLiteral("anime.images,song.artists,animethemeentries.videos.audio"));
        url.setQuery(query);
        const auto search = getJson(url, QString(), 0).value("search").toObject();
        QVariantList out;
        QSet<QString> seen;
        const auto add = [&](const QVariantList &songs) {
            for (const auto &song : songs) {
                const auto id = song.toMap().value("id").toString();
                if (!seen.contains(id)) {
                    seen.insert(id);
                    out.append(song);
                }
            }
        };
        for (const auto &value : search.value("animethemes").toArray()) {
            auto theme = value.toObject();
            auto anime = theme.value("anime").toObject();
            anime.insert("animethemes", QJsonArray{theme});
            add(songsOfAnime(anime));
        }
        for (const auto &anime : search.value("anime").toArray())
            add(songsOfAnime(anime.toObject()));
        return out;
    });
}

Task *Music::seasonSongs() {
    return Task::run([]() -> QVariant {
        ensureAvailable();
        const auto today = QDate::currentDate();
        static const QStringList seasons{QStringLiteral("Winter"), QStringLiteral("Spring"), QStringLiteral("Summer"), QStringLiteral("Fall")};
        const auto season = seasons.value((today.month() - 1) / 3);
        QUrl url(kThemes + QStringLiteral("/anime"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("filter[year]"), QString::number(today.year()));
        query.addQueryItem(QStringLiteral("filter[season]"), season);
        query.addQueryItem(QStringLiteral("include"), kInclude);
        query.addQueryItem(QStringLiteral("page[size]"), QStringLiteral("40"));
        url.setQuery(query);
        const auto data = getJson(url, QStringLiteral("seasonsongs|%1|%2").arg(today.year()).arg(season), 6 * 3600);
        QVariantList out;
        for (const auto &value : data.value("anime").toArray()) {
            const auto anime = value.toObject();
            const auto songs = songsOfAnime(anime);
            if (songs.isEmpty())
                continue;
            const auto first = songs.first().toMap();
            out.append(QVariantMap{{"anime", anime.value("name").toString()},
                                   {"cover", first.value("cover")},
                                   {"anilistId", first.value("anilistId")},
                                   {"songs", songs}});
        }
        return out;
    });
}

QVariantList Music::parseLrc(const QString &lrc) {
    static const QRegularExpression stamp(QStringLiteral(R"(\[(\d+):(\d+(?:[.:]\d+)?)\])"));
    QList<QPair<double, QString>> lines;
    for (const auto &raw : lrc.split(QLatin1Char('\n'))) {
        auto it = stamp.globalMatch(raw);
        QList<double> times;
        qsizetype end = 0;
        while (it.hasNext()) {
            const auto m = it.next();
            if (m.capturedStart() != end)
                break;  // stamps only at the start of the line
            QString seconds = m.captured(2);
            seconds.replace(QLatin1Char(':'), QLatin1Char('.'));
            times.append(m.captured(1).toInt() * 60 + seconds.toDouble());
            end = m.capturedEnd();
        }
        if (times.isEmpty())
            continue;
        const auto text = raw.mid(end).trimmed();
        for (const double t : times)
            lines.append({t, text});
    }
    std::stable_sort(lines.begin(), lines.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    QVariantList out;
    for (const auto &[time, text] : lines)
        out.append(QVariantMap{{"time", time}, {"text", text}});
    return out;
}

Task *Music::lyrics(const QString &title, const QString &artist, const QString &album, double duration) {
    return Task::run([title, artist, album, duration]() -> QVariant {
        if (title.trimmed().isEmpty())
            return QVariantMap{{"found", false}};
        const auto key = QStringLiteral("lyrics|%1|%2|%3").arg(title.toLower(), artist.toLower()).arg(qRound(duration));
        if (const auto hit = Database::cacheGet(key))
            return QJsonDocument::fromJson(*hit).object().toVariantMap();
        const auto shape = [](const QJsonObject &l) {
            const auto synced = l.value("syncedLyrics").toString();
            return QVariantMap{{"found", true},
                               {"synced", parseLrc(synced)},
                               {"plain", l.value("plainLyrics").toString()},
                               {"instrumental", l.value("instrumental").toBool()}};
        };
        QVariantMap result{{"found", false}};
        // The exact match first (title, artist, album, duration), then a
        // search by title and artist, preferring synced lyrics of about the
        // same length.
        QUrl get(kLrclib + QStringLiteral("/get"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("track_name"), title);
        query.addQueryItem(QStringLiteral("artist_name"), artist);
        if (!album.isEmpty())
            query.addQueryItem(QStringLiteral("album_name"), album);
        if (duration > 0)
            query.addQueryItem(QStringLiteral("duration"), QString::number(qRound(duration)));
        get.setQuery(query);
        const auto exact = Http::get(get, kHeaders, 12000);
        if (exact.ok()) {
            result = shape(exact.json().object());
        } else {
            QUrl search(kLrclib + QStringLiteral("/search"));
            QUrlQuery sq;
            sq.addQueryItem(QStringLiteral("track_name"), title);
            if (!artist.isEmpty())
                sq.addQueryItem(QStringLiteral("artist_name"), artist);
            search.setQuery(sq);
            const auto found = Http::get(search, kHeaders, 12000);
            if (!found.ok() && found.status != 404)
                throw Error(QStringLiteral("Lyrics are unavailable right now (%1).").arg(found.status), found.status);
            QJsonObject best;
            double bestScore = -1;
            for (const auto &value : found.json().array()) {
                const auto l = value.toObject();
                double score = l.value("syncedLyrics").toString().isEmpty() ? 0 : 2;
                if (duration > 0)
                    score += std::max(0.0, 1 - std::abs(l.value("duration").toDouble() - duration) / 10);
                if (score > bestScore) {
                    bestScore = score;
                    best = l;
                }
            }
            if (!best.isEmpty())
                result = shape(best);
        }
        Database::cachePut(key, QJsonDocument(QJsonObject::fromVariantMap(result)).toJson(QJsonDocument::Compact),
                           result.value("found").toBool() ? 30 * 86400 : 86400);
        return result;
    });
}

} // namespace sc
