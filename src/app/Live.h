#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

namespace sc {

// Live sync between the account's open apps (scancore.onl, or a scancore-web
// server 1.1 or later): while signed in, a stream of server-sent events
// stays open, and where this app is in a chapter or episode goes to the
// others within seconds instead of at the next snapshot sync. It also sends
// "continue on that device": the other app takes the position and offers to
// open it. Nothing is stored by the server; snapshots still carry it all.
//
// On Android the stream closes while the app is in the background.
class Live : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    // The account's other open apps: [{id, name, platform}].
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)

public:
    static Live *instance();
    static Live *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool connected() const { return m_connected; }
    QVariantList devices() const;

    // Opens the stream when signed in (and closes it otherwise); called at
    // start-up and whenever the account or sync settings change.
    void update();
    // Sends the series' latest position to another open app, which offers to
    // continue there. False when there is nothing to send.
    Q_INVOKABLE bool handoff(const QString &deviceId, const QString &seriesId);

    // True while live progress is written here, so writing it does not
    // schedule a sync.
    static bool applying();

signals:
    void changed();
    // Another app sent its position here: `entry` is the series (as Home's
    // history has it) with unitTitle, and `from` the device.
    void handoffReceived(const QVariantMap &entry, const QVariantMap &from);

private:
    explicit Live(QObject *parent = nullptr);
    bool wanted() const;
    void connectStream();
    void disconnectStream();
    void retryLater();
    void readEvents();
    void handle(const QByteArray &event, const QByteArray &data);
    void setOnline(const QVariantMap &device, bool online);
    void queueProgress(const QString &seriesId, const QString &unitKey);
    void sendPending();
    void post(const QByteArray &type, const QJsonObject &data, const QString &to = {});

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    QTimer m_retry;
    int m_backoffMs = 5000;
    bool m_connected = false;
    bool m_unsupported = false;
    // Other apps, and when each was last heard leaving (kept for a short
    // while: a stream that ends reconnects at once).
    QHash<QString, QVariantMap> m_devices;
    QHash<QString, qint64> m_leaving;
    QTimer m_sweep;
    // The position waiting to go out, sent at most every few seconds.
    QString m_pendingSeries;
    QString m_pendingUnit;
    QTimer m_send;
    QElapsedTimer m_sinceSent;
};

} // namespace sc
