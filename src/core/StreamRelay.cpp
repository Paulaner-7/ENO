#include "StreamRelay.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <algorithm>
#include <memory>
#include <utility>

namespace sc::video {

namespace {

// How far into a dressed-up part its media is looked for, and into one
// that is neither dressed up nor a format known at its first bytes.
constexpr qsizetype kDressedWindow = 512 * 1024;
constexpr qsizetype kPlainWindow = 64 * 1024;
constexpr qint64 kMaxPlaylist = 16 * 1024 * 1024;
// Written to the player ahead of what it has read; more is read from the
// server once it takes some.
constexpr qint64 kWriteAhead = 4 * 1024 * 1024;
constexpr qint64 kReadBuffer = 2 * 1024 * 1024;
// Streams kept (the newest), and addresses kept per stream (a live stream
// adds parts for as long as it runs).
constexpr int kStreams = 16;
constexpr int kEntries = 20000;

// ------------------------------------------------------------ media start

// Transport stream packets in a row from `at` (188 bytes each, starting
// with 0x47), up to `want`.
int packetsAt(const QByteArray &d, qsizetype at, int want) {
    int n = 0;
    for (qsizetype i = at; n < want && i < d.size() && uchar(d[i]) == 0x47; i += 188)
        ++n;
    return n;
}

// An ISO BMFF box (fragmented MP4) that can start a part.
bool boxAt(const QByteArray &d, qsizetype at) {
    if (at + 8 > d.size())
        return false;
    static const QSet<QByteArray> types{"ftyp", "styp", "moof", "sidx", "moov", "emsg", "prft"};
    if (!types.contains(d.mid(at + 4, 4)))
        return false;
    const quint32 size = quint32(uchar(d[at])) << 24 | quint32(uchar(d[at + 1])) << 16 | quint32(uchar(d[at + 2])) << 8 | uchar(d[at + 3]);
    return size == 1 || size >= 8;
}

// A picture or a page in front of the media.
bool dressed(const QByteArray &d) {
    return d.startsWith("\x89PNG") || d.startsWith("\xFF\xD8\xFF") || d.startsWith("GIF8") || d.startsWith("BM") ||
           (d.startsWith("RIFF") && d.mid(8, 4) == "WEBP") || d.startsWith('<');
}

// Formats a part may start with, known at their first bytes. -1: too few
// bytes to tell.
int knownAtStart(const QByteArray &d, bool complete) {
    if (d.startsWith("WEBVTT") || d.startsWith("\xEF\xBB\xBFWEBVTT") || d.startsWith("ID3") || boxAt(d, 0))
        return 1;
    if (d.size() >= 2) {
        const auto a = uchar(d[0]), b = uchar(d[1]);
        // ADTS or MPEG audio, AC-3, MPEG program stream.
        if ((a == 0xFF && (b & 0xE0) == 0xE0) || (a == 0x0B && b == 0x77) || d.startsWith(QByteArray("\0\0\x01\xBA", 4)))
            return 1;
    }
    if (!d.isEmpty() && uchar(d[0]) == 0x47) {
        if (packetsAt(d, 0, 2) == 2)
            return 1;
        if (d.size() <= 188)
            return complete ? 1 : -1;
    }
    return d.size() < 8 && !complete ? -1 : 0;
}

// The type of what a part starts with, or empty.
QByteArray partType(const QByteArray &d) {
    if (!d.isEmpty() && uchar(d[0]) == 0x47)
        return "video/mp2t";
    if (boxAt(d, 0))
        return "video/mp4";
    if (d.startsWith("WEBVTT") || d.startsWith("\xEF\xBB\xBFWEBVTT"))
        return "text/vtt";
    if (d.startsWith("ID3") || (d.size() >= 2 && uchar(d[0]) == 0xFF && (uchar(d[1]) & 0xF6) == 0xF0))
        return "audio/aac";
    return {};
}

// ------------------------------------------------------------- playlists

QHash<QString, QString> attributes(const QString &line) {
    QHash<QString, QString> out;
    static const QRegularExpression pair(QStringLiteral(R"(([A-Z0-9-]+)=("[^"]*"|[^,]*))"));
    auto it = pair.globalMatch(line.mid(line.indexOf(QLatin1Char(':')) + 1));
    while (it.hasNext()) {
        const auto m = it.next();
        auto value = m.captured(2);
        if (value.startsWith(QLatin1Char('"')))
            value = value.mid(1, value.size() - 2);
        out.insert(m.captured(1), value);
    }
    return out;
}

// --------------------------------------------------------------- the relay

struct Entry {
    QString key;
    QUrl url;
    Part kind = Part::Media;
    bool subtitles = false;
    // Where its media started when it was last sent whole; -1 until then.
    qint64 strip = -1;
};

struct Stream {
    QByteArray token;
    Headers headers;
    QHash<int, Entry> entries;
    QHash<QString, int> ids;
    int next = 0;
};

// Lives on its own thread: the server, the network access for every
// connection, and the streams the player was given.
class Relay : public QObject {
public:
    static Relay *get();

    QUrl add(const QUrl &url, const Headers &headers) {
        QMutexLocker lock(&m_mutex);
        auto stream = std::make_shared<Stream>();
        stream->token = QByteArray::number(QRandomGenerator::system()->generate64(), 36) +
                        QByteArray::number(QRandomGenerator::system()->generate64(), 36);
        stream->headers = headers;
        m_streams.append(stream);
        while (m_streams.size() > kStreams)
            m_streams.removeFirst();
        const auto suffix = QFileInfo(url.path()).suffix().toLower();
        const bool hls = suffix == QLatin1String("m3u8") || suffix == QLatin1String("m3u") || url.toString().contains(QLatin1String("m3u8"));
        return QUrl(QString::fromLatin1(address(*stream, url, Part::Playlist, hls ? QStringLiteral("m3u8") : QString(), false)));
    }

    // On the relay's thread.
    bool find(const QByteArray &token, int id, Entry &entry, Headers &headers) {
        QMutexLocker lock(&m_mutex);
        for (const auto &stream : std::as_const(m_streams))
            if (stream->token == token) {
                const auto it = stream->entries.constFind(id);
                if (it == stream->entries.cend())
                    return false;
                entry = *it;
                headers = stream->headers;
                return true;
            }
        return false;
    }

    void setStrip(const QByteArray &token, int id, qint64 strip) {
        QMutexLocker lock(&m_mutex);
        for (const auto &stream : std::as_const(m_streams))
            if (stream->token == token && stream->entries.contains(id))
                stream->entries[id].strip = strip;
    }

    QByteArray rewrite(const QByteArray &token, const QByteArray &text, const QUrl &base, bool subtitles) {
        QMutexLocker lock(&m_mutex);
        std::shared_ptr<Stream> stream;
        for (const auto &s : std::as_const(m_streams))
            if (s->token == token)
                stream = s;
        if (!stream)
            return {};
        return rewritePlaylist(text, base, subtitles, [&](const QUrl &url, Part kind, const QString &suffix, bool subs) {
            return address(*stream, url, kind, suffix, subs);
        });
    }

    QNetworkAccessManager *network() const { return m_network; }

private:
    Relay() = default;
    void start();

    // With the mutex held.
    QByteArray address(Stream &stream, const QUrl &url, Part kind, const QString &suffix, bool subtitles) {
        const auto key = QString::number(int(kind)) + QLatin1Char(subtitles ? 's' : '-') + url.toString(QUrl::FullyEncoded);
        auto id = stream.ids.value(key, -1);
        if (id < 0) {
            id = stream.next++;
            stream.ids.insert(key, id);
            stream.entries.insert(id, Entry{key, url, kind, subtitles});
            if (const auto old = stream.entries.constFind(id - kEntries); old != stream.entries.cend()) {
                stream.ids.remove(old->key);
                stream.entries.erase(old);
            }
        }
        auto out = "http://127.0.0.1:" + QByteArray::number(m_port) + '/' + stream.token + '/' + QByteArray::number(id);
        if (!suffix.isEmpty())
            out += '.' + suffix.toLatin1();
        return out;
    }

    QMutex m_mutex;
    QList<std::shared_ptr<Stream>> m_streams;
    QTcpServer *m_server = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    quint16 m_port = 0;
};

// One player connection: requests one after another (kept alive), each
// answered from the video's server.
class Connection : public QObject {
public:
    Connection(Relay *relay, QTcpSocket *socket) : m_relay(relay), m_socket(socket) {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this] {
            m_input += m_socket->readAll();
            if (!m_reply && !m_busy)
                next();
        });
        connect(socket, &QTcpSocket::bytesWritten, this, [this] { pump(); });
        connect(socket, &QTcpSocket::disconnected, this, [this] { close(); });
    }

    ~Connection() override { dropReply(); }

private:
    // How the answer being sent is made.
    enum class Mode {
        Probe,    // the first bytes say whether it is a playlist
        Playlist, // all of it, rewritten
        Sniff,    // a part: its first bytes say where its media starts
        Pass,     // as it comes
    };

    void next() {
        const auto end = m_input.indexOf("\r\n\r\n");
        if (end < 0) {
            if (m_input.size() > 64 * 1024)
                close();
            return;
        }
        const auto head = m_input.left(end);
        m_input.remove(0, end + 4);
        const auto lines = head.split('\n');
        const auto request = lines.value(0).trimmed().split(' ');
        m_keepAlive = request.value(2) == "HTTP/1.1";
        QByteArray range;
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const auto line = lines[i].trimmed();
            const auto colon = line.indexOf(':');
            const auto name = line.left(colon).trimmed().toLower();
            const auto value = line.mid(colon + 1).trimmed();
            if (name == "range")
                range = value;
            else if (name == "connection")
                m_keepAlive = m_keepAlive && value.toLower() != "close";
        }
        if (request.value(0) != "GET")
            return respond(405);
        // /<token>/<id>[.<suffix>]
        const auto parts = request.value(1).split('/');
        bool ok = false;
        const int id = parts.value(2).split('.').value(0).toInt(&ok);
        Headers headers;
        if (parts.size() != 3 || !ok || !m_relay->find(parts.value(1), id, m_entry, headers))
            return respond(404);
        m_token = parts.value(1);
        m_id = id;
        start(headers, range);
    }

    void start(const Headers &headers, const QByteArray &range) {
        QNetworkRequest request(m_entry.url);
        bool agent = false;
        for (const auto &[name, value] : headers) {
            request.setRawHeader(name, value);
            agent = agent || name.compare("user-agent", Qt::CaseInsensitive) == 0;
        }
        if (!agent)
            request.setHeader(QNetworkRequest::UserAgentHeader, Http::userAgent());
        // Sent as the server has it: lengths stay true.
        request.setRawHeader("Accept-Encoding", "identity");
        request.setTransferTimeout(30000);
        // bytes=<from>-[<to>]; from the start and to the end is no range.
        qint64 from = 0, to = -1;
        if (range.startsWith("bytes=")) {
            const auto span = range.mid(6).split('-');
            from = span.value(0).toLongLong();
            to = span.value(1).isEmpty() ? -1 : span.value(1).toLongLong();
        }
        const bool whole = from == 0 && to < 0;
        m_shift = 0;
        m_cut = 0;
        m_held.clear();
        m_headersSent = false;
        m_upstreamDone = false;
        switch (m_entry.kind) {
        case Part::Playlist:
            m_mode = Mode::Probe;
            if (!range.isEmpty())
                request.setRawHeader("Range", range);
            break;
        case Part::Media:
            if (whole) {
                m_mode = Mode::Sniff;
                break;
            }
            // Further into a part sent before: as far further in the
            // server's copy as its dressing was long.
            m_mode = Mode::Pass;
            m_shift = std::max<qint64>(m_entry.strip, 0);
            request.setRawHeader("Range", "bytes=" + QByteArray::number(from + m_shift) + '-' +
                                              (to < 0 ? QByteArray() : QByteArray::number(to + m_shift)));
            break;
        case Part::Raw:
            m_mode = Mode::Pass;
            if (!range.isEmpty())
                request.setRawHeader("Range", range);
            break;
        }
        m_busy = true;
        m_reply = m_relay->network()->get(request);
        m_reply->setReadBufferSize(kReadBuffer);
        connect(m_reply, &QNetworkReply::metaDataChanged, this, [this] {
            // A refusal is passed on as it is.
            if (status() >= 400)
                m_mode = Mode::Pass;
        });
        connect(m_reply, &QNetworkReply::readyRead, this, [this] { pump(); });
        connect(m_reply, &QNetworkReply::finished, this, [this] {
            m_upstreamDone = true;
            pump();
        });
    }

    int status() const { return m_reply ? m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0; }

    // Moves the answer along: on new data, when the player took some, and
    // when the server is done.
    void pump() {
        if (!m_reply)
            return;
        if (m_upstreamDone && !m_headersSent && status() == 0) {
            // No answer at all (unreachable, timed out).
            dropReply();
            return respond(502);
        }
        switch (m_mode) {
        case Mode::Probe: {
            m_held += m_reply->readAll();
            auto head = m_held.left(64);
            if (head.startsWith("\xEF\xBB\xBF"))
                head.remove(0, 3);
            head = head.trimmed();
            if (head.size() < 7 && !m_upstreamDone)
                return;
            if (!head.startsWith("#EXTM3U")) {
                m_mode = Mode::Pass;
                sendHeaders();
                return pump();
            }
            m_mode = Mode::Playlist;
            [[fallthrough]];
        }
        case Mode::Playlist:
            m_held += m_reply->readAll();
            if (m_held.size() > kMaxPlaylist) {
                dropReply();
                return respond(502);
            }
            if (!m_upstreamDone)
                return;
            {
                // Addresses in it are relative to where it ended up.
                const auto text = m_relay->rewrite(m_token, m_held, m_reply->url(), m_entry.subtitles);
                dropReply();
                if (text.isEmpty())
                    return respond(404);
                return respond(200, text, "application/vnd.apple.mpegurl");
            }
        case Mode::Sniff: {
            m_held += m_reply->readAll();
            const auto start = mediaStart(m_held, m_upstreamDone);
            if (start < 0)
                return;
            m_relay->setStrip(m_token, m_id, start);
            m_held.remove(0, start);
            m_cut = start;
            m_mode = Mode::Pass;
            sendHeaders();
            return pump();
        }
        case Mode::Pass:
            if (!m_headersSent)
                sendHeaders();
            if (!m_held.isEmpty()) {
                m_socket->write(m_held);
                m_held.clear();
            }
            while (m_socket->bytesToWrite() < kWriteAhead && m_reply->bytesAvailable() > 0)
                m_socket->write(m_reply->read(kReadBuffer));
            if (m_upstreamDone && m_reply->bytesAvailable() == 0)
                finish();
            return;
        }
    }

    // The status and headers of the server's answer, the length less what
    // was cut off and a range in the player's terms.
    void sendHeaders() {
        m_headersSent = true;
        const int code = status();
        QByteArray out;
        // A part says what it holds, not what it was dressed as (FFmpeg
        // weighs the type when it looks at a part).
        const auto type = m_mode == Mode::Pass && m_entry.kind == Part::Media && code < 300 ? partType(m_held)
                                                                                           : m_reply->rawHeader("Content-Type");
        if (!type.isEmpty())
            out += "Content-Type: " + type + "\r\n";
        const auto encoded = m_reply->rawHeader("Content-Encoding");
        if (m_reply->hasRawHeader("Content-Length") && (encoded.isEmpty() || encoded == "identity"))
            out += "Content-Length: " + QByteArray::number(m_reply->rawHeader("Content-Length").toLongLong() - m_cut) + "\r\n";
        else
            m_keepAlive = false; // its end is where the connection ends
        if (const auto ranges = m_reply->rawHeader("Accept-Ranges"); !ranges.isEmpty())
            out += "Accept-Ranges: " + ranges + "\r\n";
        if (auto span = m_reply->rawHeader("Content-Range"); !span.isEmpty()) {
            static const QRegularExpression re(QStringLiteral(R"(bytes\s+(\d+)-(\d+)/(\d+|\*))"));
            if (const auto m = re.match(QString::fromLatin1(span)); m.hasMatch() && m_shift > 0) {
                const auto total = m.captured(3);
                span = "bytes " + QByteArray::number(m.captured(1).toLongLong() - m_shift) + '-' +
                       QByteArray::number(m.captured(2).toLongLong() - m_shift) + '/' +
                       (total == QLatin1String("*") ? QByteArray("*") : QByteArray::number(total.toLongLong() - m_shift));
            }
            out += "Content-Range: " + span + "\r\n";
        }
        writeHead(code ? code : 502, out);
    }

    void respond(int code, const QByteArray &body = {}, const QByteArray &type = "text/plain") {
        m_headersSent = true;
        writeHead(code, "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n");
        m_socket->write(body);
        finish();
    }

    void writeHead(int code, const QByteArray &headers) {
        m_socket->write("HTTP/1.1 " + QByteArray::number(code) + (code < 300 ? " OK" : " Error") + "\r\n" + headers +
                        (m_keepAlive ? "Connection: keep-alive\r\n\r\n" : "Connection: close\r\n\r\n"));
    }

    // The answer is complete: the next request, or the end.
    void finish() {
        const bool clean = !m_reply || m_reply->error() == QNetworkReply::NoError || status() >= 400;
        dropReply();
        m_busy = false;
        if (!m_keepAlive || !clean) {
            // A cut-off answer must not look whole: the player sees the
            // connection end and asks again.
            m_socket->disconnectFromHost();
            return;
        }
        if (!m_input.isEmpty())
            next();
    }

    void dropReply() {
        if (!m_reply)
            return;
        auto *reply = std::exchange(m_reply, nullptr);
        reply->disconnect(this);
        if (!reply->isFinished())
            reply->abort();
        reply->deleteLater();
    }

    void close() {
        dropReply();
        deleteLater();
    }

    Relay *m_relay;
    QTcpSocket *m_socket;
    QByteArray m_input;
    QNetworkReply *m_reply = nullptr;
    bool m_busy = false;
    bool m_keepAlive = true;
    bool m_headersSent = false;
    bool m_upstreamDone = false;
    Mode m_mode = Mode::Pass;
    Entry m_entry;
    QByteArray m_token;
    int m_id = 0;
    QByteArray m_held;
    // Taken off the start of a whole part; added to a range's offsets.
    qint64 m_cut = 0;
    qint64 m_shift = 0;
};

Relay *Relay::get() {
    static Relay *relay = [] {
        auto *r = new Relay;
        auto *thread = new QThread;
        thread->setObjectName(QStringLiteral("stream-relay"));
        r->moveToThread(thread);
        thread->start();
        QMetaObject::invokeMethod(r, [r] { r->start(); }, Qt::BlockingQueuedConnection);
        if (auto *app = QCoreApplication::instance())
            QObject::connect(app, &QCoreApplication::aboutToQuit, thread, [thread] { thread->quit(); }, Qt::DirectConnection);
        return r;
    }();
    return relay->m_port ? relay : nullptr;
}

void Relay::start() {
    m_network = new QNetworkAccessManager(this);
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        qWarning("stream relay: cannot listen (%s)", qPrintable(m_server->errorString()));
        return;
    }
    m_port = m_server->serverPort();
    connect(m_server, &QTcpServer::newConnection, this, [this] {
        while (auto *socket = m_server->nextPendingConnection())
            new Connection(this, socket);
    });
}

} // namespace

qsizetype mediaStart(const QByteArray &head, bool complete) {
    const bool wrapped = dressed(head);
    if (!wrapped) {
        const int known = knownAtStart(head, complete);
        if (known != 0)
            return known > 0 ? 0 : -1;
    }
    const qsizetype window = std::min(head.size(), wrapped ? kDressedWindow : kPlainWindow);
    for (qsizetype i = 1; i < window; ++i) {
        if (uchar(head[i]) == 0x47) {
            const int n = packetsAt(head, i, 5);
            if (n == 5)
                return i;
            // Packets as far as the bytes go: wait for more, or (all of
            // it here) take a short dressed part's.
            if (i + 188 * qsizetype(n) >= head.size()) {
                if (!complete)
                    return -1;
                if (wrapped)
                    return i;
            }
        }
        if (wrapped && boxAt(head, i))
            return i;
    }
    if (!complete && head.size() < (wrapped ? kDressedWindow : kPlainWindow))
        return -1;
    return 0;
}

QByteArray rewritePlaylist(const QByteArray &text, const QUrl &base, bool subtitlePlaylist, const RelayAddress &address) {
    const auto source = QString::fromUtf8(text);
    const bool master = source.contains(QLatin1String("#EXT-X-STREAM-INF"));
    const bool fragmented = source.contains(QLatin1String("#EXT-X-MAP"));
    // FFmpeg checks a part's name against what it finds in it: transport
    // streams, fragmented MP4 and AAC pass as .ts or .m4s, the rest need
    // their own.
    const auto partSuffix = [&](const QUrl &url) {
        static const QSet<QString> own{"mp3", "mp2", "ac3", "ec3", "eac3", "vtt", "webvtt"};
        const auto suffix = QFileInfo(url.path()).suffix().toLower();
        if (own.contains(suffix))
            return suffix;
        return subtitlePlaylist ? QStringLiteral("vtt") : fragmented ? QStringLiteral("m4s") : QStringLiteral("ts");
    };
    static const QRegularExpression uriAttr(QStringLiteral(R"re(URI="([^"]*)")re"));
    bool encrypted = false;
    bool ranged = false;
    QByteArray out;
    out.reserve(text.size() * 2);
    for (auto line : source.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (!line.startsWith(QLatin1Char('#'))) {
            const auto url = base.resolved(QUrl(line));
            if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")) {
                out += line.toUtf8() + '\n';
                continue;
            }
            const auto kind = master ? Part::Playlist : (encrypted || ranged) ? Part::Raw : Part::Media;
            out += address(url, kind, master ? QStringLiteral("m3u8") : partSuffix(url), master ? false : subtitlePlaylist) + '\n';
            ranged = false;
            continue;
        }
        const auto tag = line.section(QLatin1Char(':'), 0, 0);
        if (tag == QLatin1String("#EXT-X-KEY"))
            encrypted = attributes(line).value(QStringLiteral("METHOD"), QStringLiteral("NONE")) != QLatin1String("NONE");
        else if (tag == QLatin1String("#EXT-X-BYTERANGE"))
            ranged = true;
        const auto m = uriAttr.match(line);
        if (!m.hasMatch()) {
            out += line.toUtf8() + '\n';
            continue;
        }
        const auto a = attributes(line);
        const auto url = base.resolved(QUrl(m.captured(1)));
        Part kind;
        QString suffix;
        bool subtitles = false;
        if (tag == QLatin1String("#EXT-X-MEDIA") || tag == QLatin1String("#EXT-X-I-FRAME-STREAM-INF") ||
            tag == QLatin1String("#EXT-X-RENDITION-REPORT")) {
            kind = Part::Playlist;
            suffix = QStringLiteral("m3u8");
            subtitles = a.value(QStringLiteral("TYPE")) == QLatin1String("SUBTITLES");
        } else if (tag == QLatin1String("#EXT-X-KEY") || tag == QLatin1String("#EXT-X-SESSION-KEY")) {
            kind = Part::Raw;
            suffix = QStringLiteral("key");
        } else if (tag == QLatin1String("#EXT-X-MAP") ||
                   (tag == QLatin1String("#EXT-X-PRELOAD-HINT") && a.value(QStringLiteral("TYPE")) == QLatin1String("MAP"))) {
            kind = Part::Raw;
            suffix = QStringLiteral("mp4");
        } else if (tag == QLatin1String("#EXT-X-PART") || tag == QLatin1String("#EXT-X-PRELOAD-HINT")) {
            const bool partRange = a.contains(QStringLiteral("BYTERANGE")) || a.contains(QStringLiteral("BYTERANGE-START"));
            kind = encrypted || partRange ? Part::Raw : Part::Media;
            suffix = partSuffix(url);
            subtitles = subtitlePlaylist;
        } else {
            out += line.toUtf8() + '\n';
            continue;
        }
        if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")) {
            out += line.toUtf8() + '\n';
            continue;
        }
        const auto replaced = QString::fromLatin1(address(url, kind, suffix, subtitles));
        out += (line.left(m.capturedStart(1)) + replaced + line.mid(m.capturedEnd(1))).toUtf8() + '\n';
    }
    return out;
}

QUrl StreamRelay::wrap(const QUrl &url, const Headers &headers) {
    auto *relay = Relay::get();
    return relay ? relay->add(url, headers) : url;
}

bool StreamRelay::wanted(const QUrl &url) {
    if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
        return false;
    static const QSet<QString> files{"mp4", "m4v", "mkv", "webm", "mov", "avi", "flv", "wmv", "mp3", "m4a", "ogg", "ogv", "opus", "flac", "wav"};
    return !files.contains(QFileInfo(url.path()).suffix().toLower());
}

} // namespace sc::video
