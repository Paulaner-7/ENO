#pragma once

// Internals of the plugin runtime (Runtime.cpp): one Engine per plugin,
// living on a plugin thread; Native is what its JavaScript calls.

#include "Runtime.h"
#include "core/Http.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJSEngine>
#include <QJSValue>
#include <QMutex>
#include <QWaitCondition>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <atomic>
#include <memory>

namespace sc::plugins {

class Engine;

// Lets a waiting caller stop JavaScript that runs too long, from its own
// thread (QJSEngine::setInterrupted is thread-safe).
struct Guard {
    QMutex mutex;
    QJSEngine *js = nullptr;
    // When JavaScript started running now (ms on `clock`), or -1.
    std::atomic<qint64> busySince{-1};
    QElapsedTimer clock;
    void interruptIfBusy(qint64 limitMs);
};

// The guards of one plugin thread's engines, for callers waiting on it
// (one plugin spinning blocks the others on its thread).
struct GuardList {
    QMutex mutex;
    QList<std::weak_ptr<Guard>> guards;
    void add(const std::shared_ptr<Guard> &guard);
    void interruptIfBusy(qint64 limitMs);
};

// A call waiting for a plugin's promise.
struct Pending {
    QMutex mutex;
    QWaitCondition done;
    bool finished = false;
    bool ok = false;
    QJsonValue value;
    QString error;
    int status = 0;
    void finish(bool success, const QJsonValue &result, const QString &message, int code = 0);
};

class Native : public QObject {
    Q_OBJECT
public:
    explicit Native(Engine *engine);
    ~Native() override;

    Q_INVOKABLE int setTimeout(int ms);
    Q_INVOKABLE void clearTimeout(int id);
    Q_INVOKABLE QJSValue parseUrl(const QString &input, const QString &base);
    Q_INVOKABLE int fetch(const QString &url, const QString &method, const QVariantMap &headers, const QString &kind,
                          const QJSValue &data, const QString &encoding);
    Q_INVOKABLE void abortFetch(int id);
    Q_INVOKABLE QString storageGet(const QString &key);
    Q_INVOKABLE void storageSet(const QString &key, const QString &value);
    Q_INVOKABLE QStringList storageKeys();
    Q_INVOKABLE void storageClear();
    Q_INVOKABLE QByteArray utf8Encode(const QString &text);
    Q_INVOKABLE QString decode(const QByteArray &bytes, const QString &label);
    Q_INVOKABLE QString atob(const QString &text);
    Q_INVOKABLE QString btoa(const QString &text);
    Q_INVOKABLE QString btoaBytes(const QByteArray &bytes);
    Q_INVOKABLE void settle(int id, bool ok, const QJSValue &value, const QString &error);

private:
    Engine *m_engine;
    QHash<int, QTimer *> m_timers;
    int m_nextTimer = 0;
    QHash<int, CancelFlag> m_fetches;
    QHash<int, QString> m_encodings;
    int m_nextFetch = 0;
};

class Engine : public QObject {
    Q_OBJECT
public:
    Engine(const PluginRef &plugin, const std::shared_ptr<GuardList> &guards, QObject *parent);
    ~Engine() override;

    const PluginRef &plugin() const { return m_plugin; }
    // Loads the code (once); throws Error.
    void load();
    QJsonObject info();
    // Starts `method`; `pending` is finished when its promise settles.
    void start(const QString &method, const QJsonArray &args, const std::shared_ptr<Pending> &pending);
    // Runs JavaScript from the host (timers, network answers), with the
    // interruption guard.
    void run(const std::function<void()> &work);
    void settled(int id, bool ok, const QJsonValue &value, const QString &error, int status = 0);

    QJSEngine &js() { return *m_js; }
    qint64 idleMs() const { return m_lastUse.elapsed(); }

private:
    QString failure(const QJSValue &error) const;

    PluginRef m_plugin;
    std::unique_ptr<QJSEngine> m_js;
    Native *m_native = nullptr;
    QJSValue m_instance;
    bool m_loaded = false;
    QHash<int, std::shared_ptr<Pending>> m_pending;
    int m_nextCall = 0;
    QElapsedTimer m_lastUse;
    std::shared_ptr<Guard> m_guard = std::make_shared<Guard>();
};

// The engines of one plugin thread.
class Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);
    Engine *engine(const PluginRef &plugin);
    void unload(const QString &key);
    // Set once at construction; safe to use from any thread.
    const std::shared_ptr<GuardList> guards = std::make_shared<GuardList>();

private:
    QHash<QString, Engine *> m_engines;
    QTimer m_sweep;
};

} // namespace sc::plugins
