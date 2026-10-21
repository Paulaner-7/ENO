#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

namespace sc {

// One story across comic, novel and anime. AniList's relations give a
// series' other forms (the manhwa an anime adapts, the web novel behind it,
// every season); scancore.onl's readers add where each anime season leaves
// off in the comic or novel ("points"), voted on like comments. Home turns
// both into "Continue the story" cards.
class Story : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // [{key, kind, title, text, cover, media, chapter, volume, seriesId,
    //   entry, animeId, done}]: what to continue next, newest first.
    Q_PROPERTY(QVariantList cards READ cards NOTIFY cardsChanged)

public:
    static Story *instance();
    static Story *create(QQmlEngine *, QJSEngine *) { return instance(); }

    QVariantList cards() const { return m_cards; }

    // {anilistId, forms: [{media, anilistId, title, cover, format, status,
    // year, total, volumes, relation, seriesId, saved, done, entry}]} for a
    // series: its other forms, with your progress in each. A comic or novel
    // without a known AniList entry is matched by its titles.
    Q_INVOKABLE sc::Task *forms(const QString &seriesId, const QString &media, const QString &title, int anilistId = 0);
    // {animeId: [{id, media, sourceId, chapter, volume, score, seeded,
    // mine}]}: where each anime leaves off, best first.
    Q_INVOKABLE sc::Task *points(const QVariantList &animeIds);
    // Adds a point (or votes for the same one), signed in to the site.
    Q_INVOKABLE sc::Task *submit(int animeId, const QString &media, int sourceId, double chapter, int volume);
    // value: 1, -1, or 0 to take the vote back.
    Q_INVOKABLE sc::Task *vote(int animeId, const QString &pointId, int value);
    // Works the cards out again in the background (Home calls it).
    Q_INVOKABLE void refreshCards();
    Q_INVOKABLE void dismiss(const QString &key);

signals:
    void cardsChanged();

private:
    explicit Story(QObject *parent = nullptr);
    QVariantList m_cards;
    bool m_refreshing = false;
};

} // namespace sc
