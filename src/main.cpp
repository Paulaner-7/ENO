#include "app/Account.h"
#include "app/Activity.h"
#include "app/App.h"
#include "app/Customize.h"
#include "app/Images.h"
#include "app/Presence.h"
#include "app/SingleInstance.h"
#include "app/Sync.h"
#include "app/Updater.h"
#include "app/WebBridge.h"
#include "core/Cookies.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/ImageCache.h"
#include "sources/Manifest.h"
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
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#ifdef SCANCORE_HAVE_WEBENGINE
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

#ifdef Q_OS_MACOS
// Finder opens files (a .sct theme, a comic) with an event, not arguments.
class FileOpenFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::FileOpen)
            return QObject::eventFilter(watched, event);
        const auto target = static_cast<QFileOpenEvent *>(event)->file();
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
    Manifest::load(data + QStringLiteral("/manifest.json"));
    comics::Registry::rebuild();
    novels::Registry::rebuild();
}

} // namespace

int main(int argc, char *argv[]) {
#ifdef SCANCORE_HAVE_WEBENGINE
#ifdef Q_OS_LINUX
    // Chromium's GPU compositor shares textures with Qt Quick through GBM;
    // where GBM is unavailable it falls back to Vulkan, and some Mesa drivers
    // then crash on import. Compositing the player in software avoids that
    // and video still decodes on the GPU. Users can override the flags.
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS"))
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu-compositing");
    // A global MANGOHUD=1 injects MangoHud's Vulkan layer into Chromium's
    // in-process GPU thread, which then aborts with "free(): invalid
    // pointer" shortly after startup. Set DISABLE_MANGOHUD=0 to keep it.
    if (!qEnvironmentVariableIsSet("DISABLE_MANGOHUD"))
        qputenv("DISABLE_MANGOHUD", "1");
#endif
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngineQuick::initialize();
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
#ifdef Q_OS_MACOS
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
    Application::setWindowIcon(QIcon(QStringLiteral(":/branding/scancore.svg")));
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
    if (instance.forwardToRunning(openArguments))
        return 0;
    instance.listen();
#endif

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    loadFonts();

    try {
        openStorage();
    } catch (const Error &e) {
        qCritical().noquote() << e.message();
        return 1;
    }

#ifndef Q_OS_ANDROID
    // A see-through window needs an alpha channel from the start.
    if (Customize::savedThemeIsTranslucent()) {
        Customize::setTranslucentWindow(true);
        QQuickWindow::setDefaultAlphaBuffer(true);
    }
#endif

    installWebBridge();
    Account::instance();
    Activity::instance();
    Sync::instance();
    auto *appState = App::instance();
    Customize::applyLanguage();
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("remote"), new RemoteImageProvider);
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("colorwheel"), new ColorWheelProvider);
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
    engine.loadFromModule("ScanCore", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
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
        CancelScope::cancelAll();
        CookieJar::shared().save();
    });
    return app.exec();
}
