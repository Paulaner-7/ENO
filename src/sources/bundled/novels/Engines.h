#pragma once

#include "sources/novels/NovelSource.h"

namespace sc::novels {

class RoyalRoad : public Source {
public:
    using Source::Source;
    QList<Book> search(const QString &query) override;
    Book book(const QString &url, QList<ChapterRef> &chapters) override;
    QList<Block> chapter(const QString &url, QString &title) override;
    QList<Shelf> discover() override;
    bool hasDiscover() const override { return true; }

private:
    QList<Book> listing(const QByteArray &html, int limit);
};

// The NovelFull family. These sites run either the classic NovelFull theme
// or the newer FreeWebNovel ("fwn") theme, and several have switched between
// them, so each page is parsed by whichever layout it actually has.
class NovelFull : public Source {
public:
    using Source::Source;
    QList<Book> search(const QString &query) override;
    Book book(const QString &url, QList<ChapterRef> &chapters) override;
    QList<Block> chapter(const QString &url, QString &title) override;

private:
    QList<Book> parseSearch(const QByteArray &html);
    Book classicBook(const HtmlDocument &doc, const QByteArray &raw, const QString &url, QList<ChapterRef> &chapters);
    Book fwnBook(const HtmlDocument &doc, const QString &url, QList<ChapterRef> &chapters);
    QList<Block> classicChapter(const HtmlDocument &doc, QString &title);
    QList<Block> fwnChapter(const HtmlDocument &doc, QString &title);
};

// WTR-LAB: machine-translated web novels, read through the site's own JSON
// (search, lists, chapter list) and its reader's English AI translation.
// Locked chapters stay locked; its human check opens in the browser view.
class WtrLab : public Source {
public:
    using Source::Source;
    QList<Book> search(const QString &query) override;
    Book book(const QString &url, QList<ChapterRef> &chapters) override;
    QList<Block> chapter(const QString &url, QString &title) override;
    QList<Shelf> discover() override;
    bool hasDiscover() const override { return true; }

private:
    QString novelUrl(qint64 rawId, const QString &slug) const;
    Book fromSerie(const QJsonObject &serie) const;
    QList<Book> listing(const QString &order, int page);
};

} // namespace sc::novels
