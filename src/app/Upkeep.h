#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>

namespace sc {

// A library that looks after itself: sources that stopped answering or
// updating (with a move to suggest), the same series saved twice (to
// merge), and one calendar of what comes out next across comics, novels
// and anime.
class Upkeep : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Series needing a look plus duplicate groups, for the Library banner.
    Q_PROPERTY(int attention READ attention NOTIFY changed)

public:
    static Upkeep *instance();
    static Upkeep *create(QQmlEngine *, QJSEngine *) { return instance(); }

    int attention() const { return m_attention; }

    // {sources: [{source, label, count, days, series: [...]}],
    //  stalled: [...], failing: [...], duplicates: [[...], ...]}; each
    // series {id, media, title, cover, source, sourceLabel, sourceUrl,
    // read, units, days, problem, extra}.
    Q_INVOKABLE sc::Task *report();
    // Moves the others' progress, bookmarks and links into `keepId` and
    // takes them out of the library; their sources are remembered as
    // alternatives for the kept series.
    Q_INVOKABLE sc::Task *merge(const QString &keepId, const QStringList &otherIds);
    // Other sources a merged series was on: [{id, source, sourceLabel, title}].
    Q_INVOKABLE QVariantList alternatives(const QString &seriesId) const;
    // The next `days` days: [{day: "yyyy-MM-dd", items: [{media, title,
    // cover, at, label, unit, seriesId, anilistId, guess, late}]}].
    Q_INVOKABLE sc::Task *radar(int days = 14);
    // Counts again (after checks, merges and moves).
    Q_INVOKABLE void recount();
    // "Not a duplicate": that group stays apart.
    Q_INVOKABLE void keepApart(const QStringList &ids);

signals:
    void changed();

private:
    explicit Upkeep(QObject *parent = nullptr);
    int m_attention = 0;
    bool m_counting = false;
};

} // namespace sc
