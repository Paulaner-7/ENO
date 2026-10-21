#include "Manifest.h"

#include "core/Http.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QReadWriteLock>
#include <QSaveFile>
#include <QVersionNumber>

namespace sc {

namespace {
QReadWriteLock g_lock;
QJsonObject g_builtIn;
QJsonObject g_manifest;
QString g_cachePath;

// A manifest entry's identity: "id" upstream, "key" in ENO's manifest.
QString entryId(const QJsonObject &entry) {
    const auto id = entry.value(QStringLiteral("id")).toString();
    return id.isEmpty() ? entry.value(QStringLiteral("key")).toString() : id;
}

// Merges an upstream manifest with ENO's built-in manifest so upstream domain
// fixes and new sources are applied while custom ENO sources (new IDs or
// entries marked with "enoOverride": true) are never lost or overwritten.
// A category listed in the built-in manifest's "enoExclusive" is pinned to
// the built-in list entirely (ENO ships only MangaWorld for comics).
QJsonObject mergeWithBuiltIn(const QJsonObject &incoming, const QJsonObject &builtIn) {
    if (builtIn.isEmpty())
        return incoming;
    QJsonObject merged = incoming;
    const auto exclusive = builtIn.value(QStringLiteral("enoExclusive")).toArray();
    for (const auto &category : {QStringLiteral("comics"), QStringLiteral("novels")}) {
        const auto builtInArr = builtIn.value(category).toArray();
        if (builtInArr.isEmpty())
            continue;
        if (exclusive.contains(category)) {
            merged.insert(category, builtInArr);
            continue;
        }
        auto targetArr = merged.value(category).toArray();
        QHash<QString, int> indexById;
        for (int i = 0; i < targetArr.size(); ++i) {
            const auto id = entryId(targetArr.at(i).toObject());
            if (!id.isEmpty())
                indexById.insert(id, i);
        }
        for (const auto &val : builtInArr) {
            const auto obj = val.toObject();
            const auto id = entryId(obj);
            if (id.isEmpty())
                continue;
            if (!indexById.contains(id)) {
                // Custom source added in ENO that does not exist upstream.
                indexById.insert(id, targetArr.size());
                targetArr.append(obj);
            } else if (obj.value(QStringLiteral("enoOverride")).toBool() ||
                       obj.value(QStringLiteral("kaiOverride")).toBool() ||
                       obj.value(QStringLiteral("custom")).toBool()) {
                // Existing source explicitly customized in ENO.
                targetArr.replace(indexById.value(id), obj);
            }
        }
        merged.insert(category, targetArr);
    }
    return merged;
}
} // namespace

void Manifest::load(const QString &cachePath) {
    g_cachePath = cachePath;
    QFile builtIn(QStringLiteral(":/sources/manifest.json"));
    if (builtIn.open(QIODevice::ReadOnly)) {
        const auto raw = builtIn.readAll();
        const auto obj = QJsonDocument::fromJson(raw).object();
        if (obj.value(QStringLiteral("comics")).isArray()) {
            QWriteLocker lock(&g_lock);
            g_builtIn = obj;
            g_manifest = obj;
        }
    }
    QFile cached(cachePath);
    if (cached.open(QIODevice::ReadOnly))
        apply(cached.readAll(), false);
}

QJsonObject Manifest::current() {
    QReadLocker lock(&g_lock);
    return g_manifest;
}

int Manifest::version() { return current().value(QStringLiteral("version")).toInt(); }

QString Manifest::appVersion() {
    const auto running = QCoreApplication::applicationVersion();
    return running.isEmpty() ? QStringLiteral(SCANCORE_VERSION) : running;
}

bool Manifest::supports(const QJsonObject &entry) {
    const auto wanted = entry.value(QStringLiteral("minVersion")).toString();
    if (wanted.isEmpty())
        return true;
    const auto needed = QVersionNumber::fromString(wanted);
    // A requirement this build cannot read is one it does not meet.
    return !needed.isNull() && QVersionNumber::fromString(appVersion()) >= needed;
}

bool Manifest::bundledEnabled() {
#ifndef SCANCORE_BUNDLED_SOURCES
    return false;
#else
    return current().value(QStringLiteral("bundled")).toObject().value(QStringLiteral("enabled")).toBool(true);
#endif
}

QString Manifest::bundledReason() {
    const auto reason = current().value(QStringLiteral("bundled")).toObject().value(QStringLiteral("reason")).toString();
    return reason.isEmpty() ? QStringLiteral("Built-in sources are off. Add an extension repository in Settings → Extensions to keep reading.") : reason;
}

bool Manifest::apply(const QByteArray &json, bool persist) {
    const auto doc = QJsonDocument::fromJson(json);
    const auto object = doc.object();
    if (!object.value(QStringLiteral("comics")).isArray())
        return false;
    {
        QWriteLocker lock(&g_lock);
        if (!g_manifest.isEmpty() &&
            object.value(QStringLiteral("version")).toInt() <= g_manifest.value(QStringLiteral("version")).toInt())
            return false;
        g_manifest = mergeWithBuiltIn(object, g_builtIn);
    }
    if (persist && !g_cachePath.isEmpty()) {
        QSaveFile file(g_cachePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(json);
            file.commit();
        }
    }
    return true;
}

bool Manifest::refresh() {
    const auto response = Http::get(QUrl(QString::fromLatin1(RemoteUrl)), {}, 10000);
    if (!response.ok())
        return false;
    return apply(response.body, true);
}

} // namespace sc
