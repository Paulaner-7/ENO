#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

namespace sc {

// Watch parties (scancore.onl): the host opens one and shares its code,
// everyone who joins watches the same anime and episode from their own
// source, and the host's player (playing or paused, where it is) steers
// the others' players. Chat comes with it. The site keeps nothing: the
// party ends when the host leaves.
//
// WatchPage reports the host's player here (report) and applies what
// arrives (stateReceived); Main opens the anime for someone who joined
// from elsewhere.
class Party : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Signed in to a site that has parties (an older site answers 404).
    Q_PROPERTY(bool available READ available NOTIFY changed)
    // In a party (also while its stream reconnects).
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(bool hosting READ hosting NOTIFY changed)
    // [{userId, name, avatar, host}]
    Q_PROPERTY(QVariantList members READ members NOTIFY changed)
    // [{name, avatar, text, at, mine}], oldest first.
    Q_PROPERTY(QVariantList chat READ chat NOTIFY chatChanged)
    Q_PROPERTY(int unread READ unread NOTIFY chatChanged)
    // The host's last player state: {anime: {anilistId, title, cover},
    // episode, playing, position, received (ms, this device's clock)}.
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateReceived)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // The anime the open player shows (WatchPage sets it), so joining from
    // elsewhere opens it only when it is not open already.
    Q_PROPERTY(int watching READ watching WRITE setWatching NOTIFY changed)

public:
    static Party *instance();
    static Party *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool available() const;
    bool active() const { return !m_code.isEmpty(); }
    bool connected() const { return m_connected; }
    bool busy() const { return m_busy; }
    QString code() const { return m_code; }
    bool hosting() const { return !m_you.isEmpty() && m_you == m_host; }
    QVariantList members() const { return m_members; }
    QVariantList chat() const { return m_chat; }
    int unread() const { return m_unread; }
    QVariantMap state() const { return m_state; }
    QString error() const { return m_error; }
    int watching() const { return m_watching; }
    void setWatching(int anilistId);

    // Opens a party for `anime` ({anilistId, title, cover}) and joins it.
    Q_INVOKABLE void start(const QVariantMap &anime);
    Q_INVOKABLE void join(const QString &code);
    // The host's leaving ends the party for everyone.
    Q_INVOKABLE void leave();
    Q_INVOKABLE void say(const QString &text);
    Q_INVOKABLE void markRead();
    // The host's player, on every tick: sent when something changed (the
    // episode, play or pause, a seek) and every few seconds besides.
    Q_INVOKABLE void report(const QVariantMap &anime, double episode, bool playing, double position);
    // Where the host is now: the last position plus the time since, while
    // playing.
    Q_INVOKABLE double hostPosition() const;

signals:
    void changed();
    void chatChanged();
    void stateReceived();
    // A line from someone else, for a toast while the chat is closed.
    void messageReceived(const QString &name, const QString &text);
    // The host ended it (or it ran out); not when leaving oneself.
    void ended();

private:
    explicit Party(QObject *parent = nullptr);
    void connectStream();
    void readEvents();
    void handle(const QByteArray &event, const QByteArray &data);
    void reset();
    void sendState();
    void post(const QJsonObject &body);
    void setError(const QString &error);

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    QTimer m_retry;
    int m_failures = 0;
    bool m_connected = false;
    bool m_busy = false;
    bool m_unsupported = false;
    QString m_code;
    QString m_you;
    QString m_host;
    QVariantList m_members;
    QVariantList m_chat;
    int m_unread = 0;
    QVariantMap m_state;
    QElapsedTimer m_stateClock;
    QString m_error;
    int m_watching = 0;
    // The host's side: what went out last and what waits to go.
    QVariantMap m_sent;
    QElapsedTimer m_sinceSent;
    QVariantMap m_pending;
    QTimer m_send;
};

} // namespace sc
