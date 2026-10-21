#pragma once

#include "Host.h"

#include <QByteArray>
#include <QHash>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace sc::ext {

// The user agent an extension met a site's browser check with (from the
// host's error), or empty. See ExtensionSource::checkAgent.
QString browserCheckAgent(const QString &url);
// The user agent the extension host sends when it is not SCANCORE's own
// (Android: the web view's, see host/android/Bridge.kt).
void setExtensionAgent(const QString &agent);
QString extensionAgent();

// A host spoken to with one JSON object per line (see host/core/Rpc.kt):
// requests carry an id, answers come back in any order and are matched to
// the call waiting for them. Calls block their worker thread; a cancelled
// one returns at once and tells the host to stop working on it. Subclasses
// say how a line gets there: a helper process's stdin (desktop) or JNI
// (Android).
class LineHost : public Host {
public:
    QJsonValue call(const QString &method, const QJsonObject &params) override;

    // The host's own questions ({"ask": n, "method", "params"}, see Rpc.kt),
    // such as a browser check to pass. Called on the thread lines arrive on;
    // `answer` may be called later from any thread, once.
    using Answer = std::function<void(const QJsonValue &result)>;
    using AskHandler = std::function<void(const QString &method, const QJsonObject &params, Answer answer)>;
    static void setAskHandler(AskHandler handler);

protected:
    // One request line, newline included; from any thread.
    virtual void sendLine(const QByteArray &line) = 0;
    // An answer line from the host.
    void receive(const QByteArray &line);
    // The host went away: every waiting call fails with `message`.
    void failAll(const QString &message);
    bool busy() const;

private:
    struct Waiting;
    mutable std::mutex m_mutex;
    QHash<qint64, std::shared_ptr<Waiting>> m_pending;
    std::atomic<qint64> m_nextId{1};
    // Lets answers to the host's questions see that it is gone.
    std::shared_ptr<int> m_alive = std::make_shared<int>(0);
};

} // namespace sc::ext
