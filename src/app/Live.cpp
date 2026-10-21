#include "Live.h"

#include "Account.h"
#include "Activity.h"
#include "App.h"
#include "Library.h"
#include "Sync.h"
#include "core/Cookies.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/Task.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QUrlQuery>

#include <utility>

namespace sc {

namespace {

constexpr int kMaxBackoffMs = 5 * 60 * 1000;
// Positions go out at most this often while reading or watching.
constexpr qint64 kSendGapMs = 5000;
// An app that left is kept this long: its stream may just be reconnecting.
constexpr qint64 kLeaveGraceMs = 20000;

bool g_applying = false;

QVariantMap self() {
    return {{"id", Activity::deviceId()}, {"name", Activity::deviceName()}, {"platform", Sync::platform()}};
}

bool foreground() {
#ifdef Q_OS_ANDROID
    return qGuiApp && qGuiApp->applicationState() == Qt::ApplicationActive;
#else
    return true;
#endif
}

} // namespace

Live::Live(QObject *parent) : QObject(parent), m_network(new QNetworkAccessManager(this)) {
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &Live::connectStream);
    m_send.setSingleShot(true);
    connect(&m_send, &QTimer::timeout, this, &Live::sendPending);
    m_sweep.setInterval(5000);
    connect(&m_sweep, &QTimer::timeout, this, [this] {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        bool gone = false;
        for (auto it = m_leaving.begin(); it != m_leaving.end();) {
            if (now - it.value() >= kLeaveGraceMs) {
                gone |= m_devices.remove(it.key()) > 0;
                it = m_leaving.erase(it);
            } else {
                ++it;
            }
        }
        if (m_leaving.isEmpty())
            m_sweep.stop();
        if (gone)
            emit changed();
    });
    connect(Account::instance(), &Account::changed, this, [this] {
        // Another account or site may support it.
        if (!Account::instance()->signedIn())
            m_unsupported = false;
        update();
    });
    connect(Sync::instance(), &Sync::changed, this, &Live::update);
    connect(Library::instance(), &Library::progressRecorded, this, &Live::queueProgress);
#ifdef Q_OS_ANDROID
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this] { update(); });
#endif
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance())
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this,
                [this](QNetworkInformation::Reachability reachability) {
                    if (!m_reply && wanted() && reachability == QNetworkInformation::Reachability::Online) {
                        m_backoffMs = 5000;
                        m_retry.start(1000);
                    }
                });
}

Live *Live::instance() {
    static auto *live = new Live(App::instance());
    return live;
}

bool Live::applying() { return g_applying; }

QVariantList Live::devices() const {
    QVariantList out;
    for (const auto &device : m_devices)
        out.append(device);
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("name").toString().localeAwareCompare(b.toMap().value("name").toString()) < 0;
    });
    return out;
}

bool Live::wanted() const {
    const auto *account = Account::instance();
    return account->signedIn() && account->active() && !m_unsupported && foreground();
}

void Live::update() {
    if (wanted()) {
        if (!m_reply && !m_retry.isActive())
            connectStream();
    } else {
        disconnectStream();
    }
}

void Live::disconnectStream() {
    m_retry.stop();
    m_send.stop();
    if (m_reply)
        m_reply->abort();
    if (m_connected || !m_devices.isEmpty()) {
        m_connected = false;
        m_devices.clear();
        m_leaving.clear();
        m_sweep.stop();
        emit changed();
    }
}

void Live::connectStream() {
    if (m_reply || !wanted())
        return;
    const auto me = self();
    QUrl url(Account::siteBase() + QStringLiteral("/api/app/live"));
    url.setQuery(QUrlQuery{{QStringLiteral("device"), me.value("id").toString()},
                           {QStringLiteral("name"), me.value("name").toString()},
                           {QStringLiteral("platform"), me.value("platform").toString()}});
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Http::userAgent());
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");
    // The session cookie, from the jar the rest of the app shares (this
    // manager has none of its own).
    QByteArray cookies;
    for (const auto &cookie : CookieJar::shared().cookiesForUrl(url))
        cookies += (cookies.isEmpty() ? QByteArray() : QByteArray("; ")) + cookie.toRawForm(QNetworkCookie::NameAndValueOnly);
    if (!cookies.isEmpty())
        request.setRawHeader("Cookie", cookies);
    request.setTransferTimeout(0);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_buffer.clear();
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::metaDataChanged, this, [this, reply = m_reply] {
        if (!reply || reply != m_reply)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200) {
            m_connected = true;
            m_backoffMs = 5000;
            // Whoever is still open answers this stream's arrival; the
            // others drop out after the grace period.
            const auto now = QDateTime::currentMSecsSinceEpoch();
            for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it)
                m_leaving.insert(it.key(), now);
            if (!m_leaving.isEmpty())
                m_sweep.start();
            emit changed();
        } else if (status == 404) {
            // A site without live sync (an older scancore-web).
            m_unsupported = true;
        }
    });
    connect(m_reply, &QNetworkReply::readyRead, this, &Live::readEvents);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply] {
        const int status = reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
        if (reply)
            reply->deleteLater();
        if (reply != m_reply)
            return;
        m_reply = nullptr;
        const bool wasConnected = m_connected;
        m_connected = false;
        emit changed();
        // Signed out on the site: the next sync notices and signs out here.
        if (status == 401 || status == 404)
            return;
        // A stream the server ended after its lifetime comes back at once.
        if (wasConnected)
            m_backoffMs = 1000;
        retryLater();
    });
}

void Live::retryLater() {
    if (!wanted())
        return;
    m_retry.start(m_backoffMs);
    m_backoffMs = std::min(kMaxBackoffMs, std::max(5000, m_backoffMs * 2));
}

// Server-sent events: blocks separated by a blank line, "field: value" lines.
void Live::readEvents() {
    if (!m_reply)
        return;
    m_buffer += m_reply->readAll();
    if (m_buffer.size() > 1024 * 1024) {
        m_reply->abort();
        return;
    }
    m_buffer.replace("\r\n", "\n");
    qsizetype end;
    while ((end = m_buffer.indexOf("\n\n")) >= 0) {
        const auto block = m_buffer.left(end);
        m_buffer.remove(0, end + 2);
        QByteArray event = "message", data;
        for (const auto &line : block.split('\n')) {
            if (line.isEmpty() || line.startsWith(':'))
                continue;
            const auto colon = line.indexOf(':');
            const auto field = colon < 0 ? line : line.left(colon);
            auto value = colon < 0 ? QByteArray() : line.mid(colon + 1);
            if (value.startsWith(' '))
                value.remove(0, 1);
            if (field == "event")
                event = value;
            else if (field == "data")
                data += (data.isEmpty() ? QByteArray() : QByteArray("\n")) + value;
        }
        if (!data.isEmpty())
            handle(event, data);
    }
}

void Live::handle(const QByteArray &event, const QByteArray &data) {
    const auto message = QJsonDocument::fromJson(data).object();
    if (event == "presence") {
        setOnline(message.value("device").toObject().toVariantMap(), message.value("online").toBool());
        return;
    }
    if (event != "progress" && event != "handoff")
        return;
    const auto from = message.value("from").toObject().toVariantMap();
    const auto payload = message.value("data").toObject();
    const bool handoff = event == "handoff";
    // A message from an app means it is open, even if its presence was missed.
    setOnline(from, true);
    runInBackground([this, payload, from, handoff] {
        bool merged = false;
        try {
            merged = Sync::mergeUnit(payload);
        } catch (const Error &e) {
            qWarning().noquote() << "Live progress not merged:" << e.message();
            return;
        }
        QMetaObject::invokeMethod(this, [this, payload, from, handoff, merged] {
            if (merged) {
                g_applying = true;
                Library::instance()->notifyChanged();
                g_applying = false;
            }
            if (!handoff)
                return;
            auto entry = payload.value("series").toObject().toVariantMap();
            entry.insert(QStringLiteral("id"), payload.value("seriesId").toString());
            entry.insert(QStringLiteral("unitTitle"), payload.value("unit").toArray().at(0).toString());
            emit handoffReceived(entry, from);
        });
    });
}

void Live::setOnline(const QVariantMap &device, bool online) {
    const auto id = device.value("id").toString();
    if (id.isEmpty() || id == Activity::deviceId())
        return;
    if (online) {
        m_leaving.remove(id);
        if (m_devices.value(id) == device)
            return;
        m_devices.insert(id, device);
        emit changed();
    } else if (m_devices.contains(id)) {
        m_leaving.insert(id, QDateTime::currentMSecsSinceEpoch());
        m_sweep.start();
    }
}

void Live::queueProgress(const QString &seriesId, const QString &unitKey) {
    // Only worth sending while another app is there to take it.
    if (!m_connected || m_devices.isEmpty())
        return;
    m_pendingSeries = seriesId;
    m_pendingUnit = unitKey;
    if (m_send.isActive())
        return;
    const qint64 since = m_sinceSent.isValid() ? m_sinceSent.elapsed() : kSendGapMs;
    m_send.start(int(std::max<qint64>(1000, kSendGapMs - since)));
}

void Live::sendPending() {
    if (m_pendingSeries.isEmpty())
        return;
    const auto series = std::exchange(m_pendingSeries, QString());
    const auto unit = std::exchange(m_pendingUnit, QString());
    m_sinceSent.start();
    runInBackground([this, series, unit] {
        const auto payload = Sync::unitPayload(series, unit);
        if (!payload.isEmpty())
            QMetaObject::invokeMethod(this, [this, payload] { post("progress", payload); });
    });
}

bool Live::handoff(const QString &deviceId, const QString &seriesId) {
    if (!m_connected || !m_devices.contains(deviceId))
        return false;
    const auto unit = Database::scalar(QStringLiteral("SELECT unit_key FROM progress WHERE series_id = ? ORDER BY updated_at DESC LIMIT 1"),
                                       {seriesId})
                          .toString();
    const auto payload = unit.isEmpty() ? QJsonObject() : Sync::unitPayload(seriesId, unit);
    if (payload.isEmpty())
        return false;
    post("handoff", payload, deviceId);
    return true;
}

void Live::post(const QByteArray &type, const QJsonObject &data, const QString &to) {
    QJsonObject body{{"from", QJsonObject::fromVariantMap(self())}, {"type", QString::fromLatin1(type)}, {"data", data}};
    if (!to.isEmpty())
        body.insert(QStringLiteral("to"), to);
    const QUrl url(Account::siteBase() + QStringLiteral("/api/app/live"));
    const auto json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    runInBackground([url, json] {
        try {
            const auto response = Http::postJson(url, json, {{"Accept", "application/json"}});
            if (!response.ok())
                qWarning().noquote() << "Live message not sent:" << response.status;
        } catch (const Error &e) {
            qWarning().noquote() << "Live message not sent:" << e.message();
        }
    });
}

} // namespace sc
