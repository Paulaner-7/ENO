#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <memory>

namespace sc::catalogue {

// One title in a tracker's catalogue. Comics and novels opened from it keep
// the entry (series.extra), so tracking and comments know the title whichever
// source it is read on.
struct Entry {
    QString catalog;       // "comick", "anilist", "mal"
    QString id;            // the title's id in that catalogue
    QString media;         // "comic" or "novel"
    QString title;
    QStringList titles;    // every name the catalogue knows, title first
    QString cover;
    QString type;          // Manga, Manhwa, Manhua, Novel, Light novel
    QString status;        // Ongoing, Completed, Cancelled, Hiatus
    int year = 0;
    QString lastChapter;
    QString rating;
    QString description;
    QString slug;          // Comick's page name
    QString url;           // the title's page on the catalogue's site
    bool adult = false;
    // The same title on the trackers: {"comick": hid, "anilist": id, "mal": id}.
    QVariantMap ids;

    QVariantMap toMap() const;
    static Entry fromMap(const QVariantMap &map);
};

struct Shelf {
    // Shown under a translated title chosen by key: trending, popular, top,
    // newest, completed, manhwa, manhua.
    QString key;
    QList<Entry> entries;
};

// Browsing and "search everything" through a tracker's catalogue, chosen per
// media in Settings → Connections. Every call blocks; run on a worker.
class Catalogue {
public:
    virtual ~Catalogue() = default;

    virtual QString key() const = 0;
    virtual QString label() const = 0;
    // "comic" or "novel".
    virtual bool supports(const QString &media) const = 0;
    virtual QList<Shelf> shelves(const QString &media, bool mature) = 0;
    virtual QList<Entry> search(const QString &media, const QString &query, bool mature) = 0;
    // Every name of a title; shelf entries can carry only one.
    virtual QStringList names(const Entry &entry) { return entry.titles; }
};

// The catalogue the user browses a media with (settings "catalogComic" and
// "catalogNovel"): comick, anilist, mal or none. Safe on any thread.
QString chosen(const QString &media);

// "comick", "anilist" or "mal"; null for an unknown key or a catalogue this
// build cannot use (MyAnimeList without its client id).
std::shared_ptr<Catalogue> byKey(const QString &key);
// Where AniList says a title can be read (its "external links": official
// sites such as WEBTOON editions and Naver Webtoon). Cached for a week;
// empty when AniList is unreachable.
QStringList readingLinks(int anilistId);
// The catalogue for a media: the chosen one when it has that media, else
// AniList. Null when the user chose none.
std::shared_ptr<Catalogue> forMedia(const QString &chosen, const QString &media);
// MyAnimeList's public reads need SCANCORE's client id (set by main()).
void setMalClientId(const QString &id);
bool malAvailable();

// Parsers, public for the offline tests.
Entry comickEntry(const QJsonObject &comic);
Entry anilistEntry(const QJsonObject &media);
Entry malEntry(const QJsonObject &node);
// Comick's /top answer split into shelves, without erotica unless mature.
QList<Shelf> comickShelves(const QJsonObject &top, bool mature);

} // namespace sc::catalogue
