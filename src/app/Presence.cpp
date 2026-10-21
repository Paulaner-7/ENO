#include "Presence.h"

#include "App.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtEndian>

#ifndef SCANCORE_DISCORD_APP_ID
#define SCANCORE_DISCORD_APP_ID ""
#endif

namespace sc {

namespace {

const QString kAppId = QStringLiteral(SCANCORE_DISCORD_APP_ID);

// Where the Discord app listens: a named pipe on Windows, a socket in the
// runtime or temporary folder elsewhere (also inside Flatpak and Snap).
QStringList discordSockets() {
    QStringList out;
#ifdef Q_OS_WIN
    for (int i = 0; i < 10; ++i)
        out.append(QStringLiteral("discord-ipc-%1").arg(i));
#else
    QStringList bases;
    for (const auto *name : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"})
        if (const auto value = qEnvironmentVariable(name); !value.isEmpty())
            bases.append(value);
    bases.append(QStringLiteral("/tmp"));
    for (const auto &base : std::as_const(bases))
        for (const auto &sub : {QString(), QStringLiteral("/app/com.discordapp.Discord"), QStringLiteral("/snap.discord"),
                                QStringLiteral("/.flatpak/dev.vencord.Vesktop/xdg-run")})
            for (int i = 0; i < 10; ++i) {
                const auto path = base + sub + QStringLiteral("/discord-ipc-%1").arg(i);
                if (QFileInfo::exists(path) && !out.contains(path))
                    out.append(path);
            }
#endif
    return out;
}

} // namespace

Presence::Presence(QObject *parent) : QObject(parent) {
    m_retry.setSingleShot(true);
    m_retry.setInterval(30000);
    connect(&m_retry, &QTimer::timeout, this, &Presence::connectToDiscord);
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        send(0, QJsonObject{{"v", 1}, {"client_id", kAppId}});
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] {
        m_buffer += m_socket.readAll();
        while (m_buffer.size() >= 8) {
            const auto length = qFromLittleEndian<qint32>(m_buffer.constData() + 4);
            if (m_buffer.size() < 8 + length)
                break;
            const auto message = QJsonDocument::fromJson(m_buffer.mid(8, length)).object();
            m_buffer.remove(0, 8 + length);
            if (message.value("evt").toString() == QLatin1String("READY")) {
                m_ready = true;
                emit changed();
                publish();
            }
        }
    });
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        m_ready = false;
        m_buffer.clear();
        emit changed();
        if (enabled())
            m_retry.start();
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this] {
        // The next place Discord might listen, then a pause before trying again.
        if (!m_candidates.isEmpty()) {
            m_socket.connectToServer(m_candidates.takeFirst());
            return;
        }
        if (enabled())
            m_retry.start();
    });
    connect(App::instance(), &App::settingsChanged, this, [this] {
        if (enabled() && m_socket.state() == QLocalSocket::UnconnectedState)
            connectToDiscord();
        else if (!enabled() && m_socket.state() != QLocalSocket::UnconnectedState) {
            clear();
            m_socket.disconnectFromServer();
        } else
            publish();
    });
    if (enabled())
        QTimer::singleShot(3000, this, &Presence::connectToDiscord);
}

Presence *Presence::instance() {
    static auto *presence = [] {
        auto *created = new Presence;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return presence;
}

bool Presence::available() const {
#ifdef Q_OS_ANDROID
    return false; // Discord on phones has no local socket to talk to
#else
    return !kAppId.isEmpty();
#endif
}

bool Presence::enabled() const {
    return available() && App::instance()->settings().value(QStringLiteral("discordPresence")).toBool();
}

void Presence::connectToDiscord() {
    if (!enabled() || m_socket.state() != QLocalSocket::UnconnectedState)
        return;
    m_candidates = discordSockets();
    if (m_candidates.isEmpty()) {
        m_retry.start();
        return;
    }
    m_socket.connectToServer(m_candidates.takeFirst());
}

void Presence::send(int opcode, const QJsonObject &payload) {
    const auto body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray frame(8, Qt::Uninitialized);
    qToLittleEndian<qint32>(opcode, frame.data());
    qToLittleEndian<qint32>(qint32(body.size()), frame.data() + 4);
    m_socket.write(frame + body);
}

void Presence::show(const QString &media, const QString &title, const QString &unit) {
    // Incognito: nothing shows on Discord either.
    if (App::instance()->incognito()) {
        clear();
        return;
    }
    if (m_current.value("title").toString() != title || m_current.value("media").toString() != media)
        m_startedAt = QDateTime::currentSecsSinceEpoch();
    m_current = {{"media", media}, {"title", title}, {"unit", unit}};
    publish();
}

void Presence::clear() {
    m_current.clear();
    publish();
}

void Presence::publish() {
    if (!m_ready)
        return;
    QJsonValue activity = QJsonValue::Null;
    if (!m_current.isEmpty()) {
        const auto media = m_current.value("media").toString();
        const bool watching = media == QLatin1String("anime");
        const bool hide = App::instance()->settings().value(QStringLiteral("discordHideTitles")).toBool();
        const auto kind = watching ? QStringLiteral("anime") : media == QLatin1String("novel") ? QStringLiteral("a novel")
                                                                                               : QStringLiteral("a comic");
        QJsonObject object{
            {"type", watching ? 3 : 0},
            {"details", hide ? (watching ? QStringLiteral("Watching ") : QStringLiteral("Reading ")) + kind
                             : m_current.value("title").toString().left(128)},
            {"timestamps", QJsonObject{{"start", m_startedAt}}},
            {"assets", QJsonObject{{"large_image", "logo"}, {"large_text", "SCANCORE"}}},
            {"buttons", QJsonArray{QJsonObject{{"label", "Get SCANCORE"}, {"url", "https://scancore.onl"}}}},
        };
        if (!hide && !m_current.value("unit").toString().isEmpty())
            object.insert(QStringLiteral("state"), m_current.value("unit").toString().left(128));
        activity = object;
    }
    send(1, QJsonObject{{"cmd", "SET_ACTIVITY"},
                        {"args", QJsonObject{{"pid", qint64(QCoreApplication::applicationPid())}, {"activity", activity}}},
                        {"nonce", QUuid::createUuid().toString(QUuid::WithoutBraces)}});
}

} // namespace sc
