#pragma once

#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QUrl>

#include <memory>

namespace sc {

// sources/manifest.json: source domains, availability and template options.
// The copy built into the app is the floor; a newer copy published in the
// GitHub repository replaces it at runtime so a domain change or a broken
// site does not need a new release. It holds data only, never code.
class Manifest {
public:
    static constexpr const char *RemoteUrl =
        "https://raw.githubusercontent.com/medium-gh/scancore/main/sources/manifest.json";

    static void load(const QString &cachePath);
    static QJsonObject current();
    static int version();

    // Fetches the published manifest; returns true when a newer one was
    // applied. Runs on a worker thread.
    static bool refresh();
    // Loads a manifest directly (tools and tests).
    static bool loadData(const QByteArray &json) { return apply(json, false); }

    // The running app's version, which entries' "minVersion" is checked
    // against.
    static QString appVersion();
    // False for an entry that asks for a newer app ("minVersion"), so a
    // source that needs new code is not run by an older build.
    static bool supports(const QJsonObject &entry);
    // The bundled sources pack: on unless the published manifest switches it
    // off ("bundled": {"enabled": false, "reason": "..."}), which leaves
    // extensions and files to read from. Builds without the pack say so too.
    static bool bundledEnabled();
    // Why the pack is off, for the sources' "switched off" note.
    static QString bundledReason();

private:
    static bool apply(const QByteArray &json, bool persist);
};

// The source that handles a link. Sources sharing a host tell their links
// apart by a tag in the fragment (https://mangadex.org/title/<id>#es): a
// tagged source takes only links with its tag, and the others never take a
// link that carries one of those tags.
template <typename Source>
std::shared_ptr<Source> sourceForUrl(const QList<std::shared_ptr<Source>> &sources, const QString &url) {
    const QUrl parsed(url);
    const auto fragment = parsed.fragment();
    QSet<QString> tags;
    for (const auto &source : sources) {
        const auto &tag = source->config().urlTag;
        if (tag.isEmpty())
            continue;
        if (tag == fragment && source->owns(parsed))
            return source;
        tags.insert(tag);
    }
    if (!fragment.isEmpty() && tags.contains(fragment))
        return nullptr;
    for (const auto &source : sources)
        if (source->config().urlTag.isEmpty() && source->owns(parsed))
            return source;
    return nullptr;
}

} // namespace sc
