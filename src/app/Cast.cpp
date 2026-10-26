#include "Cast.h"

#include "App.h"
#include "CastProxy.h"
#include "Platform.h"
#include "core/Http.h"
#include "core/StreamRelay.h"

#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QSslSocket>
#include <QTime>
#include <QUdpSocket>
#include <QXmlStreamReader>
#include <QtEndian>

namespace sc {

namespace {

const QHostAddress kSsdpGroup(QStringLiteral("239.255.255.250"));
const QHostAddress kMdnsGroup(QStringLiteral("224.0.0.251"));
constexpr int kSearchMs = 5000;
const QString kDefaultReceiver = QStringLiteral("CC1AD845");

// "H:MM:SS(.fff)" as seconds; -1 when it is not a time.
double parseTime(const QString &text) {
    const auto parts = text.trimmed().split(QLatin1Char(':'));
    if (parts.size() != 3)
        return -1;
    bool ok = true;
    double seconds = 0;
    for (const auto &part : parts) {
        seconds = seconds * 60 + part.toDouble(&ok);
        if (!ok)
            return -1;
    }
    return seconds;
}

QString formatTime(double seconds) {
    const auto s = qint64(std::max(0.0, seconds));
    return QStringLiteral("%1:%2:%3").arg(s / 3600).arg(s / 60 % 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString escapeXml(const QString &text) { return text.toHtmlEscaped(); }

// ------------------------------------------------------------------ mDNS

// A DNS name at `offset`, following compression pointers.
QString dnsName(const QByteArray &packet, int &offset) {
    QStringList labels;
    int position = offset;
    bool jumped = false;
    for (int guard = 0; guard < 64 && position < packet.size(); ++guard) {
        const auto length = quint8(packet.at(position));
        if (length == 0) {
            ++position;
            break;
        }
        if ((length & 0xC0) == 0xC0) {
            if (position + 1 >= packet.size())
                break;
            const int pointer = ((length & 0x3F) << 8) | quint8(packet.at(position + 1));
            if (!jumped)
                offset = position + 2;
            jumped = true;
            position = pointer;
            continue;
        }
        labels.append(QString::fromUtf8(packet.mid(position + 1, length)));
        position += 1 + length;
    }
    if (!jumped)
        offset = position;
    return labels.join(QLatin1Char('.'));
}

QByteArray mdnsQuery() {
    QByteArray packet(12, '\0');
    packet[5] = 1; // one question
    for (const auto *label : {"_googlecast", "_tcp", "local"}) {
        packet.append(char(qstrlen(label)));
        packet.append(label);
    }
    packet.append('\0');
    packet.append("\x00\x0c", 2); // PTR
    packet.append("\x80\x01", 2); // IN, unicast reply wanted
    return packet;
}

// ------------------------------------------------------------------ DLNA

class DlnaSession : public CastSession {
public:
    DlnaSession(const Cast::Device &device, QNetworkAccessManager *network) : CastSession(device), m_network(network) {
        m_poll.setInterval(1000);
        connect(&m_poll, &QTimer::timeout, this, [this] { poll(); });
    }

    void load(const QString &url, const QString &type, const QString &title, const QString &, double startAt, const QVariantList &) override {
        m_startAt = startAt;
        const auto mime = type.contains(QLatin1String("mpegurl")) ? QStringLiteral("application/vnd.apple.mpegurl") : type;
        const auto metadata =
            QStringLiteral("<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
                           "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\"><item id=\"0\" parentID=\"-1\" restricted=\"1\">"
                           "<dc:title>%1</dc:title><upnp:class>object.item.videoItem</upnp:class>"
                           "<res protocolInfo=\"http-get:*:%2:*\">%3</res></item></DIDL-Lite>")
                .arg(escapeXml(title), mime, escapeXml(url));
        Cast::instance()->sessionState(QStringLiteral("connecting"));
        soap(QStringLiteral("SetAVTransportURI"),
             QStringLiteral("<CurrentURI>%1</CurrentURI><CurrentURIMetaData>%2</CurrentURIMetaData>").arg(escapeXml(url), escapeXml(metadata)),
             [this](bool ok, const QString &) {
                 if (!ok)
                     return Cast::instance()->sessionState(QStringLiteral("error"), QStringLiteral("The TV did not take the video."));
                 soap(QStringLiteral("Play"), QStringLiteral("<Speed>1</Speed>"), [this](bool, const QString &) { m_poll.start(); });
             });
    }
    void pause() override { soap(QStringLiteral("Pause"), {}, {}); }
    void resume() override { soap(QStringLiteral("Play"), QStringLiteral("<Speed>1</Speed>"), {}); }
    void seek(double seconds) override {
        soap(QStringLiteral("Seek"), QStringLiteral("<Unit>REL_TIME</Unit><Target>%1</Target>").arg(formatTime(seconds)), {});
    }
    void stop() override {
        m_poll.stop();
        soap(QStringLiteral("Stop"), {}, {});
    }

private:
    void soap(const QString &action, const QString &arguments, std::function<void(bool, const QString &)> done) {
        const auto body = QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                                         "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                         "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>"
                                         "<u:%1 xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID>%2</u:%1>"
                                         "</s:Body></s:Envelope>")
                              .arg(action, arguments);
        QNetworkRequest request(m_device.control);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("text/xml; charset=\"utf-8\""));
        request.setRawHeader("SOAPACTION", ("\"urn:schemas-upnp-org:service:AVTransport:1#" + action + "\"").toUtf8());
        request.setTransferTimeout(8000);
        auto *reply = m_network->post(request, body.toUtf8());
        connect(reply, &QNetworkReply::finished, this, [reply, done] {
            reply->deleteLater();
            if (done)
                done(reply->error() == QNetworkReply::NoError, QString::fromUtf8(reply->readAll()));
        });
    }

    static QString field(const QString &xml, const QString &name) {
        const auto start = xml.indexOf(QLatin1Char('<') + name + QLatin1Char('>'));
        if (start < 0)
            return {};
        const auto from = start + name.size() + 2;
        return xml.mid(from, xml.indexOf(QLatin1Char('<'), from) - from);
    }

    void poll() {
        soap(QStringLiteral("GetTransportInfo"), {}, [this](bool ok, const QString &xml) {
            if (!ok)
                return;
            const auto state = field(xml, QStringLiteral("CurrentTransportState"));
            if (state == QLatin1String("PLAYING")) {
                // The start position, once the TV plays (many ignore a seek before).
                if (m_startAt > 1) {
                    seek(std::exchange(m_startAt, 0.0));
                }
                m_played = true;
                Cast::instance()->sessionState(QStringLiteral("playing"));
            } else if (state == QLatin1String("PAUSED_PLAYBACK")) {
                Cast::instance()->sessionState(QStringLiteral("paused"));
            } else if (state == QLatin1String("TRANSITIONING")) {
                Cast::instance()->sessionState(QStringLiteral("buffering"));
            } else if ((state == QLatin1String("STOPPED") || state == QLatin1String("NO_MEDIA_PRESENT")) && m_played) {
                m_poll.stop();
                Cast::instance()->sessionState(QStringLiteral("ended"));
            }
        });
        soap(QStringLiteral("GetPositionInfo"), {}, [](bool ok, const QString &xml) {
            if (!ok)
                return;
            const auto position = parseTime(field(xml, QStringLiteral("RelTime")));
            const auto duration = parseTime(field(xml, QStringLiteral("TrackDuration")));
            if (position >= 0)
                Cast::instance()->sessionPosition(position, std::max(0.0, duration));
        });
    }

    QNetworkAccessManager *m_network;
    QTimer m_poll;
    double m_startAt = 0;
    bool m_played = false;
};

// ------------------------------------------------------------------ Chromecast

// The Cast protocol: length-prefixed protobuf CastMessages over TLS, with
// JSON payloads in a few namespaces.
class ChromecastSession : public CastSession {
public:
    explicit ChromecastSession(const Cast::Device &device) : CastSession(device) {
        m_socket.setPeerVerifyMode(QSslSocket::VerifyNone);
        connect(&m_socket, &QSslSocket::encrypted, this, [this] {
            send(QStringLiteral("receiver-0"), kConnection, {{"type", "CONNECT"}});
            send(QStringLiteral("receiver-0"), kReceiver, {{"type", "LAUNCH"}, {"appId", kDefaultReceiver}, {"requestId", ++m_request}});
            m_heartbeat.start();
        });
        connect(&m_socket, &QSslSocket::readyRead, this, [this] { read(); });
        connect(&m_socket, &QSslSocket::errorOccurred, this, [this] {
            if (!m_stopping)
                Cast::instance()->sessionState(QStringLiteral("error"), QStringLiteral("Lost the connection to the Chromecast."));
        });
        m_heartbeat.setInterval(5000);
        connect(&m_heartbeat, &QTimer::timeout, this, [this] {
            send(QStringLiteral("receiver-0"), kHeartbeat, {{"type", "PING"}});
            if (!m_transport.isEmpty() && m_mediaSession)
                send(m_transport, kMedia, {{"type", "GET_STATUS"}, {"requestId", ++m_request}, {"mediaSessionId", m_mediaSession}});
        });
        m_tick.setInterval(1000);
        connect(&m_tick, &QTimer::timeout, this, [this] {
            if (m_playing)
                Cast::instance()->sessionPosition(m_position += 1, m_duration);
        });
    }

    void load(const QString &url, const QString &type, const QString &title, const QString &cover, double startAt,
              const QVariantList &subtitles) override {
        QJsonObject metadata{{"metadataType", 0}, {"title", title}};
        if (!cover.isEmpty())
            metadata.insert(QStringLiteral("images"), QJsonArray{QJsonObject{{"url", cover}}});
        QJsonObject media{{"contentId", url}, {"contentType", type}, {"streamType", "BUFFERED"}, {"metadata", metadata}};
        QJsonArray tracks, active;
        for (int i = 0; i < subtitles.size(); ++i) {
            const auto track = subtitles[i].toMap();
            tracks.append(QJsonObject{{"trackId", i + 1},
                                      {"type", "TEXT"},
                                      {"subtype", "SUBTITLES"},
                                      {"trackContentId", track.value("url").toString()},
                                      {"trackContentType", "text/vtt"},
                                      {"name", track.value("lang").toString()}});
        }
        if (!tracks.isEmpty()) {
            media.insert(QStringLiteral("tracks"), tracks);
            active.append(1);
        }
        m_load = QJsonObject{{"type", "LOAD"}, {"media", media}, {"autoplay", true}, {"currentTime", startAt}};
        if (!active.isEmpty())
            m_load.insert(QStringLiteral("activeTrackIds"), active);
        m_position = startAt;
        Cast::instance()->sessionState(QStringLiteral("connecting"));
        m_socket.connectToHostEncrypted(m_device.host.toString(), quint16(m_device.port));
    }
    void pause() override { media({{"type", "PAUSE"}}); }
    void resume() override { media({{"type", "PLAY"}}); }
    void seek(double seconds) override {
        m_position = seconds;
        media({{"type", "SEEK"}, {"currentTime", seconds}});
    }
    void stop() override {
        m_stopping = true;
        media({{"type", "STOP"}});
        if (!m_transport.isEmpty())
            send(m_transport, kConnection, {{"type", "CLOSE"}});
        m_socket.flush();
        m_socket.disconnectFromHost();
    }

    // A CastMessage (protobuf) for `payload`, length first.
    static QByteArray frame(const QString &source, const QString &destination, const QString &ns, const QByteArray &payload) {
        QByteArray body;
        const auto varint = [&](quint64 v) {
            do {
                quint8 b = v & 0x7F;
                v >>= 7;
                body.append(char(v ? b | 0x80 : b));
            } while (v);
        };
        const auto string = [&](int field, const QByteArray &value) {
            varint(quint64(field << 3 | 2));
            varint(quint64(value.size()));
            body.append(value);
        };
        varint(1 << 3); // protocol_version = CASTV2_1_0
        varint(0);
        string(2, source.toUtf8());
        string(3, destination.toUtf8());
        string(4, ns.toUtf8());
        varint(5 << 3); // payload_type = STRING
        varint(0);
        string(6, payload);
        QByteArray out(4, '\0');
        qToBigEndian(quint32(body.size()), out.data());
        return out + body;
    }

    // The namespace and payload of a CastMessage.
    static std::pair<QString, QByteArray> parse(const QByteArray &body) {
        QString ns;
        QByteArray payload;
        int i = 0;
        const auto varint = [&]() {
            quint64 v = 0;
            for (int shift = 0; i < body.size() && shift < 64; shift += 7) {
                const auto b = quint8(body.at(i++));
                v |= quint64(b & 0x7F) << shift;
                if (!(b & 0x80))
                    break;
            }
            return v;
        };
        while (i < body.size()) {
            const auto key = varint();
            const int field = int(key >> 3), wire = int(key & 7);
            if (wire == 0) {
                varint();
            } else if (wire == 2) {
                const auto length = int(varint());
                const auto value = body.mid(i, length);
                i += length;
                if (field == 4)
                    ns = QString::fromUtf8(value);
                else if (field == 6)
                    payload = value;
            } else {
                break;
            }
        }
        return {ns, payload};
    }

private:
    static inline const QString kConnection = QStringLiteral("urn:x-cast:com.google.cast.tp.connection");
    static inline const QString kHeartbeat = QStringLiteral("urn:x-cast:com.google.cast.tp.heartbeat");
    static inline const QString kReceiver = QStringLiteral("urn:x-cast:com.google.cast.receiver");
    static inline const QString kMedia = QStringLiteral("urn:x-cast:com.google.cast.media");

    void send(const QString &destination, const QString &ns, const QJsonObject &payload) {
        m_socket.write(frame(QStringLiteral("sender-scancore"), destination, ns, QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    }

    void media(QJsonObject payload) {
        if (m_transport.isEmpty() || !m_mediaSession)
            return;
        payload.insert(QStringLiteral("requestId"), ++m_request);
        payload.insert(QStringLiteral("mediaSessionId"), m_mediaSession);
        send(m_transport, kMedia, payload);
    }

    void read() {
        m_buffer += m_socket.readAll();
        while (m_buffer.size() >= 4) {
            const auto length = qFromBigEndian<quint32>(m_buffer.constData());
            if (length > 1024 * 1024) {
                m_socket.abort();
                return;
            }
            if (m_buffer.size() < 4 + qsizetype(length))
                return;
            const auto [ns, payload] = parse(m_buffer.mid(4, length));
            m_buffer.remove(0, 4 + length);
            handle(ns, QJsonDocument::fromJson(payload).object());
        }
    }

    void handle(const QString &ns, const QJsonObject &message) {
        const auto type = message.value("type").toString();
        if (ns == kHeartbeat && type == QLatin1String("PING")) {
            send(QStringLiteral("receiver-0"), kHeartbeat, {{"type", "PONG"}});
        } else if (ns == kReceiver && type == QLatin1String("RECEIVER_STATUS")) {
            for (const auto &value : message.value("status").toObject().value("applications").toArray()) {
                const auto app = value.toObject();
                if (app.value("appId").toString() != kDefaultReceiver || !m_transport.isEmpty())
                    continue;
                m_transport = app.value("transportId").toString();
                send(m_transport, kConnection, {{"type", "CONNECT"}});
                auto load = m_load;
                load.insert(QStringLiteral("requestId"), ++m_request);
                send(m_transport, kMedia, load);
            }
        } else if (ns == kReceiver && type == QLatin1String("LAUNCH_ERROR")) {
            Cast::instance()->sessionState(QStringLiteral("error"), QStringLiteral("The Chromecast could not start its player."));
        } else if (ns == kMedia && (type == QLatin1String("LOAD_FAILED") || type == QLatin1String("LOAD_CANCELLED"))) {
            Cast::instance()->sessionState(QStringLiteral("error"), QStringLiteral("The Chromecast could not play this video."));
        } else if (ns == kMedia && type == QLatin1String("MEDIA_STATUS")) {
            for (const auto &value : message.value("status").toArray()) {
                const auto status = value.toObject();
                m_mediaSession = status.value("mediaSessionId").toInt(m_mediaSession);
                const auto duration = status.value("media").toObject().value("duration").toDouble(-1);
                if (duration > 0)
                    m_duration = duration;
                if (status.contains("currentTime"))
                    m_position = status.value("currentTime").toDouble();
                const auto player = status.value("playerState").toString();
                m_playing = player == QLatin1String("PLAYING");
                m_playing ? m_tick.start() : m_tick.stop();
                if (player == QLatin1String("PLAYING"))
                    Cast::instance()->sessionState(QStringLiteral("playing"));
                else if (player == QLatin1String("PAUSED"))
                    Cast::instance()->sessionState(QStringLiteral("paused"));
                else if (player == QLatin1String("BUFFERING") || player == QLatin1String("LOADING"))
                    Cast::instance()->sessionState(QStringLiteral("buffering"));
                else if (player == QLatin1String("IDLE")) {
                    const auto why = status.value("idleReason").toString();
                    if (why == QLatin1String("FINISHED"))
                        Cast::instance()->sessionState(QStringLiteral("ended"));
                    else if (why == QLatin1String("ERROR"))
                        Cast::instance()->sessionState(QStringLiteral("error"), QStringLiteral("The Chromecast could not play this video."));
                }
                Cast::instance()->sessionPosition(m_position, m_duration);
            }
        }
    }

    QSslSocket m_socket;
    QByteArray m_buffer;
    QTimer m_heartbeat;
    QTimer m_tick;
    QJsonObject m_load;
    QString m_transport;
    int m_mediaSession = 0;
    int m_request = 0;
    double m_position = 0;
    double m_duration = 0;
    bool m_playing = false;
    bool m_stopping = false;
};

} // namespace

Cast::Cast(QObject *parent) : QObject(parent), m_network(new QNetworkAccessManager(this)), m_proxy(new CastProxy(this)) {
    m_searchEnds.setSingleShot(true);
    m_searchEnds.setInterval(kSearchMs);
    connect(&m_searchEnds, &QTimer::timeout, this, [this] {
        platform::holdMulticast(false);
        if (m_ssdp)
            m_ssdp->deleteLater();
        if (m_mdns)
            m_mdns->deleteLater();
        m_ssdp = nullptr;
        m_mdns = nullptr;
        emit devicesChanged();
    });
}

Cast::~Cast() = default;

Cast *Cast::instance() {
    static auto *cast = new Cast(App::instance());
    return cast;
}

QVariantList Cast::devices() const {
    QVariantList out;
    for (const auto &device : m_devices)
        out.append(QVariantMap{{"id", device.id}, {"name", device.name}, {"kind", device.kind}});
    return out;
}

bool Cast::active() const { return m_session != nullptr; }

QString Cast::deviceName() const { return m_session ? m_session->device().name : QString(); }

void Cast::search() {
    if (m_searchEnds.isActive())
        return;
    platform::holdMulticast(true);
    // DLNA renderers answer an SSDP search to the address it came from.
    m_ssdp = new QUdpSocket(this);
    if (m_ssdp->bind(QHostAddress::AnyIPv4, 0)) {
        connect(m_ssdp, &QUdpSocket::readyRead, this, &Cast::readSsdp);
        const QByteArray search = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\n"
                                  "ST: urn:schemas-upnp-org:service:AVTransport:1\r\n\r\n";
        for (int i = 0; i < 2; ++i)
            m_ssdp->writeDatagram(search, kSsdpGroup, 1900);
    }
    // Chromecasts over mDNS: on the mDNS port where it is free to share (so
    // multicast answers arrive too), asking for unicast answers as well.
    m_mdns = new QUdpSocket(this);
    bool shared = m_mdns->bind(QHostAddress::AnyIPv4, 5353, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (shared) {
        for (const auto &interface : QNetworkInterface::allInterfaces())
            if ((interface.flags() & QNetworkInterface::IsUp) && (interface.flags() & QNetworkInterface::CanMulticast) &&
                !(interface.flags() & QNetworkInterface::IsLoopBack))
                m_mdns->joinMulticastGroup(kMdnsGroup, interface);
    } else {
        m_mdns->bind(QHostAddress::AnyIPv4, 0);
    }
    connect(m_mdns, &QUdpSocket::readyRead, this, &Cast::readMdns);
    for (int i = 0; i < 2; ++i)
        m_mdns->writeDatagram(mdnsQuery(), kMdnsGroup, 5353);
    m_searchEnds.start();
    emit devicesChanged();
}

void Cast::readSsdp() {
    auto *socket = qobject_cast<QUdpSocket *>(sender());
    while (socket && socket->hasPendingDatagrams()) {
        const auto datagram = socket->receiveDatagram();
        for (const auto &line : datagram.data().split('\n'))
            if (line.toLower().startsWith("location:"))
                describe(QUrl(QString::fromUtf8(line.mid(9).trimmed())));
    }
}

void Cast::describe(const QUrl &location) {
    if (!location.isValid() || m_described.contains(location.toString()))
        return;
    m_described.insert(location.toString());
    QNetworkRequest request(location);
    request.setTransferTimeout(5000);
    auto *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, location] {
        reply->deleteLater();
        m_described.remove(location.toString());
        if (reply->error() != QNetworkReply::NoError)
            return;
        QXmlStreamReader xml(reply->readAll());
        QString name, serviceType, control, base, udn;
        QUrl transport;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement())
                continue;
            const auto tag = xml.name();
            if (tag == QLatin1String("friendlyName") && name.isEmpty())
                name = xml.readElementText();
            else if (tag == QLatin1String("UDN") && udn.isEmpty())
                udn = xml.readElementText();
            else if (tag == QLatin1String("URLBase"))
                base = xml.readElementText();
            else if (tag == QLatin1String("serviceType"))
                serviceType = xml.readElementText();
            else if (tag == QLatin1String("controlURL")) {
                control = xml.readElementText();
                if (serviceType.contains(QLatin1String("AVTransport")) && transport.isEmpty())
                    transport = QUrl(base.isEmpty() ? location : QUrl(base)).resolved(QUrl(control));
            }
        }
        if (transport.isEmpty())
            return;
        Device device;
        device.id = QStringLiteral("dlna:") + (udn.isEmpty() ? location.toString() : udn);
        device.name = name.isEmpty() ? location.host() : name;
        device.kind = QStringLiteral("dlna");
        device.host = QHostAddress(location.host());
        device.description = location;
        device.control = transport;
        found(device);
    });
}

void Cast::readMdns() {
    auto *socket = qobject_cast<QUdpSocket *>(sender());
    while (socket && socket->hasPendingDatagrams()) {
        const auto datagram = socket->receiveDatagram();
        const auto packet = datagram.data();
        if (packet.size() < 12 || !(quint8(packet.at(2)) & 0x80))
            continue; // not an answer
        const int questions = qFromBigEndian<quint16>(packet.constData() + 4);
        const int records = qFromBigEndian<quint16>(packet.constData() + 6) + qFromBigEndian<quint16>(packet.constData() + 8) +
                            qFromBigEndian<quint16>(packet.constData() + 10);
        int offset = 12;
        for (int q = 0; q < questions && offset < packet.size(); ++q) {
            dnsName(packet, offset);
            offset += 4;
        }
        QHash<QString, QString> ptr;               // service -> instance
        QHash<QString, QPair<QString, int>> srv;   // instance -> target, port
        QHash<QString, QHash<QString, QString>> txt;
        QHash<QString, QHostAddress> addresses;
        for (int r = 0; r < records && offset + 10 <= packet.size(); ++r) {
            const auto name = dnsName(packet, offset);
            if (offset + 10 > packet.size())
                break;
            const auto type = qFromBigEndian<quint16>(packet.constData() + offset);
            const auto length = qFromBigEndian<quint16>(packet.constData() + offset + 8);
            int data = offset + 10;
            offset = data + length;
            if (offset > packet.size())
                break;
            if (type == 12) { // PTR
                int at = data;
                ptr.insert(name, dnsName(packet, at));
            } else if (type == 33 && length > 6) { // SRV
                int at = data + 6;
                srv.insert(name, {dnsName(packet, at), qFromBigEndian<quint16>(packet.constData() + data + 4)});
            } else if (type == 16) { // TXT
                QHash<QString, QString> values;
                for (int at = data; at < data + length;) {
                    const int size = quint8(packet.at(at));
                    const auto entry = QString::fromUtf8(packet.mid(at + 1, size));
                    values.insert(entry.section(QLatin1Char('='), 0, 0), entry.section(QLatin1Char('='), 1));
                    at += 1 + size;
                }
                txt.insert(name, values);
            } else if (type == 1 && length == 4) { // A
                addresses.insert(name, QHostAddress(qFromBigEndian<quint32>(packet.constData() + data)));
            }
        }
        for (auto it = srv.begin(); it != srv.end(); ++it) {
            if (!it.key().contains(QLatin1String("_googlecast")))
                continue;
            const auto values = txt.value(it.key());
            Device device;
            device.id = QStringLiteral("cast:") + values.value(QStringLiteral("id"), it.key());
            device.name = values.value(QStringLiteral("fn"), it.key().section(QLatin1Char('.'), 0, 0));
            device.kind = QStringLiteral("chromecast");
            device.host = addresses.value(it.value().first, datagram.senderAddress());
            device.port = it.value().second;
            found(device);
        }
    }
}

void Cast::found(const Device &device) {
    for (auto &known : m_devices)
        if (known.id == device.id) {
            known = device;
            emit devicesChanged();
            return;
        }
    m_devices.append(device);
    emit devicesChanged();
}

void Cast::addDevice(const QString &kind, const QString &name, const QString &address, int port) {
    if (kind == QLatin1String("dlna"))
        return describe(QUrl(address));
    found({QStringLiteral("cast:") + address, name, QStringLiteral("chromecast"), QHostAddress(address), port, {}, {}});
}

void Cast::play(const QString &deviceId, const QVariantMap &video, const QString &title, const QString &cover, double startAt) {
    const auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const Device &d) { return d.id == deviceId; });
    if (it == m_devices.end())
        return;
    if (m_session)
        m_session->stop();
    m_proxy->clear();
    Headers headers;
    const auto wanted = video.value(QStringLiteral("headers")).toMap();
    for (auto h = wanted.begin(); h != wanted.end(); ++h)
        headers.append({h.key().toUtf8(), h.value().toString().toUtf8()});
    const QUrl source(video.value(QStringLiteral("url")).toString());
    // A stream goes through the relay first (it undoes disguised HLS, as for
    // SCANCORE's own player); the cast proxy shares the relay's address on
    // the network, so the relay itself never listens beyond this device.
    QUrl from = source;
    Headers forProxy = headers;
    if (video::StreamRelay::wanted(source) && qgetenv("SCANCORE_STREAM_RELAY") != "0") {
        const auto relayed = video::StreamRelay::wrap(source, headers);
        if (relayed.host() == QLatin1String("127.0.0.1")) {
            from = relayed;
            forProxy.clear();
        }
    }
    const auto url = m_proxy->share(from, forProxy, it->host);
    m_error.clear();
    if (url.isEmpty()) {
        m_state = QStringLiteral("error");
        m_error = QStringLiteral("SCANCORE could not share the video on this network.");
        emit stateChanged();
        return;
    }
    const bool hls = source.path().endsWith(QLatin1String(".m3u8"), Qt::CaseInsensitive) ||
                     video.value(QStringLiteral("mpvArgs")).toMap().contains(QStringLiteral("demuxer"));
    QVariantList subtitles;
    for (const auto &value : video.value(QStringLiteral("subtitles")).toList()) {
        const auto track = value.toMap();
        const auto shared = m_proxy->share(QUrl(track.value(QStringLiteral("url")).toString()), headers, it->host);
        if (!shared.isEmpty())
            subtitles.append(QVariantMap{{"url", shared}, {"lang", track.value(QStringLiteral("lang"))}});
    }
    if (it->kind == QLatin1String("dlna"))
        m_session = std::make_unique<DlnaSession>(*it, m_network);
    else
        m_session = std::make_unique<ChromecastSession>(*it);
    m_position = startAt;
    m_duration = 0;
    m_state = QStringLiteral("connecting");
    emit stateChanged();
    emit positionChanged();
    m_session->load(url, hls ? QStringLiteral("application/x-mpegURL") : QStringLiteral("video/mp4"), title, cover, startAt, subtitles);
}

void Cast::pause() {
    if (m_session)
        m_session->pause();
}

void Cast::resume() {
    if (m_session)
        m_session->resume();
}

void Cast::seek(double seconds) {
    if (!m_session)
        return;
    m_session->seek(seconds);
    m_position = seconds;
    emit positionChanged();
}

void Cast::stop() {
    if (!m_session)
        return;
    m_session->stop();
    // The session finishes its last messages, then goes.
    auto *session = m_session.release();
    QTimer::singleShot(2000, session, [session] { delete session; });
    m_proxy->clear();
    m_state.clear();
    emit stateChanged();
}

void Cast::sessionState(const QString &state, const QString &error) {
    if (!m_session || (state == m_state && error == m_error))
        return;
    m_state = state;
    m_error = error;
    emit stateChanged();
}

void Cast::sessionPosition(double position, double duration) {
    if (!m_session)
        return;
    m_position = position;
    if (duration > 0)
        m_duration = duration;
    emit positionChanged();
}

} // namespace sc
