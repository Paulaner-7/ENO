#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QStringList>

// What a music file says about itself: ID3 (MP3), Vorbis comments (FLAC,
// Ogg Vorbis, Opus) and iTunes atoms (M4A), read from the few places that
// hold them, never the whole file.
namespace sc::local {

struct AudioTags {
    QString title;
    QStringList artists;
    QString album;
    QString albumArtist;
    // Unsynced lyrics kept in the file (often LRC text).
    QString lyrics;
    int track = 0;
    int disc = 0;
    // Seconds, 0 when the file does not say.
    double duration = 0;
    // The embedded picture (the front cover when there are several).
    QByteArray art;
};

bool isAudio(const QString &name);

// `name` picks the format by its extension (content:// addresses have
// none in the path). The picture is kept only with `wantArt`.
AudioTags readAudioTags(QIODevice &file, const QString &name, bool wantArt);
AudioTags readAudioTags(const QString &path, const QString &name, bool wantArt);

} // namespace sc::local
