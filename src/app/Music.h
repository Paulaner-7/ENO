#pragma once

#include "core/Task.h"

#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

namespace sc {

// Songs: anime openings, endings and inserts from AnimeThemes
// (api.animethemes.moe, matched by AniList or MyAnimeList id), and lyrics
// from LRCLIB (lrclib.net, synced when it has them). Every call is a Task;
// nothing blocks the interface.
//
// A song is {id, type ("OP", "ED", "IN"), sequence, title, artists: [names],
// episodes, audio (Ogg), video (WebM), anime, cover, anilistId, malId}; a
// file of yours has {local: true, file, lrc, album, track, disc, duration}.
class Music : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // AnimeThemes plays from its own host; the sources manifest can turn it
    // off (music.animethemes = false) like the built-in sources.
    Q_PROPERTY(bool animeSongsAvailable READ animeSongsAvailable CONSTANT)

public:
    static Music *instance();
    static Music *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool animeSongsAvailable() const;

    // An anime's songs, by its AniList id (else its MyAnimeList id).
    Q_INVOKABLE sc::Task *animeSongs(int anilistId, int malId = 0);
    // Songs by title, artist or anime.
    Q_INVOKABLE sc::Task *searchSongs(const QString &query);
    // This season's anime and their songs: [{anime, cover, anilistId,
    // songs: [song]}].
    Q_INVOKABLE sc::Task *seasonSongs();
    // Songs in the folders chosen on the Music page (the musicFolders
    // setting), album by album. Tags are read once per version of a file;
    // a .lrc beside a file is its `lrc`.
    Q_INVOKABLE sc::Task *localSongs();
    static QStringList musicFolders();
    // A song's lyrics: {synced: [{time, text}], plain, instrumental, found}.
    // `duration` in seconds helps LRCLIB pick the right version. A file of
    // yours (`file`, `lrc`) is asked first.
    Q_INVOKABLE sc::Task *lyrics(const QString &title, const QString &artist, const QString &album, double duration,
                                 const QString &file = {}, const QString &lrc = {});

    // Parsers, public for the tests.
    // "[mm:ss.xx] text" lines into [{time, text}] by time; tags and empty
    // stamps are left out, several stamps on a line repeat it.
    static QVariantList parseLrc(const QString &lrc);
    static QVariantList songsOfAnime(const QJsonObject &anime);

private:
    explicit Music(QObject *parent = nullptr);
};

} // namespace sc
