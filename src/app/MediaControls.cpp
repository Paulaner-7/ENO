#include "MediaControls.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

#ifdef SCANCORE_HAVE_DBUS
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QVariantMap>
#endif

namespace sc {

#ifdef Q_OS_ANDROID
namespace {
constexpr auto kServiceClass = "com/mediumdev/scancore/ReadAloudService";

void jniCommand(JNIEnv *env, jclass, jstring word) {
    const jchar *chars = env->GetStringChars(word, nullptr);
    const QString text(reinterpret_cast<const QChar *>(chars), env->GetStringLength(word));
    env->ReleaseStringChars(word, chars);
    MediaControls::instance()->command(text);
}
} // namespace
#endif

#ifdef SCANCORE_HAVE_DBUS
namespace {

// MPRIS (specifications.freedesktop.org/mpris-spec): what desktops' media
// keys and player widgets talk to.
class MprisRoot : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(bool CanQuit READ no CONSTANT)
    Q_PROPERTY(bool CanRaise READ yes CONSTANT)
    Q_PROPERTY(bool HasTrackList READ no CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ none CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ none CONSTANT)

public:
    explicit MprisRoot(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    QString identity() const { return QStringLiteral("SCANCORE"); }
    QString desktopEntry() const { return QStringLiteral("com.mediumdev.scancore"); }
    bool no() const { return false; }
    bool yes() const { return true; }
    QStringList none() const { return {}; }

public slots:
    void Raise() {
        for (auto *w : QGuiApplication::topLevelWindows()) {
            w->show();
            w->raise();
            w->requestActivate();
        }
    }
    void Quit() {}
};

class MprisPlayer : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ status)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Rate READ one CONSTANT)
    Q_PROPERTY(double MinimumRate READ one CONSTANT)
    Q_PROPERTY(double MaximumRate READ one CONSTANT)
    Q_PROPERTY(double Volume READ one CONSTANT)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(bool CanGoNext READ yes CONSTANT)
    Q_PROPERTY(bool CanGoPrevious READ yes CONSTANT)
    Q_PROPERTY(bool CanPlay READ yes CONSTANT)
    Q_PROPERTY(bool CanPause READ yes CONSTANT)
    Q_PROPERTY(bool CanSeek READ no CONSTANT)
    Q_PROPERTY(bool CanControl READ yes CONSTANT)

public:
    explicit MprisPlayer(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    bool playing = false;
    QString title, subtitle, cover;

    QString status() const { return playing ? QStringLiteral("Playing") : QStringLiteral("Paused"); }
    QVariantMap metadata() const {
        QVariantMap m{{QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath("/com/mediumdev/scancore/track"))},
                      {QStringLiteral("xesam:title"), subtitle.isEmpty() ? title : subtitle},
                      {QStringLiteral("xesam:album"), title},
                      {QStringLiteral("xesam:artist"), QStringList{title}}};
        if (cover.startsWith(QLatin1String("http")))
            m.insert(QStringLiteral("mpris:artUrl"), cover);
        return m;
    }
    double one() const { return 1.0; }
    qlonglong position() const { return 0; }
    bool yes() const { return true; }
    bool no() const { return false; }

    // Tells listeners what changed (D-Bus properties do not by themselves).
    void announce() {
        auto signal = QDBusMessage::createSignal(QStringLiteral("/org/mpris/MediaPlayer2"),
                                                 QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
        signal << QStringLiteral("org.mpris.MediaPlayer2.Player")
               << QVariantMap{{QStringLiteral("PlaybackStatus"), status()}, {QStringLiteral("Metadata"), metadata()}} << QStringList{};
        QDBusConnection::sessionBus().send(signal);
    }

public slots:
    void Play() { MediaControls::instance()->command(QStringLiteral("play")); }
    void Pause() { MediaControls::instance()->command(QStringLiteral("pause")); }
    void PlayPause() { MediaControls::instance()->command(QStringLiteral("toggle")); }
    void Stop() { MediaControls::instance()->command(QStringLiteral("stop")); }
    void Next() { MediaControls::instance()->command(QStringLiteral("next")); }
    void Previous() { MediaControls::instance()->command(QStringLiteral("previous")); }
    void Seek(qlonglong) {}
    void SetPosition(const QDBusObjectPath &, qlonglong) {}
    void OpenUri(const QString &) {}
};

} // namespace
#endif

MediaControls *MediaControls::instance() {
    static auto *controls = new MediaControls(QCoreApplication::instance());
    return controls;
}

MediaControls::MediaControls(QObject *parent) : QObject(parent) {
#ifdef Q_OS_ANDROID
    const JNINativeMethod methods[] = {
        {"nativeCommand", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniCommand)},
    };
    QJniEnvironment env;
    env.registerNativeMethods(kServiceClass, methods, 1);
#endif
}

void MediaControls::command(const QString &word) {
    QMetaObject::invokeMethod(this, [this, word] {
        if (word == QLatin1String("play"))
            emit playRequested();
        else if (word == QLatin1String("pause"))
            emit pauseRequested();
        else if (word == QLatin1String("toggle"))
            emit toggleRequested();
        else if (word == QLatin1String("next"))
            emit nextRequested();
        else if (word == QLatin1String("previous"))
            emit previousRequested();
        else if (word == QLatin1String("stop"))
            emit stopRequested();
    });
}

void MediaControls::show(bool active, bool playing, const QString &title, const QString &subtitle, const QString &cover) {
    if (active == m_active && playing == m_playing && title == m_title && subtitle == m_subtitle && cover == m_cover)
        return;
    m_active = active;
    m_playing = playing;
    m_title = title;
    m_subtitle = subtitle;
    m_cover = cover;
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(
        kServiceClass, "show", "(Landroid/content/Context;ZZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QNativeInterface::QAndroidApplication::context().object(), jboolean(active), jboolean(playing),
        QJniObject::fromString(title).object<jstring>(), QJniObject::fromString(subtitle).object<jstring>(),
        QJniObject::fromString(tr("Reading aloud")).object<jstring>());
#elif defined(SCANCORE_HAVE_DBUS)
    auto bus = QDBusConnection::sessionBus();
    const auto service = QStringLiteral("org.mpris.MediaPlayer2.scancore");
    if (!active) {
        if (m_mpris) {
            bus.unregisterObject(QStringLiteral("/org/mpris/MediaPlayer2"));
            bus.unregisterService(service);
            m_mpris->deleteLater();
            m_mpris = nullptr;
        }
        return;
    }
    if (!m_mpris) {
        m_mpris = new QObject(this);
        new MprisRoot(m_mpris);
        new MprisPlayer(m_mpris);
        bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), m_mpris, QDBusConnection::ExportAdaptors);
        bus.registerService(service);
    }
    if (auto *player = m_mpris->findChild<MprisPlayer *>()) {
        player->playing = playing;
        player->title = title;
        player->subtitle = subtitle;
        player->cover = cover;
        player->announce();
    }
#else
    Q_UNUSED(cover)
#endif
}

} // namespace sc

#ifdef SCANCORE_HAVE_DBUS
#include "MediaControls.moc"
#endif
