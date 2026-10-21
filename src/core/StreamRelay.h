#pragma once

#include "core/Http.h"

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <functional>

namespace sc::video {

// A relay on 127.0.0.1 between SCANCORE's player and a video's server. Many
// anime CDNs dress HLS streams up as something else: parts named .jpg,
// .image or nothing at all, wrapped in a 1x1 PNG or a JPEG, playlists at
// addresses without .m3u8 served as text/plain. Aniyomi's player has an
// FFmpeg patched for them; every other FFmpeg (7.1 and newer especially)
// refuses them for its extension checks or reads the part as a picture. The
// relay serves each playlist rewritten to point back at it, under names
// FFmpeg accepts, and each part with what it was dressed in taken off, so
// the stream plays on any FFmpeg. Parts are passed on as they arrive.
class StreamRelay {
public:
    // Where the player should open `url`, fetched with `headers`; `url`
    // itself when the relay cannot start. Any thread.
    static QUrl wrap(const QUrl &url, const Headers &headers);
    // Whether `url` is worth relaying: anything but files players read as
    // they are (.mp4, .mkv, ...), whose servers need nothing undone.
    static bool wanted(const QUrl &url);
};

// What an address in a playlist names: another playlist, a part to undress,
// or something passed on untouched (keys, init sections, encrypted parts
// and parts read in byte ranges, where offsets must stay as they are).
enum class Part { Playlist, Media, Raw };

// `text` (an HLS playlist fetched from `base`) with every http(s) address in
// it, made absolute, replaced by `address(url, kind, suffix, subtitles)`.
// `suffix` is a name ending FFmpeg accepts for what the address holds;
// `subtitles` marks a subtitle rendition's playlist. `subtitlePlaylist` says
// `text` is one.
using RelayAddress = std::function<QByteArray(const QUrl &url, Part kind, const QString &suffix, bool subtitles)>;
QByteArray rewritePlaylist(const QByteArray &text, const QUrl &base, bool subtitlePlaylist, const RelayAddress &address);

// Where the media in an HLS part starts: 0 when it starts at once (or its
// format is not one to look into), past the picture or page it is wrapped
// in otherwise. -1 when `head` is too short to tell and more of it is
// coming (`complete` false).
qsizetype mediaStart(const QByteArray &head, bool complete);

} // namespace sc::video
