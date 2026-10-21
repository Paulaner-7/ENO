#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

namespace sc::anime::kitsu {

// Kitsu (kitsu.io), for when AniList is unreachable: its anime are given in
// AniList's own shape (Media objects with AniList's ids, from Kitsu's
// mappings), so everything built on AniList's answers (cards, details,
// episodes, the library, sync) keeps working. Anime Kitsu cannot tie to an
// AniList entry are left out. Blocking; worker threads. Answers are cached
// like AniList's, stale ones used when Kitsu fails too.

// A home list: "trending" (airing now, most followed), "popular",
// "season" (the current one) or "top".
QJsonArray list(const QString &listing);

// AniList's search filters as Anime::search takes them (q, sort, genres,
// yearFrom, yearTo, status, format, page): {media: [...], hasNextPage}.
QJsonObject search(const QVariantMap &filters);

// One anime by its AniList id, with description, titles, trailer and
// studios (no relations: Kitsu's carry no AniList ids). Throws 404 when
// Kitsu does not know it.
QJsonObject media(int anilistId);

// Episodes airing between two Unix times, as AniList's airingSchedules
// rows ({airingAt, episode, media}): the most followed airing shows on
// Kitsu, with air times and numbers from ani.zip (else Kitsu's next
// release, episode 0: unknown).
QJsonArray airing(qint64 start, qint64 end);

// The AniList id of a MyAnimeList anime (ani.zip, else Kitsu's mappings),
// or 0.
int anilistIdForMal(int malId);

// For tests: one Kitsu anime resource, with the `included` resources it
// refers to, in AniList's shape; empty when it has no AniList id.
QJsonObject toAniList(const QJsonObject &anime, const QJsonArray &included);

} // namespace sc::anime::kitsu
