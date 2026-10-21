#pragma once

#include <QNetworkCookieJar>

#include <mutex>

namespace sc {

// One cookie jar shared by every worker thread's network manager, guarded by
// a mutex. Cookies set by a browser-check window land here too, and the jar
// is saved to disk so a passed check survives restarts.
class CookieJar : public QNetworkCookieJar {
public:
    static CookieJar &shared();
    static QObject *owner();

    QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override;
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookies, const QUrl &url) override;
    bool insertCookie(const QNetworkCookie &cookie) override;
    bool deleteCookie(const QNetworkCookie &cookie) override;

    void load(const QString &path);
    void save() const;

    // SCANCORE's own site and the sync server set in Settings → Sync (and
    // their subdomains). Their cookies carry the account's session: they are
    // never handed to extensions, plugins or browser checks.
    static bool isProtected(const QUrl &url);
    // Reads the sync server again (after it changes).
    static void refreshProtected();

private:
    using QNetworkCookieJar::QNetworkCookieJar;
    mutable std::recursive_mutex m_mutex;
    QString m_path;
};

} // namespace sc
