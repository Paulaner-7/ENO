#pragma once

#include "Package.h"

#include <QList>
#include <QString>

#include <functional>
#include <optional>

namespace sc::ext {

// What the "Add repository" sheet shows before anything is stored.
struct RepositoryPreview {
    Repository repo;
    QList<Package> packages;
    // Installed right away when the user confirms: extensions in the content
    // language, without adult ones unless they are switched on.
    QList<Package> autoInstall;
};

struct Installed {
    Package package;
    QString repoUrl;
    QStringList signers;   // SHA-256 of the signing certificates
    QString path;          // the APK or plugin file
    qint64 installedAt = 0;
    bool trusted = false;
    LibSupport lib = LibSupport::Supported;
    // The newer version a repository lists, if any.
    std::optional<Package> update;
};

// A repository with what its last read found.
struct RepositoryStatus {
    Repository repo;
    int count = 0;         // extensions it lists
    QString error;         // why the last refresh failed; empty when it worked
};

// A one-tap "add repository" link: SCANCORE's own and the schemes the
// community's links already use (Mihon, Tachiyomi, Aniyomi, LNReader).
struct RepositoryLink {
    QString url;
    std::optional<Kind> kind;
};

// One source of an installed extension, as the source registries list it.
struct SourceEntry {
    QString key;           // mihon:<id>, aniyomi:<id>, lnreader:<id>
    Kind kind = Kind::Mihon;
    QString id;
    QString name;
    QString lang;
    QString baseUrl;
    QString pkg;
    QString path;
    QString signer;        // the key SCANCORE trusted it with
    bool enabled = true;
    bool nsfw = false;
    // Why it cannot run (untrusted, lib version); empty when it can.
    QString blocked;
};

// Repositories, installs, trust and per-source state, stored in the
// database and the extensions folder. Every call that touches the network
// blocks: run it on a worker (Task). Lookups are served from memory.
class Manager {
public:
    // The folder extension files go in (<app data>/extensions).
    static void setStorage(const QString &folder);
    static QString storage();

    // ---- repositories
    // Reads a repository from a link the user gave (index.min.json,
    // repo.json, a store, an LNReader plugin list; gzip or protobuf), or
    // throws. `hint` is the kind a link scheme names.
    static RepositoryPreview preview(const QString &url, std::optional<Kind> hint = std::nullopt);
    // Stores it; its signing key is trusted from now on.
    static Repository add(const RepositoryPreview &preview);
    static void removeRepository(const QString &url);
    static QList<Repository> repositories();
    static QList<RepositoryStatus> repositoryStatus();
    // The repository a one-tap link names, or nothing when `link` is not
    // one (scancore://repos/add?url=…, tachiyomi://add-repo?url=…,
    // mihon://add-repo?url=…, aniyomi://add-repo?url=…,
    // lnreader://repo/add?url=…).
    static std::optional<RepositoryLink> parseLink(const QString &link);
    // Reads a repository's index again (conditional request); returns
    // whether anything changed.
    static bool refresh(const QString &url);
    static void refreshAll();
    // Every extension the repositories list, the newest of each.
    // `repos`, when given, gets the repository each one is listed by (the
    // one with its newest version), in the same order.
    static QList<Package> available(std::optional<Kind> kind = std::nullopt, QStringList *repos = nullptr);
    static QString repositoryOf(Kind kind, const QString &pkg);

    // ---- extensions
    // Downloads and stores the extension the repositories list. An
    // extension signed with a repository's key (or trusted before, same
    // version and key) is trusted; others wait for trust().
    static Installed install(Kind kind, const QString &pkg);
    static void uninstall(Kind kind, const QString &pkg);
    static QList<Installed> installed(std::optional<Kind> kind = std::nullopt);
    static std::optional<Installed> installedPackage(Kind kind, const QString &pkg);
    static void trust(Kind kind, const QString &pkg);
    static QList<Installed> updates();
    // Installs every update whose signer is already trusted (the "update
    // automatically" setting); returns the ones that were installed.
    static QList<Installed> installTrustedUpdates();

    // The extensions a new repository installs: in `language` (one
    // language, or several sources one of which is in it), supported,
    // not torrent-only, without adult ones unless `adult`.
    static QList<Package> autoInstallSelection(const QList<Package> &packages, const QString &language, bool adult);

    // ---- sources
    static QList<SourceEntry> sources(std::optional<Kind> kind = std::nullopt);
    static void setSourceEnabled(const QString &key, bool enabled);
    // Health: the last error a source gave, cleared by a success.
    static void recordSourceResult(const QString &key, const QString &error);
    static QString sourceError(const QString &key);
    static QString preference(const QString &sourceKey, const QString &key);
    static void setPreference(const QString &sourceKey, const QString &key, const QString &value);

    // Called after anything above changes the set of sources (registries
    // rebuild from it).
    static void setChangedCallback(std::function<void()> callback);
    // Drops what is kept in memory (tests; after a backup restore).
    static void reload();
};

} // namespace sc::ext
