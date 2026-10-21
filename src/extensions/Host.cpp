#include "Host.h"

#include <QHash>
#include <QReadWriteLock>

#include <atomic>

namespace sc::ext {

namespace {
QReadWriteLock g_lock;
QHash<int, std::shared_ptr<Host>> g_hosts;
std::atomic<int> g_holds{0};
} // namespace

void holdHost(bool hold) { g_holds += hold ? 1 : -1; }
bool hostHeld() { return g_holds.load() > 0; }

QString sourceKey(Kind kind, const QString &id) { return kindName(kind) + QLatin1Char(':') + id; }

QString wrapUrl(const QString &key, const QString &url) { return key + QLatin1Char(':') + url; }

std::optional<WrappedUrl> unwrapUrl(const QString &url) {
    const auto colon = url.indexOf(QLatin1Char(':'));
    if (colon <= 0 || !kindFrom(url.left(colon)))
        return std::nullopt;
    const auto second = url.indexOf(QLatin1Char(':'), colon + 1);
    if (second <= colon + 1)
        return std::nullopt;
    return WrappedUrl{url.left(second), url.mid(second + 1)};
}

void setHost(Kind kind, std::shared_ptr<Host> host) {
    QWriteLocker lock(&g_lock);
    g_hosts.insert(int(kind), std::move(host));
}

std::shared_ptr<Host> host(Kind kind) {
    QReadLocker lock(&g_lock);
    return g_hosts.value(int(kind));
}

std::shared_ptr<Host> requireHost(Kind kind) {
    auto h = host(kind);
    if (!h)
        throw Error(kind == Kind::LNReader ? QStringLiteral("This copy of SCANCORE cannot run novel plugins.")
                                           : QStringLiteral("This copy of SCANCORE cannot run extensions."));
    if (!h->available())
        throw Error(h->unavailableReason().isEmpty() ? QStringLiteral("Extensions need the Extensions runtime. Install it in Settings → Extensions.")
                                                     : h->unavailableReason());
    return h;
}

} // namespace sc::ext
