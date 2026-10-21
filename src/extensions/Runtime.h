#pragma once

#include <QString>

#include <functional>

namespace sc::ext {

// The Extensions runtime (desktop): a trimmed Java and the extension host,
// downloaded on request (about 100 MB) from the extensions-runtime-<version>
// pre-release of SCANCORE's repository, one zip per system with a SHA-256
// beside it. The app needs the host version it was built with.
class Runtime {
public:
    static constexpr const char *HostVersion = "1";

    // Where it is installed (<app data>/extensions/runtime).
    static void setDir(const QString &dir);
    static QString dir();
    // This system as the zips name it ("linux-x64"); empty where there is
    // no runtime (Android, other CPUs).
    static QString platform();
    static bool installed();
    // The asset and checksum links; the base can be changed for tests.
    static void setDownloadBase(const QString &base);
    static QString assetUrl();

    // Downloads, checks and unpacks it, replacing an older one. Blocking;
    // `status` hears what it is doing. Throws on any failure, leaving what
    // was installed in place.
    static void install(const std::function<void(const QString &)> &status = {});
    static void remove();
};

} // namespace sc::ext
