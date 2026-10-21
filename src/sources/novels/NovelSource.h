#pragma once

#include "core/Html.h"
#include "core/Http.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QVariantMap>

#include <memory>

namespace sc::novels {

struct Book {
    QString source;
    QString url;
    QString title;
    QString author;
    QString cover;
    QString description;
    QString status;
    QString rating;
    QStringList genres;
    int chapterCount = 0;
    QVariantMap toMap() const;
};

struct ChapterRef {
    QString url;
    QString title;
    QString number;
};

// One paragraph of chapter text. `html` carries only the escaped text with
// <b>, <i>, <u> and <s>; provider markup never reaches the reader.
struct Block {
    QString kind; // paragraph | heading | quote | break
    QString text;
    QString html;
};

struct Shelf {
    QString title;
    QList<Book> books;
};

// One page of a source's own lists (see Source::browse).
struct Listing {
    QList<Book> items;
    bool hasNextPage = false;
};

struct SourceConfig {
    QString key;
    QString label;
    QString engine;
    QString base;
    QStringList hosts;
    bool enabled = true;
    bool searchable = true;
    QString disabledReason;
    QJsonObject options;
    // Language of the content and the tag that tells this source's links
    // apart from another source on the same host (see comics::SourceConfig).
    QString language = QStringLiteral("en");
    QString urlTag;
};

class Source {
public:
    explicit Source(SourceConfig config) : m_config(std::move(config)) {}
    virtual ~Source() = default;

    const SourceConfig &config() const { return m_config; }
    QString key() const { return m_config.key; }
    QString label() const { return m_config.label; }
    virtual bool owns(const QUrl &url) const;

    virtual QList<Book> search(const QString &query) = 0;
    virtual Book book(const QString &url, QList<ChapterRef> &chapters) = 0;
    virtual QList<Block> chapter(const QString &url, QString &title) = 0;
    virtual QList<Shelf> discover() { return {}; }
    virtual bool hasDiscover() const { return false; }

    // A source's own lists, a page at a time (plugins): "popular",
    // "latest" or "search" (with `query` and the states of its filters, in
    // the shape comics' filters use).
    virtual bool canBrowse() const { return false; }
    virtual bool hasLatestListing() const { return false; }
    virtual Listing browse(const QString &listing, int page, const QString &query, const QJsonArray &filters) {
        Q_UNUSED(listing)
        Q_UNUSED(page)
        Q_UNUSED(query)
        Q_UNUSED(filters)
        return {};
    }
    virtual QJsonArray filters() { return {}; }
    // Settings the source offers ([{key, type, title, summary, value, values,
    // entries}], as comics' extensions); setPreference stores one.
    virtual QJsonArray preferences() { return {}; }
    virtual void setPreference(const QString &key, const QJsonValue &value) {
        Q_UNUSED(key)
        Q_UNUSED(value)
    }
    // What cover requests send as Referer (empty: none).
    virtual QString imageReferer() const { return {}; }

protected:
    Response page(const QString &url);
    QString origin() const { return m_config.base.endsWith(QLatin1Char('/')) ? m_config.base : m_config.base + QLatin1Char('/'); }

    SourceConfig m_config;
};

std::unique_ptr<Source> createSource(const SourceConfig &config);

// Converts a chapter container into reader blocks.
QList<Block> textBlocks(const HtmlNode &container);
QString imgSrc(const HtmlNode &img, const QString &base);

} // namespace sc::novels
