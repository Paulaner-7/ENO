#pragma once

#include "core/Http.h"

#include <QString>
#include <QUrl>

#include <functional>

namespace sc::video {

// A video kept for offline playback in `folder`: an HLS stream as a local
// playlist (video.m3u8) beside its segments, keys and init sections, played
// by mpv's HLS demuxer as it is (no re-encoding, encrypted streams stay
// encrypted); anything else as one file, downloaded in place with resume.
// Segments and partial files already there are kept, so a download that
// stopped goes on where it was. Blocking; runs on a worker, observes the
// caller's cancel flag. `progress(done, total)` reports segments or bytes.
struct Saved {
    QString file;
    bool hls = false;
};
Saved save(const QUrl &url, const Headers &headers, const QString &folder,
           const std::function<void(qint64 done, qint64 total)> &progress);

// One file from `url` into `path`, written as it arrives (`path`.part until
// complete), resuming a partial one with a Range request.
void streamToFile(const QUrl &url, const Headers &headers, const QString &path,
                  const std::function<void(qint64 done, qint64 total)> &progress);

// The highest variant at or below `maxHeight` of an HLS master playlist (the
// tallest when none is), as {uri, audioGroup}; for tests.
struct Variant {
    QString uri;
    QString audioGroup;
};
Variant pickVariant(const QString &master, int maxHeight = 1080);

} // namespace sc::video
