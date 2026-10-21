#include "VideoSave.h"

#include "core/Parallel.h"
#include "core/StreamRelay.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

namespace sc::video {

namespace {

// Segments fetched at once; the image lane's per-host limit applies too.
constexpr int kBatch = 6;

// KEY=VALUE pairs of an HLS tag, values unquoted.
QHash<QString, QString> attributes(const QString &line) {
    QHash<QString, QString> out;
    static const QRegularExpression pair(QStringLiteral(R"(([A-Z0-9-]+)=("[^"]*"|[^,]*))"));
    const auto colon = line.indexOf(QLatin1Char(':'));
    auto it = pair.globalMatch(line.mid(colon + 1));
    while (it.hasNext()) {
        const auto m = it.next();
        auto value = m.captured(2);
        if (value.startsWith(QLatin1Char('"')))
            value = value.mid(1, value.size() - 2);
        out.insert(m.captured(1), value);
    }
    return out;
}

QString withUri(QString line, const QString &uri) {
    static const QRegularExpression attr(QStringLiteral(R"(URI="[^"]*")"));
    return line.replace(attr, QStringLiteral("URI=\"%1\"").arg(uri));
}

Response fetch(const QUrl &url, const Headers &headers, const QByteArray &range = {}, qint64 maxBytes = 256 * 1024 * 1024) {
    Request request;
    request.url = url;
    request.headers = headers;
    if (!range.isEmpty())
        request.headers.append({"Range", "bytes=" + range});
    request.timeoutMs = 60000;
    request.maxBytes = maxBytes;
    request.lane = Lane::Images;
    Response response;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            response = Http::send(request);
            if (response.ok())
                return response;
            if (response.status == 403 || response.status == 404 || response.status == 410)
                break;
        } catch (const Error &e) {
            if (attempt == 2 || e.status() == 499)
                throw;
        }
    }
    throw Error(QStringLiteral("The video server answered %1.").arg(response.status), response.status);
}

void writeFile(const QString &path, const QByteArray &data) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        throw Error(QStringLiteral("Could not write to the downloads folder."));
}

// What a segment holds, as ffmpeg checks it against the name.
QString segmentSuffix(const QByteArray &head) {
    if (!head.isEmpty() && uchar(head.at(0)) == 0x47)
        return QStringLiteral("ts");
    const auto box = head.mid(4, 4);
    if (box == "ftyp" || box == "styp" || box == "moof" || box == "sidx")
        return QStringLiteral("m4s");
    if (head.startsWith("ID3") || (head.size() > 1 && uchar(head.at(0)) == 0xFF && (uchar(head.at(1)) & 0xF0) == 0xF0))
        return QStringLiteral("aac");
    return QStringLiteral("ts");
}

QString existingSegment(const QString &folder, const QString &base) {
    for (const auto *suffix : {"ts", "m4s", "aac"}) {
        const auto name = base + QLatin1Char('.') + QLatin1String(suffix);
        if (QFileInfo(folder + QLatin1Char('/') + name).size() > 0)
            return name;
    }
    return {};
}

struct Segment {
    QUrl url;
    QByteArray range;
    QString base;
    int line = 0;
    bool encrypted = false;
};

// One media playlist: its segments, keys and init sections downloaded, and
// a copy pointing at them written as `name`.
void saveMedia(const QUrl &url, const Headers &headers, const QString &folder, const QString &prefix, const QString &name,
               qint64 &done, qint64 &total, const std::function<void(qint64, qint64)> &progress) {
    const auto response = fetch(url, headers, {}, 16 * 1024 * 1024);
    // Parts are relative to where the playlist ended up after redirects.
    const QUrl base = response.finalUrl.isValid() ? response.finalUrl : url;
    const auto text = QString::fromUtf8(response.body);
    if (!text.startsWith(QLatin1String("#EXTM3U")))
        throw Error(QStringLiteral("The video's playlist could not be read."));
    auto lines = text.split(QLatin1Char('\n'));
    QList<Segment> segments;
    QHash<QString, QString> keys;
    QByteArray pendingRange;
    qint64 rangeEnd = 0;
    int extras = 0;
    bool encrypted = false;
    for (int i = 0; i < lines.size(); ++i) {
        auto line = lines[i].trimmed();
        lines[i] = line;
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("#EXT-X-KEY:")) || line.startsWith(QLatin1String("#EXT-X-MAP:"))) {
            const auto a = attributes(line);
            const auto uri = a.value(QStringLiteral("URI"));
            const bool map = line.startsWith(QLatin1String("#EXT-X-MAP:"));
            if (!map)
                encrypted = a.value(QStringLiteral("METHOD"), QStringLiteral("NONE")) != QLatin1String("NONE");
            if (uri.isEmpty() || a.value(QStringLiteral("METHOD")) == QLatin1String("NONE"))
                continue;
            QByteArray range;
            if (map && a.contains(QStringLiteral("BYTERANGE"))) {
                const auto parts = a.value(QStringLiteral("BYTERANGE")).split(QLatin1Char('@'));
                const qint64 length = parts.value(0).toLongLong(), offset = parts.value(1).toLongLong();
                range = QByteArray::number(offset) + '-' + QByteArray::number(offset + length - 1);
                line.remove(QRegularExpression(QStringLiteral(R"(,?BYTERANGE="[^"]*")")));
            }
            const auto cacheKey = uri + QLatin1Char('|') + QString::fromLatin1(range);
            auto local = keys.value(cacheKey);
            if (local.isEmpty()) {
                local = QStringLiteral("%1-%2.%3").arg(prefix, map ? QStringLiteral("init") : QStringLiteral("key")).arg(++extras);
                local = map ? local + QStringLiteral(".mp4") : local + QStringLiteral(".key");
                const auto path = folder + QLatin1Char('/') + local;
                if (QFileInfo(path).size() == 0)
                    writeFile(path, fetch(base.resolved(QUrl(uri)), headers, range).body);
                keys.insert(cacheKey, local);
            }
            lines[i] = withUri(line, local);
        } else if (line.startsWith(QLatin1String("#EXT-X-BYTERANGE:"))) {
            const auto parts = line.mid(17).split(QLatin1Char('@'));
            const qint64 length = parts.value(0).toLongLong();
            const qint64 offset = parts.size() > 1 ? parts.value(1).toLongLong() : rangeEnd;
            pendingRange = QByteArray::number(offset) + '-' + QByteArray::number(offset + length - 1);
            rangeEnd = offset + length;
            // Each range becomes a file of its own.
            lines[i].clear();
        } else if (!line.startsWith(QLatin1Char('#'))) {
            segments.append({base.resolved(QUrl(line)), std::exchange(pendingRange, QByteArray()),
                             QStringLiteral("%1-%2").arg(prefix).arg(segments.size() + 1, 5, 10, QLatin1Char('0')), i, encrypted});
        }
    }
    if (segments.isEmpty())
        throw Error(QStringLiteral("The video's playlist lists no parts."));
    total += segments.size();
    for (int start = 0; start < segments.size(); start += kBatch) {
        if (CancelScope::cancelled())
            throw Error(QStringLiteral("Cancelled"), 499);
        QList<std::function<QString()>> jobs;
        for (int s = start; s < std::min<int>(start + kBatch, segments.size()); ++s)
            jobs.append([&, s]() -> QString {
                const auto &segment = segments[s];
                if (auto have = existingSegment(folder, segment.base); !have.isEmpty())
                    return have;
                auto body = fetch(segment.url, headers, segment.range).body;
                // Saved without the picture some sites wrap parts in (see
                // StreamRelay.h); encrypted parts and ranges stay as sent.
                if (!segment.encrypted && segment.range.isEmpty())
                    body.remove(0, mediaStart(body, true));
                const auto name = segment.base + QLatin1Char('.') + segmentSuffix(body.left(16));
                writeFile(folder + QLatin1Char('/') + name, body);
                return name;
            });
        const auto results = parallel<QString>(jobs);
        for (int r = 0; r < results.size(); ++r) {
            if (!results[r].ok())
                throw Error(QStringLiteral("Part %1 of the video could not be downloaded: %2").arg(start + r + 1).arg(results[r].error),
                            results[r].status);
            lines[segments[start + r].line] = *results[r].value;
            ++done;
        }
        progress(done, total);
    }
    lines.removeAll(QString());
    writeFile(folder + QLatin1Char('/') + name, lines.join(QLatin1Char('\n')).toUtf8() + '\n');
}

} // namespace

Variant pickVariant(const QString &master, int maxHeight) {
    const auto lines = master.split(QLatin1Char('\n'));
    Variant best, tallest;
    int bestHeight = -1, tallestHeight = -1;
    qint64 bestRate = -1, tallestRate = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const auto line = lines[i].trimmed();
        if (!line.startsWith(QLatin1String("#EXT-X-STREAM-INF:")))
            continue;
        QString uri;
        for (int j = i + 1; j < lines.size() && uri.isEmpty(); ++j)
            if (const auto next = lines[j].trimmed(); !next.isEmpty() && !next.startsWith(QLatin1Char('#')))
                uri = next;
        if (uri.isEmpty())
            continue;
        const auto a = attributes(line);
        const int height = a.value(QStringLiteral("RESOLUTION")).section(QLatin1Char('x'), 1, 1).toInt();
        const qint64 rate = a.value(QStringLiteral("BANDWIDTH")).toLongLong();
        const Variant variant{uri, a.value(QStringLiteral("AUDIO"))};
        if (height > tallestHeight || (height == tallestHeight && rate > tallestRate)) {
            tallest = variant;
            tallestHeight = height;
            tallestRate = rate;
        }
        if ((height <= maxHeight) && (height > bestHeight || (height == bestHeight && rate > bestRate))) {
            best = variant;
            bestHeight = height;
            bestRate = rate;
        }
    }
    return best.uri.isEmpty() ? tallest : best;
}

void streamToFile(const QUrl &url, const Headers &headers, const QString &path,
                  const std::function<void(qint64, qint64)> &progress) {
    const auto partPath = path + QStringLiteral(".part");
    QFile part(partPath);
    if (!part.open(QIODevice::ReadWrite | QIODevice::Append))
        throw Error(QStringLiteral("Could not write to the downloads folder."));
    qint64 have = part.size();
    QNetworkAccessManager network;
    network.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Http::userAgent());
    for (const auto &[name, value] : headers)
        request.setRawHeader(name, value);
    if (have > 0)
        request.setRawHeader("Range", "bytes=" + QByteArray::number(have) + '-');
    // Given up when nothing arrives for a minute, however long it all takes.
    request.setTransferTimeout(60000);
    auto *reply = network.get(request);
    qint64 total = -1;
    QString failure;
    bool complete = false;
    QElapsedTimer reported;
    reported.start();
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::metaDataChanged, &loop, [&] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 416) {
            // Asked past the end: it is all here.
            complete = true;
            reply->abort();
            return;
        }
        if (status == 200 && have > 0) {
            // The server sends it whole again.
            part.resize(0);
            have = 0;
        }
        const auto length = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (length > 0)
            total = have + length;
    });
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&] {
        const auto data = reply->readAll();
        if (part.write(data) != data.size()) {
            failure = QStringLiteral("Could not write to the downloads folder (is the disk full?).");
            reply->abort();
            return;
        }
        have += data.size();
        if (reported.elapsed() > 500) {
            reported.restart();
            progress(have, total);
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (CancelScope::cancelled())
            reply->abort();
    });
    poll.start(300);
    loop.exec();
    poll.stop();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply->error();
    const auto message = reply->errorString();
    if (error == QNetworkReply::NoError && reply->bytesAvailable() > 0)
        part.write(reply->readAll());
    delete reply;
    part.close();
    if (CancelScope::cancelled())
        throw Error(QStringLiteral("Cancelled"), 499);
    if (!failure.isEmpty())
        throw Error(failure);
    if (!complete) {
        if (error != QNetworkReply::NoError)
            throw Error(status >= 400 ? QStringLiteral("The video server answered %1.").arg(status)
                                      : QStringLiteral("The download stopped: %1").arg(message),
                        status);
        if (total > 0 && QFileInfo(partPath).size() < total)
            throw Error(QStringLiteral("The download stopped before the end; it goes on from there next time."));
    }
    QFile::remove(path);
    if (!QFile::rename(partPath, path))
        throw Error(QStringLiteral("Could not write to the downloads folder."));
    progress(QFileInfo(path).size(), QFileInfo(path).size());
}

Saved save(const QUrl &url, const Headers &headers, const QString &folder,
           const std::function<void(qint64, qint64)> &progress) {
    QDir().mkpath(folder);
    bool hls = url.path().endsWith(QLatin1String(".m3u8"), Qt::CaseInsensitive);
    QString master;
    if (!hls) {
        // Asked for its start only: a playlist says so in its first line.
        try {
            const auto head = fetch(url, headers, "0-2047", 4 * 1024 * 1024);
            if (head.body.startsWith("#EXTM3U") || head.contentType.contains("mpegurl")) {
                hls = true;
                if (head.status == 200)
                    master = QString::fromUtf8(head.body);
            }
        } catch (const Error &e) {
            if (e.status() == 499)
                throw;
            // Too large to be a playlist (the Range was ignored), or no
            // answer to it: a plain file, which streamToFile tries again.
        }
    }
    if (!hls) {
        auto suffix = QFileInfo(url.path()).suffix().toLower();
        if (!QStringList{QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("m4v"), QStringLiteral("mov"),
                         QStringLiteral("ts")}
                 .contains(suffix))
            suffix = QStringLiteral("mp4");
        const auto name = QStringLiteral("video.") + suffix;
        streamToFile(url, headers, folder + QLatin1Char('/') + name, progress);
        return {name, false};
    }
    QUrl base = url;
    if (master.isEmpty()) {
        const auto response = fetch(url, headers, {}, 16 * 1024 * 1024);
        master = QString::fromUtf8(response.body);
        if (response.finalUrl.isValid())
            base = response.finalUrl;
    }
    qint64 done = 0, total = 0;
    if (!master.contains(QLatin1String("#EXT-X-STREAM-INF"))) {
        saveMedia(url, headers, folder, QStringLiteral("v"), QStringLiteral("video.m3u8"), done, total, progress);
        return {QStringLiteral("video.m3u8"), true};
    }
    const auto variant = pickVariant(master);
    // A separate audio track of the variant's group (the default one).
    QString audioUri;
    if (!variant.audioGroup.isEmpty())
        for (const auto &line : master.split(QLatin1Char('\n'))) {
            if (!line.startsWith(QLatin1String("#EXT-X-MEDIA:")))
                continue;
            const auto a = attributes(line.trimmed());
            if (a.value(QStringLiteral("TYPE")) != QLatin1String("AUDIO") || a.value(QStringLiteral("GROUP-ID")) != variant.audioGroup ||
                a.value(QStringLiteral("URI")).isEmpty())
                continue;
            if (audioUri.isEmpty() || a.value(QStringLiteral("DEFAULT")) == QLatin1String("YES"))
                audioUri = a.value(QStringLiteral("URI"));
        }
    saveMedia(base.resolved(QUrl(variant.uri)), headers, folder, QStringLiteral("v"), QStringLiteral("v.m3u8"), done, total, progress);
    QString out = QStringLiteral("#EXTM3U\n");
    if (!audioUri.isEmpty()) {
        saveMedia(base.resolved(QUrl(audioUri)), headers, folder, QStringLiteral("a"), QStringLiteral("a.m3u8"), done, total, progress);
        out += QStringLiteral("#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"a\",NAME=\"audio\",DEFAULT=YES,AUTOSELECT=YES,URI=\"a.m3u8\"\n"
                              "#EXT-X-STREAM-INF:BANDWIDTH=1,AUDIO=\"a\"\nv.m3u8\n");
    } else {
        out += QStringLiteral("#EXT-X-STREAM-INF:BANDWIDTH=1\nv.m3u8\n");
    }
    writeFile(folder + QStringLiteral("/video.m3u8"), out.toUtf8());
    return {QStringLiteral("video.m3u8"), true};
}

} // namespace sc::video
