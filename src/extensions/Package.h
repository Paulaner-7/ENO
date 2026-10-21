#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace sc::ext {

// The extension ecosystems SCANCORE runs: Mihon/Tachiyomi extensions
// (comics), Aniyomi extensions (anime) and LNReader plugins (novels).
enum class Kind { Mihon, Aniyomi, LNReader };

QString kindName(Kind kind);           // "mihon", "aniyomi", "lnreader"
std::optional<Kind> kindFrom(const QString &name);
QString kindMedia(Kind kind);          // "comic", "anime", "novel"

enum class ContentWarning { Safe = 0, Mixed = 1, Nsfw = 2 };

// One source inside an extension. Mihon and Aniyomi ids are stable hashes
// (the same on every device and in Mihon backups), kept as text because
// they are 64-bit; LNReader plugins are one source named by the plugin id.
struct SourceInfo {
    QString id;
    QString name;
    QString lang;
    QString baseUrl;
};

// An extension as a repository lists it, or as installed.
struct Package {
    Kind kind = Kind::Mihon;
    QString pkg;          // package name, or the LNReader plugin id
    QString name;
    QString versionName;
    qint64 versionCode = 0;
    QString lib;          // extensions-lib version ("1.4", "16"); empty for plugins
    QString lang;         // one language, or "all"
    ContentWarning warning = ContentWarning::Safe;
    QString fileUrl;      // the APK, or the plugin's JavaScript
    QString iconUrl;
    QString site;         // LNReader: the site the plugin reads
    bool torrent = false; // Aniyomi: needs a torrent client, which SCANCORE has not
    QList<SourceInfo> sources;

    QJsonObject toJson() const;
    static Package fromJson(const QJsonObject &o);
    // The languages of its sources.
    QStringList languages() const;
};

// A repository the user added.
struct Repository {
    Kind kind = Kind::Mihon;
    QString url;          // the address it is known by (index or store)
    QString name;
    QString badge;        // short name for badges
    QString website;
    QString signingKey;   // SHA-256 of the signing certificate, lower-case hex
    QString listUrl;      // where its extensions are listed

    QJsonObject toJson() const;
    static Repository fromJson(const QJsonObject &o);
};

// SHA-256 fingerprints in one spelling: lower-case hex without separators.
QString normalizedFingerprint(const QString &fingerprint);

// Lib versions SCANCORE's hosts implement, per kind.
QStringList supportedLibs(Kind kind);
enum class LibSupport { Supported, TooOld, TooNew, Unknown };
LibSupport libSupport(Kind kind, const QString &lib);

} // namespace sc::ext
