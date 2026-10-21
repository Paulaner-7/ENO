#pragma once

#include "Runtime.h"
#include "extensions/Manager.h"
#include "sources/novels/NovelSource.h"

namespace sc::plugins {

// A novel source from an installed LNReader plugin, run by the plugin
// runtime. Its links are the plugin's paths wrapped with the source key
// ("lnreader:<id>:<path>"), like extension comics.
//
// Plugin methods used: popularNovels(page, {showLatestNovels, filters}),
// searchNovels(term, page), parseNovel(path) (+ parsePage(path, page) for
// books whose chapters come in pages), parseChapter(path); its `filters`
// and `pluginSettings` become SCANCORE's filter sheet and source settings.
class PluginSource : public novels::Source {
public:
    explicit PluginSource(const ext::SourceEntry &entry);

    bool owns(const QUrl &) const override { return false; }
    QList<novels::Book> search(const QString &query) override;
    novels::Book book(const QString &url, QList<novels::ChapterRef> &chapters) override;
    QList<novels::Block> chapter(const QString &url, QString &title) override;

    bool canBrowse() const override { return true; }
    bool hasLatestListing() const override { return true; }
    novels::Listing browse(const QString &listing, int page, const QString &query, const QJsonArray &filters) override;
    QJsonArray filters() override;
    QJsonArray preferences() override;
    void setPreference(const QString &key, const QJsonValue &value) override;
    QString imageReferer() const override;
    // The book's or chapter's page on the site (the plugin's resolveUrl, or
    // its site joined with the path). Blocking.
    QString webUrl(const QString &url, bool novel = true);

    // LNReader's filter declarations in comics' filter shape, and the
    // states of that shape back into LNReader's {key: {type, value}}.
    static QJsonArray toFilterSheet(const QJsonObject &filters);
    static QJsonObject fromFilterSheet(const QJsonObject &filters, const QJsonArray &states);

private:
    QJsonValue call(const QString &method, const QJsonArray &args = {});
    QString unwrap(const QString &url) const;
    novels::Book bookFrom(const QJsonObject &item) const;

    ext::SourceEntry m_entry;
    PluginRef m_plugin;
};

} // namespace sc::plugins
