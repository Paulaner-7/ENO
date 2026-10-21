#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QTimer>

namespace sc {

// Offline copies of comic chapters (images) and novel chapters (text), kept
// in the app data folder and processed two at a time.
class Downloads : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(int pending READ pending NOTIFY changed)
    Q_PROPERTY(QString folder READ folder NOTIFY changed)
    Q_PROPERTY(bool hasCustomFolder READ hasCustomFolder NOTIFY changed)

public:
    static Downloads *instance();
    static Downloads *create(QQmlEngine *, QJSEngine *) { return instance(); }

    int revision() const { return m_revision; }
    int pending() const;

    Q_INVOKABLE void enqueue(const QVariantMap &series, const QVariantList &units);
    Q_INVOKABLE sc::Task *items();
    Q_INVOKABLE QVariantMap states(const QString &seriesId) const;
    Q_INVOKABLE void remove(int id);
    Q_INVOKABLE void removeUnit(const QString &seriesId, const QString &unitKey);
    Q_INVOKABLE void retry(int id);
    Q_INVOKABLE void retryFailed();
    Q_INVOKABLE void clearFinished();
    Q_INVOKABLE QString folder() const;
    Q_INVOKABLE bool hasCustomFolder() const;
    Q_INVOKABLE void setFolder(const QString &folder);
    Q_INVOKABLE void resetFolder();
    Q_INVOKABLE QString sizeOnDisk() const;

    static QVariantList localPanels(const QString &seriesId, const QString &unitKey);
    static QVariantMap localNovelChapter(const QString &seriesId, const QString &unitKey);
    static QString unitFolder(const QString &seriesId, const QString &unitKey);

signals:
    void changed();
    void finished(const QString &title);

private:
    // Private so QML uses create(): Qt builds its own instance of a
    // singleton it can default-construct, and C++ and QML would then
    // work on different objects.
    explicit Downloads(QObject *parent = nullptr);

    void pump();
    void runJob(const QVariantMap &row);
    void notify();

    int m_revision = 0;
    QSet<int> m_running;
    QTimer m_throttle;
};

} // namespace sc
