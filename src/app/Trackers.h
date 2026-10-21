#pragma once

#include "core/Task.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTcpServer>
#include <QVariantMap>

#include <memory>

namespace sc {

// Tracking on AniList, MyAnimeList, Comick, Kitsu, MangaUpdates, Shikimori,
// Bangumi and Simkl: a chapter or episode finished in
// SCANCORE moves that series forward on the user's list there. Anime know
// their AniList id already; comics and novels are matched by title, or
// linked by hand from their page. Each service needs SCANCORE's registered
// client id (SCANCORE_ANILIST_CLIENT_ID, SCANCORE_MAL_CLIENT_ID,
// SCANCORE_COMICK_CLIENT_ID at build time); without one it is hidden.
class Trackers : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool anilistAvailable READ anilistAvailable CONSTANT)
    Q_PROPERTY(bool malAvailable READ malAvailable CONSTANT)
    Q_PROPERTY(bool comickAvailable READ comickAvailable CONSTANT)
    // {name, avatar} of the connected account, or empty.
    Q_PROPERTY(QVariantMap anilist READ anilist NOTIFY changed)
    Q_PROPERTY(QVariantMap mal READ mal NOTIFY changed)
    Q_PROPERTY(QVariantMap comick READ comick NOTIFY changed)
    // Every tracker this build has, in display order: [{key, label, media
    // (["comic", "novel", "anime"]), auth ("token", "oauth", "password",
    // "pin"), site, name, avatar, expired, connected}].
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)
    Q_PROPERTY(bool anyConnected READ anyConnected NOTIFY changed)
    // Simkl's sign-in: {code, url} to enter while it waits, or empty.
    Q_PROPERTY(QVariantMap pin READ pin NOTIFY changed)

public:
    static Trackers *instance();
    static Trackers *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool anilistAvailable() const;
    bool malAvailable() const;
    bool comickAvailable() const;
    QVariantMap anilist() const;
    QVariantMap mal() const;
    QVariantMap comick() const;
    QVariantList services() const;
    bool anyConnected() const;
    QVariantMap pin() const { return m_pin; }
    // {name, avatar, expired} of a connected service, or empty.
    Q_INVOKABLE QVariantMap account(const QString &service) const;

    // AniList: opens its sign-in page, which shows a token to paste here.
    Q_INVOKABLE void anilistSignIn() const;
    // AniList's sign-in page for SCANCORE (it shows a token when allowed).
    Q_INVOKABLE QString anilistAuthorizeUrl() const;
    Q_INVOKABLE sc::Task *anilistUseToken(const QString &token);
    // MyAnimeList: opens its sign-in page; it comes back to SCANCORE itself.
    Q_INVOKABLE sc::Task *malSignIn();
    // Comick: the same, with its own consent page.
    Q_INVOKABLE sc::Task *comickSignIn();
    // Any service that signs in on a page of its own (MyAnimeList, Comick,
    // Shikimori) or with a code (Simkl).
    Q_INVOKABLE sc::Task *signIn(const QString &service);
    // Kitsu and MangaUpdates: their sign-in is a user name and password,
    // sent only to them.
    Q_INVOKABLE sc::Task *passwordSignIn(const QString &service, const QString &user, const QString &password);
    // AniList and Bangumi: a token their page shows.
    Q_INVOKABLE sc::Task *useToken(const QString &service, const QString &token);
    Q_INVOKABLE QString tokenPage(const QString &service) const;
    // The page of a linked entry on its service.
    Q_INVOKABLE QString entryUrl(const QString &service, const QString &media, const QVariant &id) const;
    Q_INVOKABLE void signOut(const QString &service);

    // Titles on a service for linking a series: [{id, title, detail, cover}].
    Q_INVOKABLE sc::Task *search(const QString &service, const QString &media, const QString &title);
    // {anilist: {id, title}, mal: {id, title}, comick: {id, title}} for a
    // series. Comick's ids are short text.
    Q_INVOKABLE QVariantMap links(const QString &seriesId) const;
    // Connected services that could not find the series by its titles the
    // last time progress was sent ("anilist", "mal", "comick").
    Q_INVOKABLE QStringList misses(const QString &seriesId) const;
    // The best name to search the trackers with: the catalogue's title when
    // the series was opened from one, else its own.
    Q_INVOKABLE QString lookupTitle(const QString &seriesId, const QString &title) const;
    Q_INVOKABLE void link(const QString &seriesId, const QString &service, const QVariant &remoteId, const QString &title);
    Q_INVOKABLE void unlink(const QString &seriesId, const QString &service);
    // Full AniList details: synopsis, score, tags, format, user tracking state.
    Q_INVOKABLE sc::Task *anilistDetails(const QString &seriesId, const QString &media, const QString &title);

    // A chapter or episode of this series was finished (Library).
    static void unitFinished(const QString &seriesId);
    // A series moved to another source: its links (and misses) go with it.
    static void moveLinks(const QString &fromId, const QString &toId, bool keepOld);
    // Links a series without sending its progress (imports).
    static void remember(const QString &seriesId, const QString &service, const QVariant &remoteId, const QString &title);
    // Everything the connected Comick account follows (blocking; imports).
    static QVariantList comickLibrary();
    // The AniList entry of a Comick title, or 0.
    static int comickAniListId(const QString &hid);

signals:
    void changed();

private:
    explicit Trackers(QObject *parent = nullptr);
    // Starts the local page a sign-in comes back to (http://127.0.0.1:52817
    // + path); returns its waiting state, or nothing and why not.
    std::shared_ptr<void> listen(const QString &path, QString *problem);
    // Closes that page once the sign-in task ends.
    void watch(sc::Task *task);

    QPointer<QTcpServer> m_redirect;
    std::shared_ptr<void> m_pending;
    QString m_state;
    QVariantMap m_pin;
};

} // namespace sc
