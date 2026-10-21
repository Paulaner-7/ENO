#pragma once

#include "core/Http.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <memory>
#include <optional>

namespace sc::comics {

struct Summary {
    QString title;
    QString url;
    QString cover;
    QString type;
    QString author;
    QString genres;
    QString views;
    QString source;
    // The source's other names for it (other languages), for "Read from" to
    // recognise a catalogue title shown under a translated name.
    QStringList names;
    QVariantMap toMap() const;
};

struct Chapter {
    QString number;
    QString title;
    QString url;
    QString thumbnail;
    QString slug;
    QString released;
    // The group that translated it, when the source says.
    QString scanlator;
    QVariantMap toMap() const;
};

struct Panel {
    QString url;
    int width = 0;
    int height = 0;
};

struct Details {
    QString title;
    QString cover;
    QString description;
    QString author;
    QString status;
    QStringList genres;
    // Ids of the same title on trackers, when the site links them
    // ("anilist", "mal").
    QVariantMap trackerIds;
};

struct LatestItem {
    QString source;
    QString sourceLabel;
    QString title;
    QString url;
    QString cover;
    QString chapterNumber;
    QString chapterTitle;
    QString chapterUrl;
    QString publishedAt;
    QVariantMap toMap() const;
};

// One page of a source's own listing (popular, latest, or a search with
// filters).
struct Listing {
    QList<Summary> items;
    bool hasNextPage = false;
};

// Settings for one source, read from sources/manifest.json. Domains,
// availability and template options live there so they can change without
// a new release.
struct SourceConfig {
    QString key;
    QString label;
    QString engine;
    QString base;
    QString api;
    QStringList hosts;
    bool enabled = true;
    bool nsfw = false;
    QString disabledReason;
    QJsonObject options;
    // Language of the content (short BCP 47 code: "en", "es").
    QString language = QStringLiteral("en");
    // Set when another source shares the host: this source's links end in
    // "#<urlTag>" so Registry::forUrl can tell them apart.
    QString urlTag;
    // Off only because its language is not among the user's source
    // languages: "Read from" can still look there when asked to.
    bool languageOff = false;
};

class Source {
public:
    explicit Source(SourceConfig config) : m_config(std::move(config)) {}
    virtual ~Source() = default;

    const SourceConfig &config() const { return m_config; }
    QString key() const { return m_config.key; }
    QString label() const { return m_config.label; }
    QString language() const { return m_config.language; }
    bool enabled() const { return m_config.enabled; }
    virtual bool owns(const QUrl &url) const;

    virtual QList<Summary> search(const QString &query) = 0;
    virtual QList<Chapter> chapters(const QString &seriesUrl) = 0;
    virtual QList<Panel> panels(const QString &chapterUrl) = 0;
    virtual std::optional<Details> details(const QString &) { return std::nullopt; }
    virtual QList<LatestItem> latest(int) { return {}; }
    virtual bool hasLatest() const { return false; }
    // False when the series is listed but none of its chapters can be read
    // here (taken down), so "Read from" leaves it out. One request at most.
    virtual bool readable(const QString &) { return true; }
    // This source's own link for a series page link from elsewhere (a
    // tracker's "where to read"), or empty when it cannot tell.
    virtual QString seriesLink(const QUrl &) const { return {}; }
    // Referer sent with image requests for this source.
    virtual QString imageReferer() const { return m_config.base + QLatin1Char('/'); }
    // Other headers its images need.
    virtual Headers imageHeaders() const { return {}; }

    // Browsing one source by itself, a page at a time (extensions):
    // "popular", "latest" or "search" (with `query` and `filters`).
    virtual bool canBrowse() const { return false; }
    virtual bool hasLatestListing() const { return false; }
    virtual Listing browse(const QString &, int, const QString & = {}, const QJsonArray & = {}) { return {}; }
    // Its search filters and settings, described as JSON by its host.
    virtual QJsonArray filters() { return {}; }
    virtual QJsonArray preferences() { return {}; }
    virtual void setPreference(const QString &, const QJsonValue &) {}

protected:
    Headers jsonHeaders() const;
    Headers htmlHeaders() const;
    // A series or chapter link as this source hands it out, with its tag.
    QString tagged(const QString &url) const;
    // "Chapter 12: Title" in the source's language, for adapters that have
    // only a number. Titles the site writes are used as they are.
    QString chapterLabel(const QString &number, const QString &title = {}) const;

    SourceConfig m_config;
};

std::unique_ptr<Source> createSource(const SourceConfig &config);

// Shared helpers for adapters.
QString str(const QJsonValue &value);
QString chapterNumberFrom(const QString &text);
QString chapterLabel(const QString &language, const QString &number, const QString &title = {});
void sortChapters(QList<Chapter> &chapters);
QString pathSegment(const QUrl &url, int index);

} // namespace sc::comics
