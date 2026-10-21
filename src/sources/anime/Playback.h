#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>

namespace sc::anime {

// How an episode from an anime source starts playing; the rules, apart from
// the calls (Anime.cpp).

// A video for QML (AnimeSource.h describes it), with its chapters
// (openings, endings, recaps) as AniSkip-style skips: [{type: op | ed |
// recap | mixed-op, start, end}].
QVariantMap videoMap(const QJsonObject &video);
QVariantList videoList(const QJsonArray &videos);

// What plays first, as Aniyomi picks it, over hosters as Anime.videos()
// lists them: the first preferred video of a hoster that answered, else
// the first with an address, else the first that can still get one.
// Videos that need a local server (which SCANCORE does not run) are passed
// over. {hoster, video} indexes, or null.
QVariant bestVideo(const QVariantList &hosters);

// The episode numbered `number` (AniList's numbering) in a source's list
// (oldest first): by its number, else by its place when the source numbers
// none. Empty when it has no such episode.
QString episodeNumbered(const QJsonArray &episodes, double number);

} // namespace sc::anime
