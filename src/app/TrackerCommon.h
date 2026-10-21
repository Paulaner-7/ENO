#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

#include <functional>

// Shared by Trackers.cpp (AniList, MyAnimeList, Comick) and
// MoreTrackers.cpp (Kitsu, MangaUpdates, Shikimori, Bangumi, Simkl).
namespace sc::tracking {

// {seriesId: {service: {id, title}}}
QVariantMap allLinks();
// Ids are numbers on most services, and short text on Comick.
bool hasId(const QVariant &id);
void saveLink(const QString &seriesId, const QString &service, const QVariant &id, const QString &title);
void saveMiss(const QString &seriesId, const QString &service, bool missed);
bool missedRecently(const QString &seriesId, const QString &service);
QJsonObject seriesExtra(const QString &seriesId);
QStringList lookupTitles(const QString &seriesId, const QString &title);
QVariantMap exactMatch(const QVariantList &candidates, const QString &title);
QVariantMap matchByTitles(const std::function<QVariantList(const QString &)> &search, const QStringList &names);
// The series' AniList entry when SCANCORE knows it, or 0.
int anilistIdOf(const QString &seriesId);
// AniList titles: [{id, idMal, title, titles, detail, cover, total}].
QVariantList anilistSearch(const QString &media, const QString &title);
// How far the series is: the highest chapter or episode number finished.
int progressOf(const QString &seriesId);

// A service added in 2.3.0. search() gives [{id, title, titles, detail,
// cover, total}] like the others; push() moves the entry on the signed-in
// account forward to `progress` (nothing when it is already there).
struct Service {
    QString key;
    QString label;
    QStringList media;
    std::function<bool()> available;
    std::function<QVariantList(const QString &media, const QString &title)> search;
    std::function<void(const QVariant &id, const QString &media, int progress)> push;
};
const QList<Service> &services();
const Service *service(const QString &key);

// Signs in and stores the account; returns {name}. Blocking.
QVariantMap passwordSignIn(const QString &service, const QString &user, const QString &password);
QVariantMap tokenSignIn(const QString &service, const QString &token);
// Shikimori: the code its sign-in page came back with.
QVariantMap shikimoriCode(const QString &code, const QString &redirect);
QString shikimoriAuthorizeUrl(const QString &redirect, const QString &state);
// Simkl: a code to enter on simkl.com/pin, then waits for it (polls).
QVariantMap simklPin();
QVariantMap simklWait(const QString &userCode, int seconds, int interval);

// Moves the series forward on one of these services, matching it by title
// the first time. Throws Error (401 when the service signed SCANCORE out).
void update(const Service &service, const QString &seriesId, const QString &media, const QString &title, int progress);

} // namespace sc::tracking
