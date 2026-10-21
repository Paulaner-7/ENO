#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;

namespace sc {

// Announcements from the website (scancore.onl unless the user set their
// own): a server-sent event stream kept open while online, which delivers
// new posts and removals at once. After a drop it reconnects (backing off
// up to five minutes) with the last event id, so the server sends what was
// missed. Each announcement becomes a notification; important ones also a
// system notification. Settings → Library can switch it off.
class Announcements : public QObject {
    Q_OBJECT

public:
    static Announcements *instance();

    void start();
    void stop();

private:
    explicit Announcements(QObject *parent = nullptr);
    void connectStream();
    void retryLater();
    void readEvents();
    void handle(const QByteArray &event, const QByteArray &data, const QByteArray &id);

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QTimer m_retry;
    QByteArray m_buffer;
    int m_backoffMs = 15000;
    bool m_running = false;
};

} // namespace sc
