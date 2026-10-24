#include "QuietSites.h"

#include <QHash>
#include <QMutex>
#include <QUrl>

namespace sc {

namespace {

QMutex g_mutex;
QHash<QString, int> g_hosts;

QString siteHost(const QString &url) {
    auto host = QUrl(url).host().toLower();
    if (host.startsWith(QLatin1String("www.")))
        host = host.mid(4);
    return host;
}

} // namespace

QuietSite::QuietSite(const QString &siteUrl) : m_host(siteHost(siteUrl)) {
    if (m_host.isEmpty())
        return;
    QMutexLocker lock(&g_mutex);
    ++g_hosts[m_host];
}

QuietSite::~QuietSite() {
    if (m_host.isEmpty())
        return;
    QMutexLocker lock(&g_mutex);
    if (--g_hosts[m_host] <= 0)
        g_hosts.remove(m_host);
}

bool isQuietSite(const QString &url) {
    const auto host = siteHost(url);
    if (host.isEmpty())
        return false;
    QMutexLocker lock(&g_mutex);
    for (auto it = g_hosts.cbegin(); it != g_hosts.cend(); ++it)
        if (host == it.key() || host.endsWith(QLatin1Char('.') + it.key()) || it.key().endsWith(QLatin1Char('.') + host))
            return true;
    return false;
}

} // namespace sc
