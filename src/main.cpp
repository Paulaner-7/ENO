#include "app/Account.h"
#include "app/Activity.h"
#include "app/App.h"
#include "app/Profiler.h"
#include "app/Customize.h"
#include "app/Extensions.h"
#include "app/Images.h"
#include "app/MpvPlayer.h"
#include "app/Presence.h"
#include "app/CrashLog.h"
#include "app/SingleInstance.h"
#include "app/Sync.h"
#include "app/Updater.h"
#include "core/Cookies.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/ImageCache.h"
#include "extensions/Manager.h"
#include "sources/Manifest.h"
#include "sources/anime/Registry.h"
#include "sources/catalogue/Catalogue.h"
#include "sources/comics/Registry.h"
#include "sources/local/Local.h"
#include "sources/novels/Registry.h"

#ifdef SCANCORE_HAVE_WEBVIEW
#include <QtWebView/QtWebView>
#endif
#ifdef Q_OS_ANDROID
#include <QFileOpenEvent>
#include <QGuiApplication>
#else
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#endif
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QOpenGLContext>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSurfaceFormat>
#include <QStandardPaths>
#include <QTimer>

#include <stdexcept>
#include <QUrl>
#include <QtCore/private/qabstractanimation_p.h>
#include <cmath>

#ifdef SCANCORE_HAVE_WEBENGINE
#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include <QtWebEngineQuick/QtWebEngineQuick>
#endif
#ifdef SCANCORE_HAVE_PDF
#include <QBuffer>
#include <QPdfDocument>
#endif

using namespace sc;

namespace {

void loadFonts() {
    for (const auto *font : {":/fonts/DMSans.ttf", ":/fonts/DMSans-Italic.ttf", ":/fonts/SpaceGrotesk.ttf",
                             ":/fonts/Literata.ttf", ":/fonts/Literata-Italic.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(font));
    QFont font(QStringLiteral("DM Sans"));
    font.setPixelSize(14);
    font.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(font);
}

#if defined(Q_OS_MACOS) || defined(Q_OS_ANDROID)
// Finder opens files (a .sct theme, a comic) with an event, not arguments;
// links SCANCORE answers (scancore://, a repository's one-tap link) come the
// same way on macOS and Android.
class FileOpenFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::FileOpen)
            return QObject::eventFilter(watched, event);
        const auto *open = static_cast<QFileOpenEvent *>(event);
        const auto target = open->file().isEmpty() ? open->url().toString() : open->file();
        QTimer::singleShot(0, App::instance(), [target] { emit App::instance()->openRequested(target); });
        return true;
    }
};
#endif

// PDF pages on this device, drawn as pictures for the comic reader.
void registerPdf() {
#ifdef SCANCORE_HAVE_PDF
    local::setPdfRenderer({[](const QString &path) {
                               QPdfDocument doc;
                               return doc.load(path) == QPdfDocument::Error::None ? doc.pageCount() : 0;
                           },
                           [](const QString &path, int page, int width) -> QByteArray {
                               QPdfDocument doc;
                               if (doc.load(path) != QPdfDocument::Error::None)
                                   return {};
                               const auto size = doc.pagePointSize(page);
                               if (size.isEmpty())
                                   return {};
                               const auto image = doc.render(page, QSize(width, int(width * size.height() / size.width())));
                               QByteArray png;
                               QBuffer buffer(&png);
                               buffer.open(QIODevice::WriteOnly);
                               image.save(&buffer, "PNG");
                               return png;
                           }});
#endif
}

void openStorage() {
    const auto data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(data);
    QDir().mkpath(cache);
    const auto db = data + QStringLiteral("/scancore.db");
    // A backup chosen in Settings is swapped in before anything opens the
    // database.
    if (QFile::exists(db + QStringLiteral(".restore"))) {
        QFile::remove(db + QStringLiteral("-wal"));
        QFile::remove(db + QStringLiteral("-shm"));
        QFile::remove(db + QStringLiteral(".previous"));
        QFile::rename(db, db + QStringLiteral(".previous"));
        QFile::rename(db + QStringLiteral(".restore"), db);
    }
    Database::open(db);
    CookieJar::shared().load(data + QStringLiteral("/cookies.txt"));
    // The size chosen in Settings → Storage (it was always 1 GB before).
    const qint64 cacheMb = std::max<qint64>(128, Database::pref(QStringLiteral("setting.imageCacheMB"), 1024).toLongLong());
    ImageCache::init(cache + QStringLiteral("/images"), cacheMb * 1024 * 1024);
    registerPdf();
#ifdef SCANCORE_MAL_CLIENT_ID
    catalogue::setMalClientId(QStringLiteral(SCANCORE_MAL_CLIENT_ID));
#endif
    Manifest::load(data + QStringLiteral("/manifest.json"));
    ext::Manager::setStorage(data + QStringLiteral("/extensions"));
    comics::Registry::rebuild();
    novels::Registry::rebuild();
    anime::Registry::rebuild();
}

// The video player (libmpv's render API) draws with OpenGL, so the
// interface does too: Linux and Android do already; Windows and macOS would
// pick Direct3D and Metal, so OpenGL is asked for where it works. Without
// it, or with another graphics API chosen (QSG_RHI_BACKEND, the software
// renderer), anime from extensions cannot play and the web players remain.
void chooseGraphicsApi() {
    const auto backend = qEnvironmentVariable("QSG_RHI_BACKEND");
    if (QQuickWindow::sceneGraphBackend() == QLatin1String("software") || QGuiApplication::platformName() == QLatin1String("offscreen") ||
        (!backend.isEmpty() && backend != QLatin1String("opengl"))) {
        MpvPlayer::setSceneSupported(false);
        return;
    }
#if defined(Q_OS_MACOS)
    // macOS hands out a legacy 2.1 context by default, which makes mpv drop
    // its better scalers ("GLSL version too old"); ask for 4.1 Core, the most
    // macOS offers.
    {
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        format.setVersion(4, 1);
        format.setProfile(QSurfaceFormat::CoreProfile);
        QSurfaceFormat::setDefaultFormat(format);
    }
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    QOpenGLContext probe;
    if (!probe.create()) {
        qWarning("No OpenGL: videos from anime extensions cannot play here.");
        MpvPlayer::setSceneSupported(false);
        return;
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
}

// Installed, removed, trusted or switched extensions change the sources:
// the registries are rebuilt once a burst of changes settles.
void watchExtensions() {
    static QTimer *rebuild = [] {
        auto *timer = new QTimer(App::instance());
        timer->setSingleShot(true);
        timer->setInterval(150);
        QObject::connect(timer, &QTimer::timeout, App::instance(), [] {
            comics::Registry::rebuild();
            novels::Registry::rebuild();
            anime::Registry::rebuild();
            emit App::instance()->manifestChanged();
            Extensions::instance()->notifyExtensionsChanged();
        });
        return timer;
    }();
    ext::Manager::setChangedCallback([] { QMetaObject::invokeMethod(rebuild, qOverload<>(&QTimer::start)); });
}

void configureHighRefreshRate(QQuickWindow *window) {
    if (!window)
        return;
    auto syncTimingInterval = [window]() {
        QScreen *screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
        if (!screen)
            return;
        const qreal hz = screen->refreshRate();
        const int interval = hz > 61.0 ? std::max(1, int(std::floor(1000.0 / hz))) : 16;
        if (auto *timer = QUnifiedTimer::instance(true))
            timer->setTimingInterval(interval);
    };
    auto rateConn = std::make_shared<QMetaObject::Connection>();
    auto bindScreen = [window, syncTimingInterval, rateConn](QScreen *screen) {
        QObject::disconnect(*rateConn);
        syncTimingInterval();
        if (screen) {
            *rateConn = QObject::connect(screen, &QScreen::refreshRateChanged, window,
                                         [syncTimingInterval](qreal) { syncTimingInterval(); });
        }
    };
    bindScreen(window->screen());
    QObject::connect(window, &QWindow::screenChanged, window, bindScreen);
    // Keep the native CVDisplayLink / ProMotion loop stepped up at 120Hz while
    // any QML animation or Flickable scroll is active.
    QObject::connect(window, &QQuickWindow::frameSwapped, window, [window] {
        if (auto *timer = QUnifiedTimer::instance(false); timer && timer->runningAnimationCount() > 0)
            window->requestUpdate();
    });
}

} // namespace

// Development hook: SCANCORE_PROFILE=1 logs how long start-up takes, step
// by step, up to the first frame on screen.
static QElapsedTimer g_startClock;
static void startStep(const char *step) {
    if (sc::Profiler::enabled())
        qInfo("start-up: %-22s %5lld ms", step, g_startClock.elapsed());
}

int main(int argc, char *argv[]) {
    g_startClock.start();
    // Where Qt hashes through OpenSSL 3 (Arch's own Qt, for one), every hash
    // loads OpenSSL's default provider and unloads it when done; the last
    // unload frees it while another thread may be using it (decoding the
    // TLS certificates for a request), and the app crashed at startup. A
    // hash kept for the whole run (never destroyed, not even at exit, when
    // worker threads may still be hashing) holds one load, so it is never
    // freed.
    [[maybe_unused]] static const auto *providerHold = new QCryptographicHash(QCryptographicHash::Sha256);
#ifdef SCANCORE_HAVE_WEBENGINE
#ifdef Q_OS_LINUX
    // Chromium's GPU compositor shares textures with Qt Quick through GBM;
    // where GBM is unavailable it falls back to Vulkan, and some Mesa drivers
    // then crash on import. Compositing the player in software avoids that
    // and video still decodes on the GPU. WebGL took the same Vulkan path
    // and crashed the app in Mesa (Intel with an NVIDIA laptop GPU), and
    // Cloudflare's browser check draws with WebGL, so no Cloudflare site
    // could be passed there; without Vulkan Chromium uses GL, as it does
    // where GBM works. Users can override the flags.
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS"))
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu-compositing --disable-features=Vulkan");
    // A global MANGOHUD=1 injects MangoHud's Vulkan layer into Chromium's
    // in-process GPU thread, which then aborts with "free(): invalid
    // pointer" shortly after startup. Set DISABLE_MANGOHUD=0 to keep it.
    if (!qEnvironmentVariableIsSet("DISABLE_MANGOHUD"))
        qputenv("DISABLE_MANGOHUD", "1");
#endif
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngineQuick::initialize();
    // The engine's own Chromium version, so sites see one browser (see
    // Http::chromeUserAgent): before anything is requested or the extension
    // host starts with it.
    if (const int major = QByteArray(qWebEngineChromiumVersion()).split('.').value(0).toInt(); major > 0)
        sc::Http::setUserAgent(sc::Http::chromeUserAgent(major));
#endif
#ifdef SCANCORE_HAVE_WEBVIEW
#ifdef Q_OS_WIN
    // Let the player start the next episode without a click, as it does in
    // Qt WebEngine and on Android.
    if (qEnvironmentVariableIsEmpty("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"))
        qputenv("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", "--autoplay-policy=no-user-gesture-required");
#endif
    QtWebView::initialize();
#endif
#ifdef Q_OS_ANDROID
    using Application = QGuiApplication;
#else
    using Application = QApplication;
#endif
    Application app(argc, argv);
    startStep("application");
#if defined(Q_OS_MACOS) || defined(Q_OS_ANDROID)
    app.installEventFilter(new FileOpenFilter(&app));
#endif
    Application::setOrganizationName(QStringLiteral("ENO"));
    Application::setOrganizationDomain(QStringLiteral("eno.app"));
    Application::setApplicationName(QStringLiteral("ENO"));
    Application::setApplicationDisplayName(QStringLiteral("ENO"));
    Application::setApplicationVersion(QStringLiteral(SCANCORE_VERSION));
    // Development hook for testing updates.
    if (qEnvironmentVariableIsSet("SCANCORE_PRETEND_VERSION"))
        Application::setApplicationVersion(qEnvironmentVariable("SCANCORE_PRETEND_VERSION"));
    Application::setDesktopFileName(QStringLiteral("com.mediumdev.scancore"));
    // Crash reports (Settings → About), and the recent log they carry.
    sc::crashlog::install();
    // Development hook: SCANCORE_CRASH_TEST=segv|abort|throw crashes on
    // purpose a few seconds after starting, to check the reports.
    if (const auto test = qEnvironmentVariable("SCANCORE_CRASH_TEST"); !test.isEmpty())
        QTimer::singleShot(4000, [test] {
            qWarning("Crash test: %s", qPrintable(test));
            if (test == QLatin1String("abort"))
                qFatal("Crash test");
            if (test == QLatin1String("throw"))
                throw std::runtime_error("crash test");
            volatile int *nowhere = nullptr;
            *nowhere = 1;
        });
    // On macOS the Dock uses the bundle icon (.icns), so the system applies its own
    // Liquid Glass treatment; a runtime pixmap would override it with an opaque image.
#ifndef Q_OS_MACOS
    Application::setWindowIcon(QIcon(QStringLiteral(":/branding/eno-icon-glass.png")));
#endif
#ifndef Q_OS_ANDROID
    QApplication::setQuitOnLastWindowClosed(false);
#endif

#ifndef Q_OS_ANDROID
    // Files and links given on the command line (a theme, for example).
    QStringList openArguments;
    for (const auto &argument : QCoreApplication::arguments().mid(1))
        if (!argument.startsWith(QLatin1Char('-')))
            openArguments.append(QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile).toString());
    // Launching again while SCANCORE runs (in the tray, say) brings that one
    // back instead of starting a second copy on the same library.
    SingleInstance instance;
    if (qEnvironmentVariableIsEmpty("SCANCORE_START")) {
        if (instance.forwardToRunning(openArguments))
            return 0;
        instance.listen();
    }
#endif

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    loadFonts();
    startStep("fonts");

    try {
        openStorage();
        startStep("storage and sources");
        watchExtensions();
        Extensions::instance()->startHost();
    } catch (const Error &e) {
        qCritical().noquote() << e.message();
        return 1;
    }

    chooseGraphicsApi();

#ifndef Q_OS_ANDROID
    // A see-through window needs an alpha channel from the start.
    if (Customize::savedThemeIsTranslucent()) {
        Customize::setTranslucentWindow(true);
        QQuickWindow::setDefaultAlphaBuffer(true);
    }
#endif

    Account::instance();
    Activity::instance();
    Sync::instance();
    auto *appState = App::instance();
    Customize::applyLanguage();
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("remote"), new RemoteImageProvider);
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("colorwheel"), new ColorWheelProvider);
    engine.addImageProvider(QStringLiteral("asciiglyphs"), new AsciiGlyphProvider);
    // Settings saved by Settings pages that App does not list among its
    // defaults (text and cover size, animation speed, start page and others)
    // are restored as they were stored.
    for (const auto &row : Database::query(QStringLiteral("SELECT key FROM prefs WHERE key LIKE 'setting.%'"))) {
        const auto key = row.toMap().value(QStringLiteral("key")).toString().mid(8);
        if (!key.isEmpty() && !appState->settings().contains(key))
            appState->setSetting(key, Database::pref(QStringLiteral("setting.") + key));
    }
#ifndef Q_OS_ANDROID
    QSystemTrayIcon tray(QApplication::windowIcon());
    QMenu trayMenu;
    trayMenu.addAction(QStringLiteral("Open ENO"), appState, &App::activateRequested);
    trayMenu.addSeparator();
    trayMenu.addAction(QStringLiteral("Quit"), &app, &QApplication::quit);
    tray.setContextMenu(&trayMenu);
    tray.setToolTip(QStringLiteral("ENO"));
    QObject::connect(&tray, &QSystemTrayIcon::activated, appState, [appState](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit appState->activateRequested();
    });
    if (QSystemTrayIcon::isSystemTrayAvailable())
        tray.show();
    appState->setTrayIcon(&tray);
#endif

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    startStep("before the interface");
    engine.loadFromModule("ScanCore", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    startStep("interface created");
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
        configureHighRefreshRate(window);
        QObject::connect(window, &QQuickWindow::frameSwapped, window, [] { startStep("first frame"); },
                         Qt::SingleShotConnection);
        Profiler::instance()->attach(window);
        QObject::connect(appState, &App::activateRequested, window, [window] {
            if (window->visibility() == QWindow::Hidden || window->visibility() == QWindow::Minimized)
                window->setVisibility(QWindow::Windowed);
            window->show();
            window->raise();
            window->requestActivate();
        });
    }
    // The window decides whether closing hides to the tray; once it is gone
    // there is nothing left to show.
    QObject::connect(engine.rootObjects().first(), &QObject::destroyed, &app, &Application::quit);

#ifndef Q_OS_ANDROID
    QObject::connect(&instance, &SingleInstance::activated, appState, [appState](const QStringList &arguments) {
        emit appState->activateRequested();
        for (const auto &argument : arguments)
            emit appState->openRequested(argument);
    });
    QTimer::singleShot(0, appState, [appState, openArguments] {
        for (const auto &argument : openArguments)
            emit appState->openRequested(argument);
    });
#endif

    Presence::instance();
    appState->startBackgroundWork();
    Updater::instance()->startAutomatic();

    QObject::connect(&app, &Application::aboutToQuit, [] {
        Extensions::instance()->stopHost();
        CancelScope::cancelAll();
        CookieJar::shared().save();
        try {
            Database::exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
        } catch (...) {}
    });
    return app.exec();
}
