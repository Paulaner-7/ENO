#pragma once

#include "LineHost.h"

#include <QStringList>

#include <memory>

namespace sc::ext {

// The extension host as a helper process (desktop): a Java runtime running
// host/desktop, spoken to over its stdin and stdout, one JSON object per
// line. It starts on the first call, answers many calls at once (matched by
// id), and stops after ten minutes unused; a call after it stopped or
// crashed starts it again.
class ProcessHost : public LineHost {
public:
    struct Launch {
        QString java;          // the java executable
        QString libDir;        // the host's jars
        QString root;          // its data folder (cookies, converted jars)
        QString userAgent;     // so browser-check cookies stay valid
        QString appVersion;
        QStringList jvmArgs;
    };

    explicit ProcessHost(Launch launch);
    ~ProcessHost() override;

    bool available() const override;
    QString unavailableReason() const override;

    // Stops the process now (app exit, or to restart it). Calls waiting
    // fail; the next call starts it again.
    void stop();
    void setIdleTimeout(int ms);
    // Uses another runtime from the next start (after one was installed);
    // stops the running one.
    void setLaunch(Launch launch);

    // How the runtime is found: an installed Extensions runtime (bin/java,
    // lib/), or a development build of host/desktop with a Java 17+ on
    // PATH or in JAVA_HOME. Empty `java` when there is none.
    static Launch find(const QString &runtimeDir, const QString &developmentLibDir);

protected:
    void sendLine(const QByteArray &line) override;

private:
    struct State;
    std::shared_ptr<State> m_state;
};

} // namespace sc::ext
