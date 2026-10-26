#include "LineHost.h"

#include "core/Http.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <chrono>
#include <condition_variable>

namespace sc::ext {

namespace {

std::mutex g_mutex;
LineHost::AskHandler g_ask;
QHash<QString, QString> g_agents; // host -> user agent
QString g_extensionAgent;

} // namespace

QString browserCheckAgent(const QString &url) {
    const auto host = QUrl(url).host();
    std::lock_guard lock(g_mutex);
    return g_agents.value(host);
}

void setExtensionAgent(const QString &agent) {
    std::lock_guard lock(g_mutex);
    g_extensionAgent = agent;
}

QString extensionAgent() {
    std::lock_guard lock(g_mutex);
    return g_extensionAgent.isEmpty() ? QString::fromUtf8(Http::userAgent()) : g_extensionAgent;
}

void LineHost::setAskHandler(AskHandler handler) {
    std::lock_guard lock(g_mutex);
    g_ask = std::move(handler);
}

struct LineHost::Waiting {
    std::mutex mutex;
    std::condition_variable arrived;
    bool done = false;
    QJsonObject reply;
};

QJsonValue LineHost::call(const QString &method, const QJsonObject &params) {
    if (!available())
        throw Error(unavailableReason());
    const auto id = m_nextId++;
    auto waiting = std::make_shared<Waiting>();
    {
        std::lock_guard lock(m_mutex);
        m_pending.insert(id, waiting);
    }
    sendLine(QJsonDocument(QJsonObject{{"id", id}, {"method", method}, {"params", params}}).toJson(QJsonDocument::Compact) + '\n');

    const auto giveUp = [&](const Error &error) {
        {
            std::lock_guard lock(m_mutex);
            m_pending.remove(id);
        }
        // The host stops the call; its answer, if any, is dropped.
        sendLine(QJsonDocument(QJsonObject{{"id", 0}, {"method", "cancel"}, {"params", QJsonObject{{"id", id}}}})
                     .toJson(QJsonDocument::Compact) +
                 '\n');
        throw error;
    };
    // A host's own limit (Rpc.kt) and a little more: one series' details,
    // chapters or pages may take minutes from sources that page through
    // the site in a web view (Comix: 23 rounds for 471 chapters).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(LineHost::slowCall(method) ? 315 : 135);
    std::unique_lock lock(waiting->mutex);
    while (!waiting->done) {
        if (CancelScope::cancelled()) {
            lock.unlock();
            giveUp(Error(QStringLiteral("Cancelled"), 499));
        }
        if (std::chrono::steady_clock::now() > deadline) {
            lock.unlock();
            giveUp(Error(QStringLiteral("The extension took too long to answer."), 504));
        }
        waiting->arrived.wait_for(lock, std::chrono::milliseconds(50));
    }
    const auto reply = waiting->reply;
    lock.unlock();
    if (reply.contains(QStringLiteral("error"))) {
        const auto e = reply.value(QStringLiteral("error")).toObject();
        const auto agent = e.value(QStringLiteral("verifyAgent")).toString();
        if (!agent.isEmpty()) {
            std::lock_guard agents(g_mutex);
            g_agents.insert(QUrl(e.value(QStringLiteral("verifyUrl")).toString()).host(), agent);
        }
        throw Error(e.value(QStringLiteral("message")).toString(QStringLiteral("The extension failed.")),
                    e.value(QStringLiteral("status")).toInt(), e.value(QStringLiteral("verifyUrl")).toString());
    }
    return reply.value(QStringLiteral("result"));
}

void LineHost::receive(const QByteArray &line) {
    const auto reply = QJsonDocument::fromJson(line).object();
    if (reply.contains(QStringLiteral("ask"))) {
        // Interactive checks need the request's identity before a failure
        // reply exists; otherwise the visible browser uses a different UA.
        if (reply.value(QStringLiteral("method")).toString() == QLatin1String("readerBrowserCheck")) {
            const auto params = reply.value(QStringLiteral("params")).toObject();
            const auto agent = params.value(QStringLiteral("userAgent")).toString();
            if (!agent.isEmpty()) {
                std::lock_guard lock(g_mutex);
                g_agents.insert(QUrl(params.value(QStringLiteral("url")).toString()).host(), agent);
            }
        }
        const auto ask = reply.value(QStringLiteral("ask")).toInteger();
        const std::weak_ptr<int> alive = m_alive;
        Answer answer = [this, alive, ask](const QJsonValue &result) {
            if (alive.expired())
                return;
            sendLine(QJsonDocument(QJsonObject{{"id", 0}, {"method", "answer"}, {"params", QJsonObject{{"ask", ask}, {"result", result}}}})
                         .toJson(QJsonDocument::Compact) +
                     '\n');
        };
        AskHandler handler;
        {
            std::lock_guard lock(g_mutex);
            handler = g_ask;
        }
        if (handler)
            handler(reply.value(QStringLiteral("method")).toString(), reply.value(QStringLiteral("params")).toObject(), std::move(answer));
        else
            answer(QJsonValue());
        return;
    }
    if (!reply.contains(QStringLiteral("id")))
        return;
    std::shared_ptr<Waiting> waiting;
    {
        std::lock_guard lock(m_mutex);
        waiting = m_pending.take(reply.value(QStringLiteral("id")).toInteger());
    }
    if (!waiting)
        return;
    {
        std::lock_guard lock(waiting->mutex);
        waiting->reply = reply;
        waiting->done = true;
    }
    waiting->arrived.notify_all();
}

void LineHost::failAll(const QString &message) {
    QHash<qint64, std::shared_ptr<Waiting>> all;
    {
        std::lock_guard lock(m_mutex);
        all.swap(m_pending);
    }
    const QJsonObject reply{{"error", QJsonObject{{"message", message}, {"status", 503}}}};
    for (const auto &waiting : all) {
        {
            std::lock_guard lock(waiting->mutex);
            waiting->reply = reply;
            waiting->done = true;
        }
        waiting->arrived.notify_all();
    }
}

bool LineHost::busy() const {
    std::lock_guard lock(m_mutex);
    return !m_pending.isEmpty();
}

} // namespace sc::ext
