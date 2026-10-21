#pragma once

#include "Manager.h"
#include "sources/comics/ComicSource.h"

namespace sc::ext {

// A Mihon extension's source, run by the extension host. Its links are the
// extension's own, wrapped with the source key (see wrapUrl).
//
// Host calls (JSON in, JSON out; `source` is the source id as text):
//   search  {source, page, query, filters} -> {items: [{url, title, cover}], hasNextPage}
//   popular {source, page}                 -> the same
//   latest  {source, page}                 -> the same
//   details {source, url}                  -> {title, cover, description, author, artist, genres, status}
//   chapters {source, url}                 -> [{url, name, number, uploaded, scanlator}]
//   pages   {source, url}                  -> [{index, url, imageUrl}]
//   fetchImage {source, page, target}      -> writes the image to `target`
//   filters / preferences {source}          -> [...]; setPreference {source, key, value}
// Every call also names the extension (pkg, apk, signer), so the host
// loads it on first use. Pages are "extimage:<key>:<page>" panels: the image
// cache has the host fetch each one, through the extension's own client
// (rate limits, headers, unscrambling), straight into the cache file.
class ExtensionSource : public comics::Source {
public:
    explicit ExtensionSource(const SourceEntry &entry);

    bool owns(const QUrl &) const override { return false; }
    QList<comics::Summary> search(const QString &query) override;
    QList<comics::Chapter> chapters(const QString &seriesUrl) override;
    QList<comics::Panel> panels(const QString &chapterUrl) override;
    std::optional<comics::Details> details(const QString &seriesUrl) override;
    QString imageReferer() const override;
    // A series' page on its site: the source's base joined with the path.
    QString webUrl(const QString &url) const;

    bool canBrowse() const override { return true; }
    bool hasLatestListing() const override { return true; }
    comics::Listing browse(const QString &listing, int page, const QString &query, const QJsonArray &filters) override;
    QJsonArray filters() override;
    QJsonArray preferences() override;
    void setPreference(const QString &key, const QJsonValue &value) override;

    static QList<comics::Panel> parsePages(const QString &key, const QJsonArray &pages);
    static QString statusName(int status);
    // Writes the image of an extimage: panel to `path` (ImageCache).
    void fetchImage(const QJsonObject &page, const QString &path);
    // Lets the image cache fetch extimage: panels.
    static void installImageWriter();
    // After a browser check passed in SCANCORE's window: gives the site's
    // cookies to the extension host, when an extension uses that site.
    // Blocking.
    static void shareCookies(const QString &url);
    // The user agent to pass a site's browser check with, since Cloudflare
    // ties the clearance to it: the one an extension met the check with,
    // else the extensions' own for a site an extension reads, else SCANCORE's.
    static QString checkAgent(const QString &url);

private:
    QJsonValue call(const QString &method, QJsonObject params);
    QString unwrap(const QString &url) const;
    comics::Listing listing(const QJsonValue &result) const;

    SourceEntry m_entry;
};

} // namespace sc::ext
