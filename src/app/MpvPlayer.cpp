#include "MpvPlayer.h"

#include "core/Http.h"
#include "core/StreamRelay.h"
#include "core/Task.h"
#include "extensions/Host.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QHostAddress>
#include <QStandardPaths>
#include <QMutex>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QSet>
#include <QUrl>
#include <QtGui/qguiapplication_platform.h>

#include <algorithm>
#include <atomic>
#include <clocale>
#include <cmath>

#ifdef SCANCORE_HAVE_MPV
#include <mpv/client.h>
#include <mpv/render_gl.h>
#endif
#if defined(SCANCORE_HAVE_MPV) && defined(Q_OS_ANDROID)
#include <QDir>
#include <QFile>
#include <QJniEnvironment>
#include <QJniObject>
#include <QStandardPaths>
#include <QtCore/qcoreapplication_platform.h>
extern "C" {
#include <libavcodec/jni.h>
}
#endif

namespace sc {

namespace {
std::atomic_bool g_sceneSupported{true};
} // namespace

// The mpv instance, shared by the item and its renderer (which lives on the
// scene graph's thread and may outlast the item for a moment), and the way
// mpv's threads reach the item while it exists.
struct MpvCore {
#ifdef SCANCORE_HAVE_MPV
    mpv_handle *mpv = nullptr;
#endif
    QMutex mutex;
    MpvPlayer *item = nullptr;

    // From any thread: calls `method` on the item's thread, if the item is
    // still there then.
    void post(void (MpvPlayer::*method)()) {
        QMutexLocker lock(&mutex);
        if (item)
            QMetaObject::invokeMethod(item, [target = item, method] { (target->*method)(); }, Qt::QueuedConnection);
    }

    ~MpvCore() {
#ifdef SCANCORE_HAVE_MPV
        if (mpv)
            mpv_terminate_destroy(mpv);
#endif
    }
};

#ifdef SCANCORE_HAVE_MPV

namespace {

enum Property : quint64 {
    TimePos = 1,
    Duration,
    Pause,
    PausedForCache,
    Seeking,
    EofReached,
    Volume,
    Mute,
    Speed,
    TrackList,
    CacheDuration,
    Hwdec,
};

QVariant toVariant(const mpv_node &node) {
    switch (node.format) {
    case MPV_FORMAT_STRING: return QString::fromUtf8(node.u.string);
    case MPV_FORMAT_FLAG: return bool(node.u.flag);
    case MPV_FORMAT_INT64: return qint64(node.u.int64);
    case MPV_FORMAT_DOUBLE: return node.u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        for (int i = 0; i < node.u.list->num; ++i)
            list.append(toVariant(node.u.list->values[i]));
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        for (int i = 0; i < node.u.list->num; ++i)
            map.insert(QString::fromUtf8(node.u.list->keys[i]), toVariant(node.u.list->values[i]));
        return map;
    }
    default: return {};
    }
}

void command(mpv_handle *mpv, const QStringList &args) {
    QList<QByteArray> utf8;
    for (const auto &a : args)
        utf8.append(a.toUtf8());
    QList<const char *> argv;
    for (const auto &a : utf8)
        argv.append(a.constData());
    argv.append(nullptr);
    mpv_command_async(mpv, 0, argv.data());
}

void setString(mpv_handle *mpv, const char *name, const QString &value) {
    mpv_set_property_string(mpv, name, value.toUtf8().constData());
}

// Options a video's source may set: how to reach and read it, nothing that
// touches files, scripts or other programs.
const QSet<QString> &allowedOptions() {
    static const QSet<QString> allowed{
        "user-agent", "referrer", "http-header-fields", "network-timeout", "demuxer", "demuxer-lavf-format", "demuxer-lavf-o",
        "stream-lavf-o", "start", "aid", "sid", "vid", "sub-delay", "audio-delay", "cache-secs", "demuxer-readahead-secs"};
    return allowed;
}

#ifdef Q_OS_ANDROID
// Once per process. FFmpeg's MediaCodec decoders and mpv's audio reach
// Android's Java side through the VM and the app's context, and subtitles
// find Android's fonts through a fonts.conf in mpv's own folder (which
// holds nothing else). Returns that folder.
QString androidSetup() {
    static const QString dir = [] {
        av_jni_set_java_vm(QJniEnvironment::javaVM(), nullptr);
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        QJniEnvironment env;
        static jobject app = env->NewGlobalRef(context.object());
        av_jni_set_android_app_ctx(app, nullptr);
        const auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/mpv");
        const auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/fontconfig");
        QDir().mkpath(path);
        QDir().mkpath(cache);
        QFile conf(path + QStringLiteral("/fonts.conf"));
        if (conf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            conf.write(QStringLiteral("<fontconfig>\n<dir>/system/fonts/</dir>\n<dir>/product/fonts/</dir>\n"
                                      "<cachedir>%1</cachedir>\n"
                                      "<alias><family>serif</family><prefer><family>Noto Serif</family></prefer></alias>\n"
                                      "<alias><family>sans-serif</family><prefer><family>Roboto</family><family>Noto Sans</family></prefer></alias>\n"
                                      "<alias><family>monospace</family><prefer><family>Droid Sans Mono</family></prefer></alias>\n"
                                      "</fontconfig>\n")
                           .arg(cache)
                           .toUtf8());
        return path;
    }();
    return dir;
}
#endif

void *procAddress(void *, const char *name) {
    auto *context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void *>(context->getProcAddress(QByteArray(name))) : nullptr;
}

class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(std::shared_ptr<MpvCore> core) : m_core(std::move(core)) {}

    // On the scene graph's thread, with its OpenGL context current, as
    // mpv requires.
    ~MpvRenderer() override {
        if (m_context)
            mpv_render_context_free(m_context);
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
        if (!m_context && m_core->mpv)
            init();
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        return new QOpenGLFramebufferObject(size, format);
    }

    void render() override {
        if (!m_context)
            return;
        auto *fbo = framebufferObject();
        mpv_opengl_fbo target{int(fbo->handle()), fbo->width(), fbo->height(), 0};
        int flip = 0;
        mpv_render_param params[]{{MPV_RENDER_PARAM_OPENGL_FBO, &target},
                                  {MPV_RENDER_PARAM_FLIP_Y, &flip},
                                  {MPV_RENDER_PARAM_INVALID, nullptr}};
        mpv_render_context_render(m_context, params);
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    void init() {
        mpv_opengl_init_params gl{procAddress, nullptr};
        QList<mpv_render_param> params{{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                                       {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl}};
        // Hardware decoders hand frames to OpenGL through the windowing
        // system's display.
#if QT_CONFIG(wayland)
        if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
            params.append({MPV_RENDER_PARAM_WL_DISPLAY, wayland->display()});
#endif
#if QT_CONFIG(xcb)
        if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
            params.append({MPV_RENDER_PARAM_X11_DISPLAY, x11->display()});
#endif
        params.append({MPV_RENDER_PARAM_INVALID, nullptr});
        if (mpv_render_context_create(&m_context, m_core->mpv, params.data()) < 0) {
            qWarning("mpv: no render context");
            m_context = nullptr;
            return;
        }
        mpv_render_context_set_update_callback(
            m_context, [](void *core) { static_cast<MpvCore *>(core)->post(&MpvPlayer::update); }, m_core.get());
        m_core->post(&MpvPlayer::rendererReady);
    }

    std::shared_ptr<MpvCore> m_core;
    mpv_render_context *m_context = nullptr;
};

} // namespace

#else

namespace {
class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(std::shared_ptr<MpvCore>) {}
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override { return new QOpenGLFramebufferObject(size); }
    void render() override {}
};
} // namespace

#endif

bool MpvPlayer::supported() {
#ifdef SCANCORE_HAVE_MPV
    return g_sceneSupported;
#else
    return false;
#endif
}

void MpvPlayer::setSceneSupported(bool supported) { g_sceneSupported = supported; }

MpvPlayer::MpvPlayer(QQuickItem *parent) : QQuickFramebufferObject(parent), m_core(std::make_shared<MpvCore>()) {
    m_core->item = this;
#ifdef SCANCORE_HAVE_MPV
    if (!supported())
        return;
    // mpv reads numbers the C way and refuses to start otherwise.
    std::setlocale(LC_NUMERIC, "C");
#ifdef Q_OS_ANDROID
    const auto configDir = androidSetup().toUtf8();
#endif
    auto *mpv = mpv_create();
    if (!mpv) {
        m_error = QStringLiteral("The video player could not start.");
        return;
    }
    const auto option = [mpv](const char *name, const char *value) { mpv_set_option_string(mpv, name, value); };
    option("vo", "libmpv");
    // Hardware decoding where it is safe (Android: MediaCodec, straight
    // into OpenGL when it can, else copied); SCANCORE_MPV_HWDEC overrides it
    // (e.g. "no" to compare).
#ifdef Q_OS_ANDROID
    const char *hwdec = "mediacodec,mediacodec-copy";
    option("hwdec-codecs", "h264,hevc,mpeg4,mpeg2video,vp8,vp9,av1");
#else
    const char *hwdec = "auto-safe";
#endif
    option("hwdec", qEnvironmentVariableIsSet("SCANCORE_MPV_HWDEC") ? qgetenv("SCANCORE_MPV_HWDEC").constData() : hwdec);
    // Nothing of the user's own mpv setup, no scripts, no downloader. On
    // Android mpv's folder is SCANCORE's own, for fonts.conf.
#ifdef Q_OS_ANDROID
    option("config", "yes");
    option("config-dir", configDir.constData());
#else
    option("config", "no");
#endif
    option("load-scripts", "no");
    option("ytdl", "no");
    option("osc", "no");
    option("input-default-bindings", "no");
    option("input-vo-keyboard", "no");
    option("terminal", "no");
    option("keep-open", "yes");
    option("audio-client-name", "SCANCORE");
    // A demuxer cache that holds HLS streams through slow patches; less on
    // phones.
    option("cache", "yes");
#ifdef Q_OS_ANDROID
    option("demuxer-max-bytes", "64MiB");
    option("demuxer-max-back-bytes", "16MiB");
#else
    option("demuxer-max-bytes", "150MiB");
    option("demuxer-max-back-bytes", "50MiB");
#endif
    option("network-timeout", "30");
    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        m_error = QStringLiteral("The video player could not start.");
        return;
    }
    m_core->mpv = mpv;
    mpv_request_log_messages(mpv, "warn");
    mpv_observe_property(mpv, TimePos, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, Duration, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, Pause, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, PausedForCache, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, Seeking, "seeking", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, EofReached, "eof-reached", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, Volume, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, Mute, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, Speed, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, TrackList, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(mpv, CacheDuration, "demuxer-cache-duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, Hwdec, "hwdec-current", MPV_FORMAT_STRING);
    mpv_set_wakeup_callback(
        mpv, [](void *core) { static_cast<MpvCore *>(core)->post(&MpvPlayer::drainEvents); }, m_core.get());
#endif
}

MpvPlayer::~MpvPlayer() {
    if (m_holdsHost)
        ext::holdHost(false);
    {
        QMutexLocker lock(&m_core->mutex);
        m_core->item = nullptr;
    }
#ifdef SCANCORE_HAVE_MPV
    // Quiet at once; the renderer may hold mpv a moment longer.
    if (m_core->mpv)
        command(m_core->mpv, {QStringLiteral("stop")});
#endif
    // Closing mpv waits for its threads: not on the interface's thread.
    runInBackground([core = std::move(m_core)]() mutable { core.reset(); });
}

QQuickFramebufferObject::Renderer *MpvPlayer::createRenderer() const { return new MpvRenderer(m_core); }

void MpvPlayer::setSource(const QString &source) {
    if (source == m_source)
        return;
    m_source = source;
    emit sourceChanged();
    // Headers and options usually change with it: load once they all have.
    if (!m_loadQueued) {
        m_loadQueued = true;
        QMetaObject::invokeMethod(this, &MpvPlayer::load, Qt::QueuedConnection);
    }
}

void MpvPlayer::setHeaders(const QVariantMap &headers) {
    if (headers == m_headers)
        return;
    m_headers = headers;
    emit headersChanged();
}

void MpvPlayer::setOptions(const QVariantMap &options) {
    if (options == m_options)
        return;
    m_options = options;
    emit optionsChanged();
}

void MpvPlayer::setSubtitles(const QVariantList &subtitles) {
    if (subtitles == m_subtitles)
        return;
    m_subtitles = subtitles;
    emit subtitlesChanged();
}

void MpvPlayer::setAudio(const QVariantList &audio) {
    if (audio == m_audio)
        return;
    m_audio = audio;
    emit audioChanged();
}

void MpvPlayer::setStartAt(double seconds) {
    if (seconds == m_startAt)
        return;
    m_startAt = seconds;
    emit startAtChanged();
}

void MpvPlayer::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
    if (!error.isEmpty() && m_buffering) {
        m_buffering = false;
        emit bufferingChanged();
    }
}

void MpvPlayer::rendererReady() {
    m_rendererReady = true;
    if (m_loadWaiting)
        load();
}

void MpvPlayer::load() {
    m_loadQueued = false;
    // mpv opens its video output with the file, which needs the render
    // context: until the scene has drawn this item once, the file waits.
    m_loadWaiting = !m_rendererReady && !m_source.isEmpty();
    if (m_loadWaiting) {
        update();
        return;
    }
    m_position = 0;
    m_reportedPosition = -1;
    m_duration = 0;
    m_buffered = 0;
    m_loaded = false;
    m_ended = false;
    m_cachePause = false;
    m_seeking = false;
    m_tracks.clear();
    emit positionChanged();
    emit durationChanged();
    emit bufferedChanged();
    emit loadedChanged();
    emit endedChanged();
    emit tracksChanged();
    setError({});
#ifdef SCANCORE_HAVE_MPV
    const QUrl url(m_source);
    // A server on this device is one an extension runs in the host.
    const bool fromHost =
        !url.isLocalFile() && (QHostAddress(url.host()).isLoopback() || url.host() == QLatin1String("localhost"));
    if (fromHost != m_holdsHost) {
        m_holdsHost = fromHost;
        ext::holdHost(fromHost);
    }
    auto *mpv = m_core->mpv;
    if (!mpv)
        return;
    if (m_source.isEmpty()) {
        command(mpv, {QStringLiteral("stop")});
        return;
    }
    if (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http") && !url.isLocalFile()) {
        setError(QStringLiteral("This video's address cannot be played."));
        return;
    }
    // The headers its site wants, User-Agent and Referer in mpv's own
    // options.
    QString agent = QString::fromUtf8(Http::userAgent());
    QString referer;
    QList<QByteArray> fields;
    for (auto it = m_headers.begin(); it != m_headers.end(); ++it) {
        const auto name = it.key().toLower();
        if (name == QLatin1String("user-agent"))
            agent = it.value().toString();
        else if (name == QLatin1String("referer"))
            referer = it.value().toString();
        else
            fields.append((it.key() + QStringLiteral(": ") + it.value().toString()).toUtf8());
    }
    setString(mpv, "user-agent", agent);
    setString(mpv, "referrer", referer);
    QList<mpv_node> nodes;
    for (auto &f : fields) {
        mpv_node n;
        n.format = MPV_FORMAT_STRING;
        n.u.string = f.data();
        nodes.append(n);
    }
    mpv_node_list list{int(nodes.size()), nodes.data(), nullptr};
    mpv_node value;
    value.format = MPV_FORMAT_NODE_ARRAY;
    value.u.list = &list;
    mpv_set_property(mpv, "http-header-fields", MPV_FORMAT_NODE, &value);
    // mpv keeps options from one file to the next: the last video's go back
    // to their defaults first (a downloaded episode's HLS demuxer must not
    // reach the next stream). An empty demuxer is mpv's own choice; there is
    // no demuxer named "auto", and asking for it fails every file.
    static const QList<QPair<const char *, const char *>> defaults{
        {"demuxer", ""}, {"demuxer-lavf-format", ""}, {"demuxer-lavf-o", ""}, {"stream-lavf-o", ""}, {"aid", "auto"}, {"sid", "auto"},
        {"vid", "auto"}, {"sub-delay", "0"}, {"audio-delay", "0"}};
    for (const auto &[name, value] : defaults)
        if (!m_options.contains(QLatin1String(name)))
            mpv_set_property_string(mpv, name, value);
    for (auto it = m_options.begin(); it != m_options.end(); ++it) {
        if (allowedOptions().contains(it.key()))
            setString(mpv, it.key().toUtf8().constData(), it.value().toString());
        else
            qInfo("mpv: the source's option %s is not used", qPrintable(it.key()));
    }
    setString(mpv, "start", m_startAt > 0 ? QString::number(m_startAt) : QStringLiteral("none"));
    // The last video's end paused mpv (keep-open); the next one plays.
    setString(mpv, "pause", QStringLiteral("no"));
    m_buffering = true;
    emit bufferingChanged();
    // Streams go through SCANCORE's relay, which undoes what anime CDNs
    // dress HLS streams in (core/StreamRelay.h); SCANCORE_STREAM_RELAY=0
    // plays them direct (e.g. to compare).
    auto target = m_source;
    if (video::StreamRelay::wanted(url) && qgetenv("SCANCORE_STREAM_RELAY") != "0") {
        Headers headers;
        for (auto it = m_headers.begin(); it != m_headers.end(); ++it)
            headers.append({it.key().toUtf8(), it.value().toString().toUtf8()});
        target = video::StreamRelay::wrap(url, headers).toString(QUrl::FullyEncoded);
    }
    command(mpv, {QStringLiteral("loadfile"), target, QStringLiteral("replace")});
#endif
}

void MpvPlayer::drainEvents() {
#ifdef SCANCORE_HAVE_MPV
    auto *mpv = m_core->mpv;
    if (!mpv)
        return;
    while (true) {
        auto *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        switch (event->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            const auto *p = static_cast<mpv_event_property *>(event->data);
            const bool has = p->format != MPV_FORMAT_NONE && p->data;
            const auto number = [&] { return has && p->format == MPV_FORMAT_DOUBLE ? *static_cast<double *>(p->data) : 0.0; };
            const auto flag = [&] { return has && p->format == MPV_FORMAT_FLAG && *static_cast<int *>(p->data) != 0; };
            switch (event->reply_userdata) {
            case TimePos:
                m_position = number();
                // QML hears about it a few times a second, and at once after a seek.
                if (std::abs(m_position - m_reportedPosition) >= 0.25) {
                    m_reportedPosition = m_position;
                    emit positionChanged();
                }
                break;
            case Duration:
                m_duration = number();
                emit durationChanged();
                break;
            case Pause:
                m_paused = flag();
                emit pausedChanged();
                break;
            case PausedForCache:
                m_cachePause = flag();
                break;
            case Seeking:
                m_seeking = flag();
                break;
            case EofReached:
                if (flag() && !m_ended) {
                    m_ended = true;
                    emit endedChanged();
                    emit finished();
                }
                break;
            case Volume:
                m_volume = number();
                emit volumeChanged();
                break;
            case Mute:
                m_muted = flag();
                emit mutedChanged();
                break;
            case Speed:
                m_speed = number();
                emit speedChanged();
                break;
            case TrackList: {
                QVariantList tracks;
                if (has && p->format == MPV_FORMAT_NODE)
                    for (const auto &t : toVariant(*static_cast<mpv_node *>(p->data)).toList()) {
                        const auto m = t.toMap();
                        tracks.append(QVariantMap{{"id", m.value("id")},
                                                  {"type", m.value("type")},
                                                  {"title", m.value("title")},
                                                  {"lang", m.value("lang")},
                                                  {"selected", m.value("selected")},
                                                  {"external", m.value("external")}});
                    }
                m_tracks = tracks;
                emit tracksChanged();
                break;
            }
            case CacheDuration:
                m_buffered = number();
                emit bufferedChanged();
                break;
            case Hwdec:
                m_hwdec = has && p->format == MPV_FORMAT_STRING ? QString::fromUtf8(*static_cast<char **>(p->data)) : QString();
                if (m_hwdec == QLatin1String("no"))
                    m_hwdec.clear();
                emit hwdecChanged();
                break;
            }
            break;
        }
        case MPV_EVENT_FILE_LOADED: {
            m_loaded = true;
            emit loadedChanged();
            // External tracks; the first subtitle is shown.
            for (int i = 0; i < m_subtitles.size(); ++i) {
                const auto t = m_subtitles[i].toMap();
                command(mpv, {QStringLiteral("sub-add"), t.value("url").toString(), i == 0 ? QStringLiteral("select") : QStringLiteral("auto"),
                              t.value("lang").toString(), t.value("lang").toString()});
            }
            for (const auto &a : m_audio) {
                const auto t = a.toMap();
                command(mpv, {QStringLiteral("audio-add"), t.value("url").toString(), QStringLiteral("auto"), t.value("lang").toString(),
                              t.value("lang").toString()});
            }
            break;
        }
        case MPV_EVENT_END_FILE: {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end->reason == MPV_END_FILE_REASON_ERROR) {
                setError(QStringLiteral("The video could not be played (%1).").arg(QString::fromUtf8(mpv_error_string(end->error))));
            }
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {
            const auto *log = static_cast<mpv_event_log_message *>(event->data);
            qWarning("mpv [%s] %s", log->prefix, QByteArray(log->text).trimmed().constData());
            break;
        }
        default: break;
        }
    }
    // Waiting: for the first frame, after a seek, or for the network.
    const bool buffering = !m_source.isEmpty() && m_error.isEmpty() && !m_ended && (!m_loaded || m_cachePause || m_seeking);
    if (buffering != m_buffering) {
        m_buffering = buffering;
        emit bufferingChanged();
    }
#endif
}

void MpvPlayer::setPaused(bool paused) {
#ifdef SCANCORE_HAVE_MPV
    if (m_core->mpv) {
        int flag = paused;
        mpv_set_property_async(m_core->mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
    }
#else
    Q_UNUSED(paused)
#endif
}

void MpvPlayer::setVolume(double volume) {
#ifdef SCANCORE_HAVE_MPV
    if (m_core->mpv) {
        double value = std::clamp(volume, 0.0, 130.0);
        mpv_set_property_async(m_core->mpv, 0, "volume", MPV_FORMAT_DOUBLE, &value);
    }
#else
    Q_UNUSED(volume)
#endif
}

void MpvPlayer::setMuted(bool muted) {
#ifdef SCANCORE_HAVE_MPV
    if (m_core->mpv) {
        int flag = muted;
        mpv_set_property_async(m_core->mpv, 0, "mute", MPV_FORMAT_FLAG, &flag);
    }
#else
    Q_UNUSED(muted)
#endif
}

void MpvPlayer::setSpeed(double speed) {
#ifdef SCANCORE_HAVE_MPV
    if (m_core->mpv) {
        double value = std::clamp(speed, 0.25, 4.0);
        mpv_set_property_async(m_core->mpv, 0, "speed", MPV_FORMAT_DOUBLE, &value);
    }
#else
    Q_UNUSED(speed)
#endif
}

void MpvPlayer::setSubtitlePosition(int position) {
    position = std::clamp(position, 0, 150);
    if (position == m_subtitlePosition)
        return;
    m_subtitlePosition = position;
    emit subtitlePositionChanged();
#ifdef SCANCORE_HAVE_MPV
    if (m_core->mpv) {
        qint64 value = position;
        mpv_set_property_async(m_core->mpv, 0, "sub-pos", MPV_FORMAT_INT64, &value);
    }
#endif
}

#ifdef SCANCORE_HAVE_MPV
namespace {

// Anime4K's mode A: restore, then upscale; "fast" for most graphics, "hq"
// for strong ones. The shaders come from the app's resources into its
// cache folder once, as mpv reads files.
QStringList sharpenShaders(const QString &mode) {
    static const QHash<QString, QStringList> modes{
        {QStringLiteral("fast"),
         {"Clamp_Highlights", "Restore_CNN_M", "Upscale_CNN_x2_M", "AutoDownscalePre_x2", "AutoDownscalePre_x4", "Upscale_CNN_x2_S"}},
        {QStringLiteral("hq"),
         {"Clamp_Highlights", "Restore_CNN_VL", "Upscale_CNN_x2_VL", "AutoDownscalePre_x2", "AutoDownscalePre_x4", "Upscale_CNN_x2_M"}}};
    QStringList out;
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/anime4k");
    QDir().mkpath(dir);
    for (const auto &name : modes.value(mode)) {
        const auto file = QStringLiteral("Anime4K_%1.glsl").arg(name);
        const auto path = dir + QLatin1Char('/') + file;
        const QString resource = QStringLiteral(":/shaders/anime4k/") + file;
        if (QFileInfo(path).size() != QFileInfo(resource).size()) {
            QFile::remove(path);
            if (!QFile::copy(resource, path))
                return {};
        }
        out.append(path);
    }
    return out;
}

} // namespace
#endif

void MpvPlayer::setLook(const QVariantMap &look) {
    if (look == m_look)
        return;
    m_look = look;
    emit lookChanged();
#ifdef SCANCORE_HAVE_MPV
    auto *mpv = m_core->mpv;
    if (!mpv)
        return;
    const auto number = [&](const char *key, double fallback) { return look.value(QLatin1String(key), fallback).toDouble(); };
    setString(mpv, "sub-scale", QString::number(std::clamp(number("subtitleScale", 1), 0.5, 3.0)));
    // Older mpv names it "sub-border-*".
    const auto outline = QString::number(std::clamp(number("subtitleOutline", 3), 0.0, 10.0));
    if (mpv_set_property_string(mpv, "sub-outline-size", outline.toUtf8().constData()) < 0)
        setString(mpv, "sub-border-size", outline);
    const auto color = look.value(QStringLiteral("subtitleColor")).toString();
    setString(mpv, "sub-color", QColor::isValidColorName(color) ? color : QStringLiteral("#FFFFFFFF"));
    // A dark box behind the lines instead of an outline.
    const bool box = look.value(QStringLiteral("subtitleBox")).toBool();
    setString(mpv, "sub-border-style", box ? QStringLiteral("opaque-box") : QStringLiteral("outline-and-shadow"));
    const auto boxColor = box ? QStringLiteral("#B0000000") : QStringLiteral("#FF000000");
    if (mpv_set_property_string(mpv, "sub-outline-color", boxColor.toUtf8().constData()) < 0)
        setString(mpv, "sub-border-color", boxColor);
    // A list, so no separator (":" or ";" by system) gets in the way.
    const auto shaders = sharpenShaders(look.value(QStringLiteral("sharpen")).toString());
    QList<QByteArray> paths;
    for (const auto &path : shaders)
        paths.append(path.toUtf8());
    QList<mpv_node> nodes;
    for (auto &path : paths) {
        mpv_node n;
        n.format = MPV_FORMAT_STRING;
        n.u.string = path.data();
        nodes.append(n);
    }
    mpv_node_list list{int(nodes.size()), nodes.data(), nullptr};
    mpv_node value;
    value.format = MPV_FORMAT_NODE_ARRAY;
    value.u.list = &list;
    mpv_set_property(mpv, "glsl-shaders", MPV_FORMAT_NODE, &value);
#endif
}

void MpvPlayer::seek(double seconds) {
#ifdef SCANCORE_HAVE_MPV
    if (m_core->mpv)
        command(m_core->mpv, {QStringLiteral("seek"), QString::number(std::max(0.0, seconds)), QStringLiteral("absolute")});
#else
    Q_UNUSED(seconds)
#endif
}

void MpvPlayer::seekBy(double seconds) {
#ifdef SCANCORE_HAVE_MPV
    if (m_core->mpv)
        command(m_core->mpv, {QStringLiteral("seek"), QString::number(seconds), QStringLiteral("relative")});
#else
    Q_UNUSED(seconds)
#endif
}

void MpvPlayer::togglePause() { setPaused(!m_paused); }

void MpvPlayer::setTrack(const QString &type, int id) {
#ifdef SCANCORE_HAVE_MPV
    const char *name = type == QLatin1String("audio") ? "aid" : type == QLatin1String("video") ? "vid" : "sid";
    if (m_core->mpv)
        setString(m_core->mpv, name, id > 0 ? QString::number(id) : QStringLiteral("no"));
#else
    Q_UNUSED(type)
    Q_UNUSED(id)
#endif
}

void MpvPlayer::stop() { setSource({}); }

} // namespace sc
