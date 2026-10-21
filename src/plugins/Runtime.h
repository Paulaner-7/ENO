#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace sc::plugins {

// An installed LNReader plugin: its source key ("lnreader:<id>"), the id
// its code declares, and the file holding the code.
struct PluginRef {
    QString key;
    QString id;
    QString path;
};

// Runs LNReader plugins (compiled JavaScript) in QJSEngine. Each plugin has
// one engine, created on first use on one of a few plugin threads and
// reused: SCANCORE's shims (resources/plugins/runtime.js) and the bundled
// libraries are evaluated once per engine and the plugin's code compiled
// once. Requests go through SCANCORE's network (cookies, user agent) on
// the shared pool, so a plugin waiting for a site holds no thread; the
// plugin's storage is kept in the database. A call that runs JavaScript
// for longer than its limit is interrupted, so one plugin cannot stall the
// others on its thread. Engines idle for a quarter of an hour are dropped.
class Runtime {
public:
    // Blocking: calls `method` with `args` (JSON) and waits for its promise.
    // Run it on a worker. Throws Error with the plugin's message.
    static QJsonValue call(const PluginRef &plugin, const QString &method, const QJsonArray &args = {}, int timeoutMs = 30000);
    // What the plugin declares: {id, name, version, site, lang, icon,
    // filters, pluginSettings, imageRequestInit, hasParsePage,
    // hasPopularLatest}. Loads it if needed.
    static QJsonObject info(const PluginRef &plugin);
    // Drops the plugin's engine (after an update or removal).
    static void unload(const QString &key);
    // The libraries plugins need (cheerio, htmlparser2, dayjs) that this
    // build lacks; empty when complete.
    static QStringList missingLibraries();
};

} // namespace sc::plugins
