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
    // Development hook: SCANCORE_START='{"open": {...}, "autoResume": true}'
    // opens a page at startup for smoke tests and screenshots.
    Q_PROPERTY(QVariantMap startAction READ startAction CONSTANT)

public:
    static App *instance();
    static App *create(QQmlEngine *, QJSEngine *) { return instance(); }

    QString version() const;
    QString platform() const;
    QVariantMap settings() const { return m_settings; }
    QString dataFolder() const;
    int manifestVersion() const;
    bool webEngine() const;
    QVariantMap startAction() const;

    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE QString image(const QString &url, const QString &referer = QString()) const;
    Q_INVOKABLE QString icon(const QString &name, const QString &color) const;
    Q_INVOKABLE void openUrl(const QString &url) const;
    // Android: send the app to the background, as Back on the first screen
    // does. Does nothing elsewhere.
    Q_INVOKABLE void moveToBackground() const;
    Q_INVOKABLE void openFolder(const QString &path) const;
    Q_INVOKABLE QVariantList googleDriveAccounts() const;
    Q_INVOKABLE QString googleDrivePath() const;
    // A file dialog's address as a path to show or store.
    Q_INVOKABLE QString localPath(const QUrl &url) const { return url.isLocalFile() ? url.toLocalFile() : url.toString(); }
    Q_INVOKABLE void copy(const QString &text) const;
    Q_INVOKABLE void notify(const QString &title, const QString &body);
    Q_INVOKABLE QString relativeTime(const QVariant &when) const;
    Q_INVOKABLE QString formatBytes(qint64 bytes) const;
    // Development hook used by SCANCORE_START screenshots.
    Q_INVOKABLE bool saveWindowImage(QObject *window, const QString &path) const;

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

private:
    // Private so QML uses create(): Qt builds its own instance of a
    // singleton it can default-construct, and C++ and QML would then
    // work on different objects.
    explicit App(QObject *parent = nullptr);

    void scheduleLibraryCheck();

    QVariantMap m_settings;
    QTimer m_libraryTimer;
    QSystemTrayIcon *m_tray = nullptr;
};

} // namespace sc
