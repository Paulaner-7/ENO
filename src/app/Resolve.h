#pragma once

#include "core/Parallel.h"
#include "core/QuietSites.h"
#include "core/Text.h"

#include <QSet>
#include <QStringList>
#include <QVariantList>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

namespace sc {

// Which sources a "Read from" lookup asks. No scope: built-in sources only
// (one tap must not wait on every installed extension), those `builtIn`
// accepts; "extensions": every extension that is on (moving a series); any
// other scope: the one source with that key (an extension the reader picked).
inline bool resolveAsks(const QString &scope, const QString &key, bool enabled, bool extension, bool builtIn) {
    if (scope.isEmpty())
        return !extension && builtIn;
    if (scope == QLatin1String("extensions"))
        return enabled && extension;
    return enabled && key == scope;
}

// A source's site, for QuietSite (comic sources keep it in their config,
// anime sources answer it themselves).
template <typename SourcePtr>
QString siteOf(const SourcePtr &source) {
    if constexpr (requires { source->baseUrl(); })
        return source->baseUrl();
    else if constexpr (requires { source->config().base; })
        return source->config().base;
    else
        return {};
}

// Search results that carry the other names a source lists a series under,
// and how many chapters it has there. A named concept on the type: MSVC
// gets a requires-expression on a local variable inside a lambda wrong
// (always false).
template <typename T>
concept HasOtherNames = requires(const T &item) { item.names; };
template <typename T>
concept HasUnitCount = requires(const T &item) { item.units; };

// "Read from" for a catalogue title: the series on the given sources listed
// under any of its names. The main name is asked everywhere first; other
// names only of sources that missed (three rounds at most). `readable`
// drops matches whose chapters were all taken down. Matches in the content
// language come first, then the sources' own order; each says whether it is
// in that language ("preferred"), so the caller can open the only one.
// `known` gives a source's series straight away (a tracker's link to it).
// `progress` gets the matches so far each time a source answers with one
// (not yet checked with `readable`), so they show before the slowest source.
template <typename SourcePtr>
QVariantList resolveAcross(const QList<SourcePtr> &sources, const QStringList &titles,
                           const std::function<bool(const SourcePtr &, const QString &url)> &readable = {},
                           const std::function<QString(const SourcePtr &)> &known = {},
                           const std::function<void(const QVariantList &)> &progress = {}) {
    QStringList names;
    QSet<QString> wanted;
    for (const auto &title : titles) {
        const auto n = normalizedTitle(title);
        if (n.isEmpty() || wanted.contains(n))
            continue;
        wanted.insert(n);
        names.append(title);
    }
    struct Found {
        SourcePtr source;
        QString url;
        QString title;
        QString cover;
        int order;
    };
    QList<Found> found;
    QSet<QString> done;
    const auto language = contentLanguage();
    const auto preferred = [&](const Found &f) { return baseLanguage(f.source->config().language) == language; };
    const auto build = [&] {
        auto order = found;
        std::stable_sort(order.begin(), order.end(), [&](const Found &a, const Found &b) {
            if (preferred(a) != preferred(b))
                return preferred(a);
            return a.order < b.order;
        });
        QVariantList matches;
        for (const auto &f : order)
            matches.append(QVariantMap{{"source", f.source->key()},
                                       {"sourceLabel", f.source->label()},
                                       {"language", f.source->config().language},
                                       {"preferred", preferred(f)},
                                       {"url", f.url},
                                       {"title", f.title},
                                       {"cover", f.cover}});
        return matches;
    };
    // Series a tracker links to on a source: no need to search there.
    if (known && !names.isEmpty())
        for (const auto &source : sources)
            if (const auto url = known(source); !url.isEmpty()) {
                done.insert(source->key());
                found.append({source, url, names.first(), QString(), int(sources.indexOf(source))});
            }
    if (progress && !found.isEmpty())
        progress(build());
    for (int round = 0; round < std::min<int>(names.size(), 3) && done.size() < sources.size(); ++round) {
        QList<SourcePtr> asked;
        using Hit = QList<QStringList>;
        QList<std::function<Hit()>> jobs;
        for (const auto &source : sources)
            if (!done.contains(source->key())) {
                asked.append(source);
                jobs.append([source, name = names[round], many = sources.size() > 1] {
                    // Many sites at once: a browser check waits for its own
                    // button, for as long as this source's call runs.
                    std::optional<QuietSite> quiet;
                    if (many)
                        quiet.emplace(siteOf(source));
                    Hit hits;
                    for (const auto &item : source->search(name)) {
                        QStringList hit{item.title, item.url, item.cover, QStringLiteral("0")};
                        if constexpr (HasUnitCount<std::decay_t<decltype(item)>>)
                            hit[3] = QString::number(item.units);
                        if constexpr (HasOtherNames<std::decay_t<decltype(item)>>)
                            hit += item.names;
                        hits.append(hit);
                    }
                    return hits;
                });
            }
        // A source that hangs is left out of this round, not waited for.
        parallelEach<Hit>(jobs, 15000, [&](int i, const Outcome<Hit> &result) {
            if (!result.ok())
                return;
            if (done.contains(asked[i]->key()))
                return;
            // A hit is [title, url, cover, chapters, other names...]: a
            // source may list it under a translated name the catalogue does
            // not have. A hit named so comes first; one only known by such a
            // name (a spin-off can carry its series' name) only when none is.
            // Among hits named alike, the one with the most chapters: an
            // aggregator (Kagane) lists a series once per uploader, some with
            // only a few chapters.
            const QStringList *pick = nullptr;
            const auto fuller = [](const QStringList &a, const QStringList *b) { return !b || a[3].toInt() > b->at(3).toInt(); };
            for (const auto &hit : *result.value)
                if (wanted.contains(normalizedTitle(hit[0])) && fuller(hit, pick))
                    pick = &hit;
            for (int h = 0; !pick && h < result.value->size(); ++h) {
                const auto &hit = result.value->at(h);
                if (std::any_of(hit.begin() + 4, hit.end(), [&](const QString &n) { return wanted.contains(normalizedTitle(n)); }))
                    pick = &hit;
            }
            if (pick) {
                done.insert(asked[i]->key());
                found.append({asked[i], pick->at(1), pick->at(0), pick->at(2), int(sources.indexOf(asked[i]))});
                if (progress)
                    progress(build());
            }
        });
    }
    // Taken-down series are left out (one request per match, in parallel).
    if (readable && !found.isEmpty()) {
        QList<std::function<bool()>> checks;
        for (const auto &f : found)
            checks.append([readable, f] { return readable(f.source, f.url); });
        const auto results = parallel(checks);
        QList<Found> kept;
        for (int i = 0; i < found.size(); ++i)
            if (!results[i].ok() || *results[i].value)
                kept.append(found[i]);
        found = kept;
    }
    return build();
}

} // namespace sc
