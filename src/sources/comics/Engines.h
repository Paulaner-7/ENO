#pragma once

#include "ComicSource.h"

namespace sc::comics {

class AsuraSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QList<LatestItem> latest(int limit) override;
    bool hasLatest() const override { return true; }
};

class WebtoonSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QString imageReferer() const override { return QStringLiteral("https://www.webtoons.com/"); }
};

class MangaDexSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

private:
    // Translations to list (option "languages", default English).
    QStringList languages() const;
    QJsonDocument api(const QString &path, const QList<QPair<QString, QString>> &params, bool atHome = false);
};

class FlameSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
};

// Iken-based sites (Vortex) expose a JSON API next to the site.
class IkenSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QList<LatestItem> latest(int limit) override;
    bool hasLatest() const override { return true; }

protected:
    QString postId(const QUrl &seriesUrl);
};

class HiveSource : public IkenSource {
public:
    using IkenSource::IkenSource;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    QList<LatestItem> latest(int) override { return {}; }
    bool hasLatest() const override { return false; }
};

class QiMangaSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QList<LatestItem> latest(int limit) override;
    bool hasLatest() const override { return true; }
};

// ManhwaWeb: a JSON API next to the site (Spanish).
class ManhwaWebSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

    // Chapters of /manhwa/see/<id>, linked under `base`.
    static QList<Chapter> parseChapters(const QJsonObject &series, const QString &id, const QString &base,
                                        const QString &language);

private:
    QJsonObject series(const QString &seriesUrl);
};

// InManga (Spanish): search and chapter pages through the site's own AJAX
// endpoints, the chapter list as JSON and pages from its image CDN.
class InMangaSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

    static QList<Summary> parseSearch(const QByteArray &html, const QString &base);
    // Page images of a chapter, from its page picker.
    static QList<Panel> parsePages(const QByteArray &html, const QString &imageHost, const QString &chapterId);
};

// LeerCapitulo (Spanish): plain pages.
class LeerCapituloSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

    static QList<Summary> parseSearch(const QByteArray &html, const QString &base);
    static QList<Chapter> parseChapters(const QByteArray &html, const QString &pageUrl);
    static QList<Panel> parsePanels(const QByteArray &html, const QString &pageUrl);
};

// MangaWorld (Italian): server-rendered archive and series pages, and a
// reader that lists every page at once when asked with ?style=list.
class MangaWorldSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QList<LatestItem> latest(int limit) override;
    bool hasLatest() const override { return true; }

    static QList<Summary> parseCards(const QByteArray &html, const QString &base);
    static QList<Chapter> parseChapters(const QByteArray &html, const QString &pageUrl);
    static QList<Panel> parsePanels(const QByteArray &html, const QString &pageUrl);
};

// WordPress manga themes (MangaReader/MangaStream).
class WpSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

protected:
    QList<Summary> parseSearch(const QByteArray &html) const;
    QList<Summary> searchHtml(const QString &query);
    QList<Chapter> parseChapterList(const QByteArray &html, const QString &pageUrl) const;
};

// The MangaVerse WordPress theme (MangaOni): its own search cards and a
// chapter list loaded ten at a time.
class MangaverseSource : public WpSource {
public:
    using WpSource::WpSource;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

    // Series titles end with the edition (" ES"), named by `titleSuffix`.
    static QList<Summary> parseCards(const QByteArray &html, const QString &base, const QString &titleSuffix);
    static QList<Chapter> parseChapters(const QByteArray &html, const QString &pageUrl);
    static QString cleanTitle(QString title, const QString &suffix);
};

// The Madara WordPress theme.
class MadaraSource : public WpSource {
public:
    using WpSource::WpSource;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
};

} // namespace sc::comics
