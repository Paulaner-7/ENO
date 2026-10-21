#pragma once

#include "LineHost.h"

#include <memory>

namespace sc::ext {

// The extension host inside the app (Android): host/android's Bridge, in
// the app's own Java runtime, where extensions load as they do in Mihon.
// Lines go to Bridge.send() over JNI and answers come back through its
// native reply(). Starts on the first call.
class JniHost : public LineHost, public std::enable_shared_from_this<JniHost> {
public:
    struct Launch {
        QString root;      // its data folder (cookies, memos, settings)
        QString userAgent; // so browser-check cookies stay valid
        QString appVersion;
    };

    static std::shared_ptr<JniHost> create(Launch launch);
    ~JniHost() override;

    bool available() const override { return true; }
    QJsonValue call(const QString &method, const QJsonObject &params) override;

protected:
    void sendLine(const QByteArray &line) override;

private:
    explicit JniHost(Launch launch);
    static void reply(const QByteArray &line);

    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace sc::ext
