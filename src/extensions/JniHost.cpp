#include "JniHost.h"

#include <QCoreApplication>
#include <QDir>
#include <QJniEnvironment>
#include <QJniObject>

#include <mutex>

namespace sc::ext {

namespace {

constexpr auto kBridge = "com/mediumdev/scancore/host/android/Bridge";

// The host answers arrive for; replaced when the app sets up another.
std::mutex g_mutex;
std::weak_ptr<JniHost> g_current;

} // namespace

struct JniHost::State {
    Launch launch;
    std::once_flag started;
    bool ok = false;
};

JniHost::JniHost(Launch launch) : m_state(std::make_unique<State>()) { m_state->launch = std::move(launch); }

JniHost::~JniHost() = default;

std::shared_ptr<JniHost> JniHost::create(Launch launch) {
    std::shared_ptr<JniHost> host(new JniHost(std::move(launch)));
    std::lock_guard lock(g_mutex);
    g_current = host;
    return host;
}

void JniHost::reply(const QByteArray &line) {
    std::shared_ptr<JniHost> host;
    {
        std::lock_guard lock(g_mutex);
        host = g_current.lock();
    }
    if (host)
        host->receive(line);
}

QJsonValue JniHost::call(const QString &method, const QJsonObject &params) {
    // Sets up the host's libraries on first use, on the worker thread
    // asking (a few hundred milliseconds), not at app start.
    std::call_once(m_state->started, [this] {
        static const bool registered = [] {
            const JNINativeMethod methods[] = {
                {"reply", "(Ljava/lang/String;)V", reinterpret_cast<void *>(+[](JNIEnv *env, jclass, jstring line) {
                     // UTF-16: JNI's own "UTF" is not UTF-8 for emoji.
                     const jchar *chars = env->GetStringChars(line, nullptr);
                     const auto text = QString(reinterpret_cast<const QChar *>(chars), env->GetStringLength(line));
                     env->ReleaseStringChars(line, chars);
                     JniHost::reply(text.toUtf8());
                 })},
            };
            QJniEnvironment env;
            return env.registerNativeMethods(kBridge, methods, 1);
        }();
        if (!registered)
            return;
        const auto &launch = m_state->launch;
        QDir().mkpath(launch.root);
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        QJniObject::callStaticMethod<void>(kBridge, "start",
                                           "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                           context.object(), QJniObject::fromString(launch.root).object<jstring>(),
                                           QJniObject::fromString(launch.userAgent).object<jstring>(),
                                           QJniObject::fromString(launch.appVersion).object<jstring>());
        QJniEnvironment env;
        m_state->ok = !env.checkAndClearExceptions();
        // Extensions send the web view's user agent; the check window for
        // their sites uses it too.
        if (m_state->ok)
            setExtensionAgent(QJniObject::callStaticObjectMethod(kBridge, "userAgent", "()Ljava/lang/String;").toString());
    });
    if (!m_state->ok)
        throw Error(QStringLiteral("The extension host could not start."), 503);
    return LineHost::call(method, params);
}

void JniHost::sendLine(const QByteArray &line) {
    // Bridge.send() returns at once; the call runs on the host's own
    // threads.
    QJniObject::callStaticMethod<void>(kBridge, "send", "(Ljava/lang/String;)V",
                                       QJniObject::fromString(QString::fromUtf8(line.trimmed())).object<jstring>());
}

} // namespace sc::ext
