#pragma once

#include "sources/comics/ComicSource.h"

#include <QJsonArray>
#include <QSet>

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

// WEBTOON: one edition per source (option "edition": the site's language
// path, "en" by default; "es", "fr", "de", "id", "th", "zh-hant").
class WebtoonSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QString imageReferer() const override { return QStringLiteral("https://www.webtoons.com/"); }
    // Only its edition's links (the path starts with the language).
    bool owns(const QUrl &url) const override;
    QString seriesLink(const QUrl &url) const override;

private:
    QString edition() const;
};

// Dongman Manhua (Simplified Chinese WEBTOON): the same viewer and series
// pages, with its own search and paged episode lists.
class DongmanSource : public WebtoonSource {
public:
    using WebtoonSource::WebtoonSource;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QString imageReferer() const override { return m_config.base + QLatin1Char('/'); }
    QString seriesLink(const QUrl &) const override { return {}; }

    static QList<Summary> parseSearch(const QByteArray &html, const QString &base, const QString &source);
    static QList<Chapter> parseEpisodes(const QByteArray &html, const QString &base, const QString &language);
};

class MangaDexSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    bool readable(const QString &seriesUrl) override;

    // The readable chapters of one feed page. `takenDown` counts chapters
    // MangaDex lists but no longer serves (licensed, "isUnavailable").
    QList<Chapter> feedChapters(const QJsonArray &rows, QSet<QString> &seen, int *takenDown) const;

    // A title's names in other languages ({language code, name}), for
    // finding it on sites that list it under a translated name. The title
    // is the one whose AniList or MyAnimeList id matches (when given), else
    // the one named `title` exactly. Cached for a week.
    QList<QPair<QString, QString>> otherNames(const QString &title, int anilist, int mal);

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
// Naver Webtoon (Korean): the site's own JSON API and viewer pages. Lists
// the episodes anyone can read without an account (not paid previews or
// adult titles, which need a login).
class NaverSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QString imageReferer() const override { return m_config.base + QLatin1Char('/'); }
    QString seriesLink(const QUrl &url) const override;

    static QList<Chapter> parseChapters(const QJsonObject &page, const QString &base);
    static QList<Panel> parsePanels(const QByteArray &html);
};

// Kakao Page (Korean): the site's own API. Lists the episodes it shows
// without an account (free ones; wait-for-free and paid ones need a login,
// as do adult titles).
class KakaoPageSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    bool readable(const QString &seriesUrl) override;
    QString seriesLink(const QUrl &url) const override;

    static QString image(const QString &kid, const QString &size);
    // Free episodes of one list page, numbered by their "<n>화" when they
    // have one.
    static QList<Chapter> parseEpisodes(const QJsonArray &rows, const QString &base, const QString &seriesId, const QString &seriesTitle);

private:
    QJsonObject api(const QString &path, const QList<QPair<QString, QString>> &params);
};

// ZonaTMO (Spanish): the site's JSON API ("api" = <site>/wp-api/api) and
// its image CDN (cdn.<site>).
class ZonaTmoSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

    static QList<Summary> parseSearch(const QJsonObject &root, const QString &base, const QString &source, bool adult);
    static QList<Chapter> parseChapters(const QJsonArray &rows, const QString &base, const QString &slug, const QString &language);
};

// Olympus Scanlation (Spanish): a JSON API on the site. Series links carry
// the series id ("#<id>") because the site renames its slugs now and then.
class OlympusSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

    static QList<Panel> parsePanels(const QByteArray &html);

private:
    // Every comic: id -> {slug, name, cover}, cached for an hour.
    QJsonArray seriesList(bool fresh = false);
    // The series' current slug, looked up by id when the link's one is old.
    QString currentSlug(const QString &seriesUrl, bool fresh);
};

// ZeroComics sites (LectorManga.lat, Spanish): a JSON API ("api").
class ZeroComicsSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

private:
    QJsonObject series(const QString &seriesUrl);
};

// Kagane: the site's JSON API (<base>/api/v2), which answers without a
// browser check. Series are <base>/series/<id>, chapters
// <base>/series/<id>/reader/<book id>. Options: "languages" (the content
// languages to list; default the source's language), "adult" (adult titles
// too).
class KaganeSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QList<LatestItem> latest(int limit) override;
    bool hasLatest() const override { return true; }

    QList<Summary> parseSearch(const QJsonObject &root) const;
    QList<LatestItem> parseLatest(const QJsonObject &root) const;
    // The compact chapter index (/books/index?fields=List).
    QList<Chapter> parseChapters(const QJsonObject &index, const QString &seriesId) const;
    Details parseDetails(const QJsonObject &series) const;
    // A chapter's pages from the answer to its page request.
    static QList<Panel> parsePanels(const QJsonObject &book, const QString &bookId);

private:
    QJsonObject api(const QByteArray &method, const QString &path, const QList<QPair<QString, QString>> &params = {},
                    const QByteArray &body = {}, const Headers &extra = {});
    QByteArray searchBody(const QString &query) const;
    QString integrityToken(bool fresh);
};

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
