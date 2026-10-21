#pragma once

#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

#include <memory>

class QNetworkAccessManager;
class QUdpSocket;

namespace sc {

class CastProxy;
class CastSession;

// Casting an episode to a TV: Chromecasts (found over mDNS, played by
// Google's Default Media Receiver over the Cast protocol) and DLNA renderers
// (smart TVs, found over SSDP, driven with UPnP AVTransport). The device
// fetches the video through CastProxy, which adds the headers it needs.
// The player shows the device's position and controls it while casting.
class Cast : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // [{id, name, kind: chromecast|dlna}]
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY devicesChanged)
    // Casting (connecting, playing, paused, …) to `deviceName`.
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY stateChanged)
    // connecting, buffering, playing, paused, ended, error
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY positionChanged)

public:
    static Cast *instance();
    static Cast *create(QQmlEngine *, QJSEngine *) { return instance(); }
    ~Cast() override;

    QVariantList devices() const;
    bool searching() const { return m_searchEnds.isActive(); }
    bool active() const;
    QString deviceName() const;
    QString state() const { return m_state; }
    QString error() const { return m_error; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }

    // Looks for devices on the network for a few seconds (the list keeps
    // what it found before).
    Q_INVOKABLE void search();
    // Plays `video` (url, headers, subtitles) on a device from `startAt`.
    Q_INVOKABLE void play(const QString &deviceId, const QVariantMap &video, const QString &title, const QString &cover, double startAt);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void seek(double seconds);
    // Stops the video on the device and ends casting.
    Q_INVOKABLE void stop();

    // For tests: a device at a known address ("chromecast" or "dlna";
    // for DLNA, `address` is its description's URL).
    Q_INVOKABLE void addDevice(const QString &kind, const QString &name, const QString &address, int port = 8009);

    // Session callbacks.
    void sessionState(const QString &state, const QString &error = {});
    void sessionPosition(double position, double duration);

    struct Device {
        QString id;
        QString name;
        QString kind;
        QHostAddress host;
        int port = 0;
        QUrl description; // DLNA: the device description
        QUrl control;     // DLNA: AVTransport's control address
    };

signals:
    void devicesChanged();
    void stateChanged();
    void positionChanged();

private:
    explicit Cast(QObject *parent = nullptr);
    void readSsdp();
    void readMdns();
    void describe(const QUrl &location);
    void found(const Device &device);

    QNetworkAccessManager *m_network = nullptr;
    CastProxy *m_proxy = nullptr;
    QUdpSocket *m_ssdp = nullptr;
    QUdpSocket *m_mdns = nullptr;
    QTimer m_searchEnds;
    QList<Device> m_devices;
    QSet<QString> m_described;
    std::unique_ptr<CastSession> m_session;
    QString m_state;
    QString m_error;
    double m_position = 0;
    double m_duration = 0;
};

// One device being cast to (Cast.cpp has the two kinds).
class CastSession : public QObject {
    Q_OBJECT
public:
    explicit CastSession(const Cast::Device &device) : m_device(device) {}
    const Cast::Device &device() const { return m_device; }
    virtual void load(const QString &url, const QString &type, const QString &title, const QString &cover, double startAt,
                      const QVariantList &subtitles) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void seek(double seconds) = 0;
    virtual void stop() = 0;

protected:
    Cast::Device m_device;
};

} // namespace sc
