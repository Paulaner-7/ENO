#include "Party.h"

#include "Account.h"
#include "App.h"
#include "EventStream.h"
#include "core/Cookies.h"
#include "core/Http.h"
#include "core/Task.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QRegularExpression>

#include <cmath>

namespace sc {

namespace {

// The host's state goes out at least this often while nothing changes, so
// players that drift are pulled back.
constexpr qint64 kRefreshMs = 8000;
// And at most this often while it does (a seek bar being dragged).
constexpr qint64 kMinGapMs = 300;
// Further than this from where the host's last state puts them, a seek.
constexpr double kSeekJump = 2.5;
constexpr int kMaxChat = 200;

QString normalCode(const QString &code) {
    auto out = code.trimmed().toUpper();
    out.remove(QRegularExpression(QStringLiteral("[\\s-]")));
    return out;
}

// The site's own message for a refusal ({error}), or a plain one.
QString refusal(const Response &response) {
    const auto message = QJsonDocument::fromJson(response.body).object().value("error").toString();
    if (!message.isEmpty())
        return message;
    return Party::tr("The party could not be reached (%1).").arg(response.status);
}

} // namespace

Party::Party(QObject *parent) : QObject(parent), m_network(new QNetworkAccessManager(this)) {
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &Party::connectStream);
    m_send.setSingleShot(true);
    connect(&m_send, &QTimer::timeout, this, &Party::sendState);
    connect(Account::instance(), &Account::changed, this, [this] {
        if (!Account::instance()->signedIn()) {
            m_unsupported = false;
            if (active())
                reset();
        }
        emit changed();
    });
}

Party *Party::instance() {
    static auto *party = new Party(App::instance());
    return party;
}

bool Party::available() const { return Account::instance()->signedIn() && !m_unsupported; }

void Party::setWatching(int anilistId) {
    if (m_watching == anilistId)
        return;
    m_watching = anilistId;
    emit changed();
}

void Party::setError(const QString &error) {
    m_error = error;
    emit changed();
}

void Party::start(const QVariantMap &anime) {
    if (m_busy || !Account::instance()->signedIn())
        return;
    if (active())
        leave();
    m_busy = true;
    m_error.clear();
    emit changed();
    const QUrl url(Account::siteBase() + QStringLiteral("/api/app/party"));
    runInBackground([this, url, anime] {
        QString code, error;
        bool unsupported = false;
        try {
            const auto response = Http::postJson(url, "{}", {{"Accept", "application/json"}});
            if (response.ok())
                code = QJsonDocument::fromJson(response.body).object().value("code").toString();
            else if (response.status == 404)
                unsupported = true;
            else
                error = refusal(response);
        } catch (const Error &e) {
            error = e.message();
        }
        QMetaObject::invokeMethod(this, [this, code, error, unsupported, anime] {
            m_busy = false;
            if (unsupported) {
                m_unsupported = true;
                setError(tr("Watch parties need scancore.onl (this site has none yet)."));
                return;
            }
            if (code.isEmpty()) {
                setError(error.isEmpty() ? tr("The party could not be opened.") : error);
                return;
            }
            join(code);
            // Everyone who joins sees the anime at once, before the player
            // has said anything.
            m_pending = {{"anime", anime}, {"episode", anime.value("episode", 1)}, {"playing", false}, {"position", 0.0}};
        });
    });
}

void Party::join(const QString &rawCode) {
    const auto code = normalCode(rawCode);
    if (!QRegularExpression(QStringLiteral("^[A-HJ-NP-Z2-9]{6}$")).match(code).hasMatch()) {
        setError(tr("A party code has six letters and numbers."));
        return;
    }
    if (active() && code != m_code)
        leave();
    m_code = code;
    m_error.clear();
    m_failures = 0;
    emit changed();
    connectStream();
}

void Party::connectStream() {
    if (m_reply || m_code.isEmpty())
        return;
    const QUrl url(Account::siteBase() + QStringLiteral("/api/app/party/") + m_code);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Http::userAgent());
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");
    // The session cookie, as Live sends it.
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
        if (reply && reply == m_reply && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            m_connected = true;
            m_failures = 0;
            emit changed();
        }
    });
    connect(m_reply, &QNetworkReply::readyRead, this, &Party::readEvents);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply] {
        if (!reply)
            return;
        reply->deleteLater();
        if (reply != m_reply)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto body = status == 200 ? QByteArray() : reply->readAll();
        m_reply = nullptr;
        m_connected = false;
        if (m_code.isEmpty())
            return emit changed();
        if (status == 404 || status == 401 || status == 403) {
            const bool wasIn = !m_you.isEmpty();
            const auto message = QJsonDocument::fromJson(body).object().value("error").toString();
            reset();
            setError(wasIn ? tr("The party is over.") : !message.isEmpty() ? message : tr("No party with that code."));
            if (wasIn)
                emit ended();
            return;
        }
        // The stream ends after a while, or the network dropped: back at once,
        // then more slowly.
        emit changed();
        m_retry.start(std::min(30000, 1000 * (1 << std::min(m_failures++, 5))));
    });
}

void Party::readEvents() {
    if (!m_reply)
        return;
    m_buffer += m_reply->readAll();
    if (m_buffer.size() > 1024 * 1024) {
        m_reply->abort();
        return;
    }
    takeEvents(m_buffer, [this](const QByteArray &event, const QByteArray &data) { handle(event, data); });
}

void Party::handle(const QByteArray &event, const QByteArray &data) {
    const auto message = QJsonDocument::fromJson(data).object();
    if (event == "hello") {
        m_you = message.value("you").toString();
        m_host = message.value("host").toString();
        emit changed();
        // The host's first state (the anime) waits for the stream.
        if (hosting() && !m_pending.isEmpty())
            sendState();
    } else if (event == "state") {
        m_state = message.value("state").toObject().toVariantMap();
        // The time since the host sent it, on this device's clock.
        m_stateClock.start();
        const double age = m_state.value("age").toDouble() / 1000.0;
        if (m_state.value("playing").toBool())
            m_state.insert(QStringLiteral("position"), m_state.value("position").toDouble() + std::clamp(age, 0.0, 30.0));
        emit stateReceived();
    } else if (event == "members") {
        m_host = message.value("host").toString(m_host);
        QVariantList members;
        for (const auto &value : message.value("members").toArray()) {
            auto member = value.toObject().toVariantMap();
            member.insert(QStringLiteral("host"), member.value("userId").toString() == m_host);
            members.append(member);
        }
        // The host first.
        std::stable_sort(members.begin(), members.end(),
                         [](const QVariant &a, const QVariant &b) { return a.toMap().value("host").toBool() && !b.toMap().value("host").toBool(); });
        m_members = members;
        emit changed();
    } else if (event == "chat") {
        const auto from = message.value("from").toObject();
        const bool mine = from.value("userId").toString() == m_you;
        m_chat.append(QVariantMap{{"name", from.value("name").toString()},
                                  {"avatar", from.value("avatar").toString()},
                                  {"text", message.value("text").toString()},
                                  {"at", message.value("at").toDouble()},
                                  {"mine", mine}});
        while (m_chat.size() > kMaxChat)
            m_chat.removeFirst();
        if (!mine)
            ++m_unread;
        emit chatChanged();
        if (!mine)
            emit messageReceived(from.value("name").toString(), message.value("text").toString());
    } else if (event == "ended") {
        const bool wasHost = hosting();
        reset();
        if (!wasHost) {
            setError(tr("The host ended the party."));
            emit ended();
        }
    }
}

void Party::reset() {
    m_retry.stop();
    m_send.stop();
    m_code.clear();
    if (m_reply) {
        auto *reply = m_reply.data();
        m_reply = nullptr;
        reply->abort();
    }
    m_connected = false;
    m_you.clear();
    m_host.clear();
    m_members.clear();
    m_state.clear();
    m_sent.clear();
    m_pending.clear();
    const bool hadChat = !m_chat.isEmpty() || m_unread;
    m_chat.clear();
    m_unread = 0;
    emit changed();
    if (hadChat)
        emit chatChanged();
    emit stateReceived();
}

void Party::leave() {
    if (!active())
        return;
    if (hosting())
        post({{"type", "end"}});
    m_error.clear();
    reset();
}

void Party::say(const QString &text) {
    const auto line = text.trimmed().left(300);
    if (line.isEmpty() || !active())
        return;
    post({{"type", "chat"}, {"text", line}});
}

void Party::markRead() {
    if (m_unread == 0)
        return;
    m_unread = 0;
    emit chatChanged();
}

double Party::hostPosition() const {
    const double position = m_state.value("position").toDouble();
    if (!m_state.value("playing").toBool() || !m_stateClock.isValid())
        return position;
    return position + m_stateClock.elapsed() / 1000.0;
}

void Party::report(const QVariantMap &anime, double episode, bool playing, double position) {
    if (!hosting() || anime.value("anilistId").toInt() <= 0)
        return;
    const QVariantMap next{{"anime", anime}, {"episode", episode}, {"playing", playing}, {"position", position}};
    bool changedNow = m_sent.isEmpty();
    if (!changedNow) {
        const double gap = m_sinceSent.elapsed() / 1000.0;
        const double expected = m_sent.value("position").toDouble() + (m_sent.value("playing").toBool() ? gap : 0);
        changedNow = m_sent.value("anime").toMap().value("anilistId") != anime.value("anilistId")
                     || m_sent.value("episode").toDouble() != episode || m_sent.value("playing").toBool() != playing
                     || std::abs(expected - position) > kSeekJump;
    }
    m_pending = next;
    const qint64 since = m_sinceSent.isValid() ? m_sinceSent.elapsed() : kRefreshMs;
    if (!changedNow && since < kRefreshMs)
        return;
    if (since >= kMinGapMs) {
        sendState();
    } else if (!m_send.isActive()) {
        m_send.start(int(kMinGapMs - since));
    }
}

void Party::sendState() {
    if (m_pending.isEmpty() || !hosting())
        return;
    m_sent = std::exchange(m_pending, QVariantMap());
    m_sinceSent.start();
    const auto anime = m_sent.value("anime").toMap();
    QJsonObject state{{"anime", QJsonObject{{"anilistId", anime.value("anilistId").toInt()},
                                            {"title", anime.value("title").toString()},
                                            {"cover", anime.value("cover").toString()}}},
                      {"episode", m_sent.value("episode").toDouble()},
                      {"playing", m_sent.value("playing").toBool()},
                      {"position", std::max(0.0, m_sent.value("position").toDouble())}};
    post({{"type", "state"}, {"state", state}});
}

void Party::post(const QJsonObject &body) {
    if (m_code.isEmpty())
        return;
    const QUrl url(Account::siteBase() + QStringLiteral("/api/app/party/") + m_code);
    const auto json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    runInBackground([this, url, json] {
        QString error;
        try {
            const auto response = Http::postJson(url, json, {{"Accept", "application/json"}});
            if (!response.ok())
                error = refusal(response);
        } catch (const Error &e) {
            error = e.message();
        }
        if (!error.isEmpty()) {
            qWarning().noquote() << "Party message not sent:" << error;
            QMetaObject::invokeMethod(this, [this, error] { setError(error); });
        }
    });
}

} // namespace sc
