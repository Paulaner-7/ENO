#include "Announcements.h"

#include "Account.h"
#include "App.h"
#include "Notifications.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/Task.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QUrlQuery>

namespace sc {

namespace {

constexpr int kMaxBackoffMs = 5 * 60 * 1000;
const auto kCursorPref = QStringLiteral("announcements.cursor");

bool enabled() { return Database::pref(QStringLiteral("setting.announcements"), true).toBool(); }

} // namespace

Announcements::Announcements(QObject *parent) : QObject(parent), m_network(new QNetworkAccessManager(this)) {
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &Announcements::connectStream);
    // Back online: reconnect now rather than at the next try.
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance())
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this,
                [this](QNetworkInformation::Reachability reachability) {
                    if (m_running && !m_reply && reachability == QNetworkInformation::Reachability::Online) {
                        m_backoffMs = 15000;
                        m_retry.start(1000);
                    }
                });
}

Announcements *Announcements::instance() {
    static auto *announcements = new Announcements(App::instance());
    return announcements;
}

void Announcements::start() {
    if (m_running || !enabled())
        return;
    m_running = true;
    connectStream();
}

void Announcements::stop() {
    m_running = false;
    m_retry.stop();
    if (m_reply)
        m_reply->abort();
}

void Announcements::connectStream() {
    if (!m_running || m_reply)
        return;
    if (!enabled()) {
        m_running = false;
        return;
    }
    QUrl url(Account::siteBase() + QStringLiteral("/api/announcements/stream"));
    auto cursor = Database::pref(kCursorPref).toString();
    // The first time, the last two weeks' announcements.
    if (cursor.isEmpty())
        cursor = QDateTime::currentDateTimeUtc().addDays(-14).toString(Qt::ISODateWithMs);
    url.setQuery(QUrlQuery{{QStringLiteral("after"), cursor}});
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Http::userAgent());
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Last-Event-ID", cursor.toUtf8());
    // The stream stays open for many minutes; nothing may time it out.
    request.setTransferTimeout(0);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_buffer.clear();
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &Announcements::readEvents);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply] {
        const bool gotData = reply && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        if (reply)
            reply->deleteLater();
        m_reply = nullptr;
        // A stream that ran (the server ends them after a while) comes
        // back soon; failures wait longer each time.
        if (gotData)
            m_backoffMs = 15000;
        retryLater();
    });
}

void Announcements::retryLater() {
    if (!m_running)
        return;
    m_retry.start(m_backoffMs);
    m_backoffMs = std::min(kMaxBackoffMs, m_backoffMs * 2);
}

// Server-sent events: blocks separated by a blank line, "field: value" lines.
void Announcements::readEvents() {
    if (!m_reply)
        return;
    m_buffer += m_reply->readAll();
    // A runaway stream (not an event stream) is dropped.
    if (m_buffer.size() > 1024 * 1024) {
        m_reply->abort();
        return;
    }
    m_buffer.replace("\r\n", "\n");
    qsizetype end;
    while ((end = m_buffer.indexOf("\n\n")) >= 0) {
        const auto block = m_buffer.left(end);
        m_buffer.remove(0, end + 2);
        QByteArray event = "message", data, id;
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
            else if (field == "id")
                id = value;
        }
        handle(event, data, id);
    }
}

void Announcements::handle(const QByteArray &event, const QByteArray &data, const QByteArray &id) {
    if (event != "announcement" || data.isEmpty())
        return;
    const auto a = QJsonDocument::fromJson(data).object();
    const auto key = QStringLiteral("announcement:") + a.value("id").toString();
    if (a.value("id").toString().isEmpty())
        return;
    if (!a.value("removedAt").toString().isEmpty()) {
        Notifications::withdraw(key);
    } else {
        const auto link = a.value("link").toString();
        const bool important = a.value("level").toString() == QLatin1String("important");
        Notifications::post({QStringLiteral("announcement"), key, a.value("title").toString(), a.value("body").toString(),
                             link.startsWith(QLatin1String("https://")) ? QVariantMap{{"url", link}} : QVariantMap{}, true, important});
    }
    if (!id.isEmpty())
        runInBackground([cursor = QString::fromUtf8(id)] { Database::setPref(kCursorPref, cursor); });
}

} // namespace sc
