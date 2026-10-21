#pragma once

#include "Package.h"
#include "core/Http.h"

#include <QJsonObject>
#include <QJsonValue>

#include <memory>
#include <optional>

namespace sc::ext {

// Source keys: "mihon:<source id>", "aniyomi:<source id>",
// "lnreader:<plugin id>". Series and chapter links of an extension source
// carry the key in front of the extension's own link ("mihon:1234:/manga/9"),
// so they lead back to their source, and library ids stay
// "<media>:<key>:<link>" (Library::idFor).
QString sourceKey(Kind kind, const QString &id);
QString wrapUrl(const QString &key, const QString &url);
struct WrappedUrl {
    QString key;
    QString url;
};
std::optional<WrappedUrl> unwrapUrl(const QString &url);

// Where extensions run: the Kotlin host for Mihon and Aniyomi extensions
// (a helper process on desktop, in-process on Android) and the JavaScript
// runtime for LNReader plugins. Calls are JSON-RPC style, block, and run on
// worker threads; they throw sc::Error with a message for the user.
class Host {
public:
    virtual ~Host() = default;
    // False until the runtime is there (the desktop download).
    virtual bool available() const = 0;
    virtual QString unavailableReason() const { return {}; }
    virtual QJsonValue call(const QString &method, const QJsonObject &params) = 0;
};

void setHost(Kind kind, std::shared_ptr<Host> host);
std::shared_ptr<Host> host(Kind kind);
// The host for a kind, or an Error the user can act on.
std::shared_ptr<Host> requireHost(Kind kind);

// Some extensions serve their videos from a server of their own in the
// host (an address on this device): while one plays, the host stays up
// however long nothing is asked of it. Counted; any thread.
void holdHost(bool hold);
bool hostHeld();

} // namespace sc::ext
