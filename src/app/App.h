#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

class QSystemTrayIcon;

namespace sc {

// App-wide state for QML: settings, image/icon URLs, update checks,
// notifications, backups and maintenance.
class App : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString platform READ platform CONSTANT)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QString dataFolder READ dataFolder CONSTANT)
    Q_PROPERTY(int manifestVersion READ manifestVersion NOTIFY manifestChanged)
    Q_PROPERTY(bool webEngine READ webEngine CONSTANT)
    // Android: battery optimisation is on for SCANCORE, so the system stops
    // it soon after it leaves the screen. Checked again on every return.
    Q_PROPERTY(bool batteryRestricted READ batteryRestricted NOTIFY batteryChanged)
    // Development hook: SCANCORE_START='{"open": {...}, "autoResume": true}'
    // opens a page at startup for smoke tests and screenshots.
    Q_PROPERTY(QVariantMap startAction READ startAction CONSTANT)

public:
    static App *instance();
    static App *create(QQmlEngine *, QJSEngine *) { return instance(); }

    QString version() const;
    QString platform() const;
    QVariantMap settings() const { return m_settings; }
    // Incognito mode (setting "incognito"): no progress, history, activity,
    // tracker updates or Discord status.
    bool incognito() const { return m_settings.value(QStringLiteral("incognito")).toBool(); }
    QString dataFolder() const;
    int manifestVersion() const;
    bool webEngine() const;
    bool batteryRestricted() const { return m_batteryRestricted; }
    QVariantMap startAction() const;

    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE QString image(const QString &url, const QString &referer = QString()) const;
    Q_INVOKABLE QString icon(const QString &name, const QString &color) const;
    Q_INVOKABLE void openUrl(const QString &url) const;
    // Before an embedded browser view loads anything (see WebBridge.h).
    Q_INVOKABLE void prepareWeb() const;
    // Android: send the app to the background, as Back on the first screen
    // does. Does nothing elsewhere.
    Q_INVOKABLE void moveToBackground() const;
    // Android: a site's browser check in the system web view (see
    // Platform.h); siteChecked(url) follows. False where there is none.
    Q_INVOKABLE bool openSiteCheck(const QString &url, const QString &title, const QString &doneLabel);
    // SCANCORE's own site or the sync server: no browser checks there.
    Q_INVOKABLE bool protectedSite(const QString &url) const;
    // The user agent to pass a site's check with: the extension's own when
    // one met the check, else SCANCORE's (Cloudflare ties the clearance to it).
    Q_INVOKABLE QString siteAgent(const QString &url) const;
    Q_INVOKABLE QString defaultAgent() const;
    // The reader passed a check in the window (desktop): extensions get the
    // site's cookies, then siteChecked(url) tells pages to try again.
    Q_INVOKABLE void finishSiteCheck(const QString &url);
    // The check window closed without the check being passed.
    Q_INVOKABLE void cancelSiteCheck(const QString &url);
    Q_INVOKABLE void askBatteryUnrestricted() const;
    // Android: the anime player's web view, and turning the phone for its
    // full screen (see Platform.h). Nothing elsewhere.
    Q_INVOKABLE void prepareWebPlayer() const;
    Q_INVOKABLE void setOrientation(const QString &mode) const;
    Q_INVOKABLE void setBarIcons(bool darkScreen) const;
    // Android: volume keys come as volumeKey() instead of changing the
    // volume while on (the comic reader turns pages with them).
    Q_INVOKABLE void setVolumeKeys(bool on) const;
    // SCANCORE_PROFILE: the page now on screen, for the profiler's log.
    Q_PROPERTY(bool profiling READ profiling CONSTANT)
    bool profiling() const;
    Q_INVOKABLE void profilePlace(const QString &place) const;
    Q_INVOKABLE void openFolder(const QString &path) const;
    Q_INVOKABLE QVariantList googleDriveAccounts() const;
    Q_INVOKABLE QString googleDrivePath() const;
    // A file dialog's address as a path to show or store.
    Q_INVOKABLE QString localPath(const QUrl &url) const { return url.isLocalFile() ? url.toLocalFile() : url.toString(); }
    Q_INVOKABLE void copy(const QString &text) const;
    // Crash reports and diagnostics (see CrashLog): [{file, when, summary,
    // seen}], one report's text, the unseen ones, and sharing a text (the
    // system's share sheet on Android; false elsewhere).
    Q_INVOKABLE QVariantList crashReports() const;
    Q_INVOKABLE QString crashReport(const QString &file) const;
    Q_INVOKABLE QStringList unseenCrashReports() const;
    Q_INVOKABLE void markCrashReportSeen(const QString &file);
    Q_INVOKABLE void removeCrashReport(const QString &file);
    Q_INVOKABLE QString diagnostics() const;
    Q_INVOKABLE bool shareText(const QString &text, const QString &subject) const;
    // A new GitHub issue for a report, its first part filled in.
    Q_INVOKABLE QString reportIssueUrl(const QString &text) const;
    Q_INVOKABLE void notify(const QString &title, const QString &body);
    Q_INVOKABLE QString relativeTime(const QVariant &when) const;
    Q_INVOKABLE QString formatBytes(qint64 bytes) const;
    // Makes a binding depend on values it does not use, as in
    // `{ App.dependOn(Library.revision); return Library.isSaved(id) }`. The
    // QML compiler drops a bare `Library.revision;` statement, and with it
    // the dependency, so the binding would never update.
    Q_INVOKABLE void dependOn(const QVariant &, const QVariant & = {}, const QVariant & = {}) const {}
    // Development hook used by SCANCORE_START screenshots.
    Q_INVOKABLE bool saveWindowImage(QObject *window, const QString &path) const;
    // Moves `item` right after (or before) its sibling in stacking order,
    // which is also the order Column and Row lay children out in (page
    // sections, PageArranger.qml).
    // Languages of the built-in sources: [{value, label, count, on}], the
    // ones that are on first (Settings -> Sources -> Source languages).
    Q_INVOKABLE QVariantList builtinLanguages() const;
    // The open-source software in the app ({name, use, licence, file}),
    // then each video-player library's own notices when the build has them.
    Q_INVOKABLE QVariantList licences() const;
    // A licence text listed by licences().
    Q_INVOKABLE QString licenceText(const QString &file) const;
    Q_INVOKABLE void stackAfter(QObject *item, QObject *sibling) const;
    Q_INVOKABLE void stackBefore(QObject *item, QObject *sibling) const;

    // {available, version, url, notes}
    Q_INVOKABLE sc::Task *checkForUpdate();
    Q_INVOKABLE sc::Task *refreshSources();
    Q_INVOKABLE sc::Task *cacheSize();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE sc::Task *exportBackup(const QUrl &file);
    Q_INVOKABLE sc::Task *importBackup(const QUrl &file);

    void startBackgroundWork();
    void setTrayIcon(QSystemTrayIcon *tray) { m_tray = tray; }

signals:
    void settingsChanged();
    void manifestChanged();
    void updateAvailable(const QString &version, const QString &url);
    void activateRequested();
    // A file or link to open, given on the command line or by a later launch.
    void openRequested(const QString &target);
    void siteChecked(const QString &url);
    // The embedded browser received a site's Cloudflare clearance.
    void siteCleared(const QString &domain);
    // An extension met a check the hidden browser could not pass: open the
    // site's check now (Main.qml); its request waits for it.
    void browserCheckWanted(const QString &url);
    void batteryChanged();
    // Android: the player's web view is ready to load (see prepareWebPlayer).
    void webPlayerPrepared();
    void volumeKey(bool down);

private:
    // Extension requests waiting on the reader's browser check, by site.
    void askReader(const QString &url, std::function<void(const QJsonValue &)> answer);
    void answerReader(const QString &url, bool passed);
    QHash<QString, QList<std::function<void(const QJsonValue &)>>> m_readerChecks;
    // Private so QML uses create(): Qt builds its own instance of a
    // singleton it can default-construct, and C++ and QML would then
    // work on different objects.
    explicit App(QObject *parent = nullptr);

    void scheduleLibraryCheck();

    QVariantMap m_settings;
    QTimer m_libraryTimer;
    bool m_batteryRestricted = false;
    QSystemTrayIcon *m_tray = nullptr;
};

} // namespace sc
