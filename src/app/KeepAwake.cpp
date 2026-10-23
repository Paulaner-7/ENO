#include "KeepAwake.h"

#include <QSet>

#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>
#elif defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <IOKit/pwr_mgt/IOPMLib.h>
#elif defined(SCANCORE_HAVE_DBUS)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDebug>
#endif

namespace sc::keepAwake {

namespace {

QSet<QString> g_holders;

#if defined(SCANCORE_HAVE_DBUS)
// The screensaver's cookie, or the portal request's path (closing it lets go).
uint g_cookie = 0;
QString g_portalRequest;
#elif defined(Q_OS_MACOS)
IOPMAssertionID g_assertion = 0;
#endif

void apply(bool on) {
#if defined(Q_OS_ANDROID)
    QJniObject::callStaticMethod<void>("com/mediumdev/scancore/Screen", "keepOn", "(Landroid/app/Activity;Z)V",
                                       QNativeInterface::QAndroidApplication::context().object(), jboolean(on));
#elif defined(Q_OS_WIN)
    SetThreadExecutionState(on ? ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED : ES_CONTINUOUS);
#elif defined(Q_OS_MACOS)
    if (on && g_assertion == 0) {
        IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
                                    CFSTR("SCANCORE is playing a video"), &g_assertion);
    } else if (!on && g_assertion != 0) {
        IOPMAssertionRelease(g_assertion);
        g_assertion = 0;
    }
#elif defined(SCANCORE_HAVE_DBUS)
    auto bus = QDBusConnection::sessionBus();
    if (on) {
        // Most desktops (KDE, GNOME, Xfce, hypridle, swayidle helpers).
        QDBusInterface saver(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"),
                             QStringLiteral("org.freedesktop.ScreenSaver"), bus);
        if (saver.isValid()) {
            const QDBusReply<uint> reply = saver.call(QStringLiteral("Inhibit"), QStringLiteral("SCANCORE"), QStringLiteral("Playing a video"));
            if (reply.isValid()) {
                g_cookie = reply.value();
                return;
            }
        }
        // A sandbox (Flatpak) without that name: the desktop portal.
        QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral("/org/freedesktop/portal/desktop"),
                              QStringLiteral("org.freedesktop.portal.Inhibit"), bus);
        if (portal.isValid()) {
            // 8: idle (the screen dimming and locking).
            const QDBusReply<QDBusObjectPath> reply =
                portal.call(QStringLiteral("Inhibit"), QString(), 8u, QVariantMap{{QStringLiteral("reason"), QStringLiteral("Playing a video")}});
            if (reply.isValid())
                g_portalRequest = reply.value().path();
        }
        if (g_cookie == 0 && g_portalRequest.isEmpty())
            qInfo() << "Keep awake: no screensaver or portal answered";
        return;
    }
    if (g_cookie != 0) {
        QDBusInterface saver(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"),
                             QStringLiteral("org.freedesktop.ScreenSaver"), bus);
        saver.call(QDBus::NoBlock, QStringLiteral("UnInhibit"), g_cookie);
        g_cookie = 0;
    }
    if (!g_portalRequest.isEmpty()) {
        bus.send(QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"), g_portalRequest,
                                                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Close")));
        g_portalRequest.clear();
    }
#else
    Q_UNUSED(on)
#endif
}

} // namespace

void hold(const QString &holder, bool on) {
    const bool before = !g_holders.isEmpty();
    if (on)
        g_holders.insert(holder);
    else
        g_holders.remove(holder);
    const bool after = !g_holders.isEmpty();
    if (before != after)
        apply(after);
}

} // namespace sc::keepAwake
