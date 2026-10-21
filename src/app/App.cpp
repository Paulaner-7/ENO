#include "App.h"

#include "Images.h"
#include "Library.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/ImageCache.h"
#include "sources/Manifest.h"
#include "sources/comics/Registry.h"
#include "sources/novels/Registry.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QQuickWindow>
#include <QStandardPaths>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#else
#include <QSystemTrayIcon>
#endif
#include <QVersionNumber>

namespace sc {

namespace {

const QVariantMap kDefaults{
    {"theme", "dark"},
    {"accent", "mono"},
    {"reducedMotion", false},
    {"readerMode", "strip"},
    {"readerWidth", 820},
    {"readerGap", 0},
    {"readerDirection", "ltr"},
    {"readerSpread", "single"},
    {"readerCrop", false},
    {"readerProgressBar", true},
    {"novelFont", "Literata"},
    {"novelSize", 19},
    {"novelLineHeight", 1.5},
    {"novelWidth", 720},
    {"novelTheme", "app"},
    {"novelJustify", false},
    {"autoNext", true},
    {"autoSkip", false},
    {"animeSubLang", "it"},
    {"animeSubQuality", "high"},
    {"animeVideoQuality", "max"},
    {"libraryCheckHours", 2},
    {"notifyUpdates", true},
    {"showNsfwSources", false},
    {"imageCacheMB", 1024},
    {"closeToTray", false},
    {"checkAppUpdates", true},
    {"sidebarCollapsed", false},
    {"comicSource", "all"},
    {"comickCatalog", true},
    // Content language "All sources" searches; empty for every language.
    {"sourceLanguages", ""},
    {"onboarded", false},
    {"autoUpdate", true},
    {"novelSource", "all"},
    {"downloadsPath", ""},
};

} // namespace

App::App(QObject *parent) : QObject(parent) {
    m_settings = kDefaults;
    for (auto it = kDefaults.begin(); it != kDefaults.end(); ++it)
        m_settings.insert(it.key(), Database::pref(QStringLiteral("setting.") + it.key(), it.value()));
    m_libraryTimer.setSingleShot(false);
    connect(&m_libraryTimer, &QTimer::timeout, this, [] { Library::instance()->checkUpdates(); });
}

App *App::instance() {
    static auto *app = [] {
        auto *created = new App;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return app;
}

QString App::version() const { return QCoreApplication::applicationVersion(); }

QString App::platform() const {
#if defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

QString App::dataFolder() const { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }
int App::manifestVersion() const { return Manifest::version(); }

bool App::webEngine() const {
#if defined(SCANCORE_HAVE_WEBENGINE) || defined(SCANCORE_HAVE_WEBVIEW)
    return true;
#else
    return false;
#endif
}

QVariantMap App::startAction() const {
    return QJsonDocument::fromJson(qgetenv("SCANCORE_START")).object().toVariantMap();
}

void App::setSetting(const QString &key, const QVariant &value) {
    if (m_settings.value(key) == value)
        return;
    m_settings.insert(key, value);
    Database::setPref(QStringLiteral("setting.") + key, value);
    if (key == QLatin1String("libraryCheckHours"))
        scheduleLibraryCheck();
    if (key == QLatin1String("imageCacheMB")) {
        ImageCache::setBudget(std::max<qint64>(128, value.toLongLong()) * 1024 * 1024);
        // A smaller limit takes effect now, not at the next start.
        runInBackground([] { ImageCache::trim(); });
    }
    emit settingsChanged();
}

QString App::image(const QString &url, const QString &referer) const {
    if (url.isEmpty())
        return {};
    if (url.startsWith(QLatin1String("file:")) || url.startsWith(QLatin1String("qrc:")) ||
        url.startsWith(QLatin1String("image:")))
        return url;
    return RemoteImageProvider::encode(url, referer);
}

QString App::icon(const QString &name, const QString &color) const {
    return QStringLiteral("image://icon/%1?color=%2").arg(name, QString::fromUtf8(QUrl::toPercentEncoding(color)));
}

void App::openUrl(const QString &url) const {
    const QUrl parsed(url);
    if (parsed.scheme() == QLatin1String("https") || parsed.scheme() == QLatin1String("http"))
        QDesktopServices::openUrl(parsed);
}

void App::moveToBackground() const {
#ifdef Q_OS_ANDROID
    // What Back on an app's first screen does on Android 12 and later. Runs on
    // the Android UI thread without waiting for it, so it cannot deadlock with
    // Qt's thread.
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
            activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", jboolean(true));
    });
#endif
}

void App::openFolder(const QString &path) const {
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QVariantList App::googleDriveAccounts() const {
    QVariantList list;
#if defined(Q_OS_MACOS)
    const QString home = QDir::homePath();
    const QDir cloudStorage(home + QStringLiteral("/Library/CloudStorage"));
    if (cloudStorage.exists()) {
        const auto entries = cloudStorage.entryList({QStringLiteral("GoogleDrive-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &entry : entries) {
            const QString base = cloudStorage.filePath(entry);
            QString targetPath;
            for (const auto &sub : {QStringLiteral("Il mio Drive"), QStringLiteral("My Drive")}) {
                const QString candidate = base + QLatin1Char('/') + sub;
                if (QDir(candidate).exists()) {
                    targetPath = candidate;
                    break;
                }
            }
            if (targetPath.isEmpty() && QDir(base).exists()) {
                if (!QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty())
                    targetPath = base;
            }
            if (!targetPath.isEmpty()) {
                QString email = entry.mid(12);
                if (email.contains(QLatin1Char(' ')))
                    email = email.section(QLatin1Char(' '), 0, 0);
                list.append(QVariantMap{
                    {QStringLiteral("email"), email},
                    {QStringLiteral("path"), targetPath}
                });
            }
        }
    }
#elif defined(Q_OS_WIN)
    for (const auto &drive : {QStringLiteral("G:/My Drive"), QStringLiteral("G:/Il mio Drive"), QStringLiteral("G:/")}) {
        if (QDir(drive).exists()) {
            list.append(QVariantMap{
                {QStringLiteral("email"), QStringLiteral("Google Drive")},
                {QStringLiteral("path"), drive}
            });
            break;
        }
    }
#endif
    return list;
}

QString App::googleDrivePath() const {
    const auto accounts = googleDriveAccounts();
    if (!accounts.isEmpty())
        return accounts.first().toMap().value(QStringLiteral("path")).toString();
    return QString();
}

void App::copy(const QString &text) const { QGuiApplication::clipboard()->setText(text); }

void App::notify(const QString &title, const QString &body) {
#ifndef Q_OS_ANDROID
    if (m_tray && QSystemTrayIcon::supportsMessages())
        m_tray->showMessage(title, body, QSystemTrayIcon::Information, 6000);
#else
    Q_UNUSED(title)
    Q_UNUSED(body)
#endif
}

QString App::relativeTime(const QVariant &when) const {
    QDateTime time;
    if (when.typeId() == QMetaType::QString)
        time = QDateTime::fromString(when.toString(), Qt::ISODate);
    else if (when.toLongLong() > 0)
        time = QDateTime::fromSecsSinceEpoch(when.toLongLong());
    if (!time.isValid())
        return {};
    const auto secs = time.secsTo(QDateTime::currentDateTime());
    if (secs < 0) {
        const auto ahead = -secs;
        if (ahead < 3600)
            return QStringLiteral("in %1m").arg(std::max<qint64>(1, ahead / 60));
        if (ahead < 86400)
            return QStringLiteral("in %1h").arg(ahead / 3600);
        return QStringLiteral("in %1d").arg(ahead / 86400);
    }
    if (secs < 60)
        return QStringLiteral("just now");
    if (secs < 3600)
        return QStringLiteral("%1m ago").arg(secs / 60);
    if (secs < 86400)
        return QStringLiteral("%1h ago").arg(secs / 3600);
    if (secs < 7 * 86400)
        return QStringLiteral("%1d ago").arg(secs / 86400);
    return QLocale().toString(time.date(), QLocale::ShortFormat);
}

QString App::formatBytes(qint64 bytes) const { return QLocale().formattedDataSize(bytes); }

bool App::saveWindowImage(QObject *window, const QString &path) const {
    auto *quick = qobject_cast<QQuickWindow *>(window);
    return quick && quick->grabWindow().save(path);
}

Task *App::checkForUpdate() {
    return Task::run([this]() -> QVariant {
        const auto doc = Http::getJson(QUrl(QStringLiteral("https://api.github.com/repos/medium-gh/scancore/releases/latest")),
                                       {{"Accept", "application/vnd.github+json"}});
        const auto release = doc.object();
        auto tag = release.value("tag_name").toString();
        if (tag.startsWith(QLatin1Char('v')))
            tag.remove(0, 1);
        const auto latest = QVersionNumber::fromString(tag);
        const auto current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
        const bool available = !latest.isNull() && latest > current;
        const auto url = release.value("html_url").toString();
        if (available)
            QMetaObject::invokeMethod(this, [this, tag, url] { emit updateAvailable(tag, url); });
        return QVariantMap{{"available", available},
                           {"version", tag},
                           {"url", url},
                           {"notes", release.value("body").toString()}};
    });
}

Task *App::refreshSources() {
    return Task::run([this]() -> QVariant {
        const bool changed = Manifest::refresh();
        if (changed) {
            comics::Registry::rebuild();
            novels::Registry::rebuild();
            QMetaObject::invokeMethod(this, [this] { emit manifestChanged(); });
        }
        return QVariantMap{{"changed", changed}, {"version", Manifest::version()}};
    });
}

Task *App::cacheSize() {
    return Task::run([]() -> QVariant { return ImageCache::sizeOnDisk(); });
}

void App::clearCache() {
    runInBackground([] {
        ImageCache::clear();
        Database::exec(QStringLiteral("DELETE FROM cache"));
    });
}

Task *App::exportBackup(const QUrl &file) {
    return Task::run([file]() -> QVariant {
        const auto path = file.toLocalFile();
        if (path.isEmpty())
            throw Error(QStringLiteral("Choose a file to save the backup to."));
        QFile::remove(path);
        // VACUUM INTO writes a consistent copy while the app keeps running.
        auto escaped = path;
        escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
        Database::exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped));
        return path;
    });
}

Task *App::importBackup(const QUrl &file) {
    return Task::run([file]() -> QVariant {
        const auto path = file.toLocalFile();
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly) || in.read(16) != QByteArray("SQLite format 3\0", 16))
            throw Error(QStringLiteral("That file is not a SCANCORE backup."));
        in.close();
        const auto pending = Database::path() + QStringLiteral(".restore");
        QFile::remove(pending);
        if (!QFile::copy(path, pending))
            throw Error(QStringLiteral("Could not copy the backup."));
        return QStringLiteral("Restart SCANCORE to finish restoring the backup.");
    });
}

void App::scheduleLibraryCheck() {
    const int hours = m_settings.value(QStringLiteral("libraryCheckHours")).toInt();
    if (hours <= 0) {
        m_libraryTimer.stop();
        return;
    }
    m_libraryTimer.start(hours * 3600 * 1000);
}

void App::startBackgroundWork() {
    connect(Library::instance(), &Library::updatesFound, this, [this](int count, const QStringList &titles) {
        if (!m_settings.value(QStringLiteral("notifyUpdates")).toBool())
            return;
        const auto body = titles.size() == 1
                              ? QStringLiteral("%1 has %2 new chapter%3.").arg(titles.first()).arg(count).arg(count == 1 ? "" : "s")
                              : QStringLiteral("%1 new chapters in %2 series.").arg(count).arg(titles.size());
        notify(QStringLiteral("New chapters"), body);
    });
    scheduleLibraryCheck();
    QTimer::singleShot(4000, this, [this] {
        refreshSources();
        if (m_settings.value(QStringLiteral("libraryCheckHours")).toInt() > 0)
            Library::instance()->checkUpdates();
        runInBackground([] {
            ImageCache::trim();
            Database::cachePrune();
        });
    });
}

} // namespace sc
