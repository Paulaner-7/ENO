#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

namespace sc {

// The notification area: new chapters, announcements from scancore.onl,
// extension and app updates. Kept in the local database (newest 300);
// each can also show as a system notification (tray message on desktop,
// Android's notifications), when SCANCORE is not on screen or the
// announcement is important.
class Notifications : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int unread READ unread NOTIFY changed)
    // Bumped on every change, for bindings that list them.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    static Notifications *instance();
    static Notifications *create(QQmlEngine *, QJSEngine *) { return instance(); }

    int unread() const { return m_unread; }
    int revision() const { return m_revision; }

    // [{id, kind, title, body, action, createdAt, read}], newest first.
    // action: {seriesId} | {url} | {page}.
    Q_INVOKABLE sc::Task *list(int limit = 100);
    Q_INVOKABLE void markRead(qint64 id);
    Q_INVOKABLE void markAllRead();
    Q_INVOKABLE void remove(qint64 id);
    Q_INVOKABLE void clear();
    // One from QML (and development hooks).
    Q_INVOKABLE void add(const QString &kind, const QString &title, const QString &body, bool system = false);

    struct Item {
        // chapters, announcement, extensions, app
        QString kind;
        // The same key again replaces the earlier one (an announcement
        // edited, a series with more new chapters).
        QString key;
        QString title;
        QString body;
        QVariantMap action;
        // Show it in the system's notifications too (if allowed).
        bool system = false;
        // Even while SCANCORE is on screen.
        bool urgent = false;
    };
    // From any thread.
    static void post(const Item &item);
    // Drops the one with this key (an announcement taken down).
    static void withdraw(const QString &key);

    // Asks Android (13 and later) to allow notifications, once.
    Q_INVOKABLE void askPermission();

signals:
    void changed();
    // One just arrived (for an in-app toast).
    void arrived(const QString &title, const QString &body);

private:
    explicit Notifications(QObject *parent = nullptr);
    void refreshUnread();
    void bump();

    int m_unread = 0;
    int m_revision = 0;
};

} // namespace sc
