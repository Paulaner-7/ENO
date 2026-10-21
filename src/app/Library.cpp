#include "Library.h"

#include "Activity.h"
#include "Comics.h"
#include "Novels.h"
#include "Trackers.h"
#include "core/Database.h"
#include "core/Parallel.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace sc {

namespace {

QString json(const QVariant &value) {
    if (!value.isValid() || value.isNull())
        return {};
    return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
}

QVariant parse(const QVariant &text) {
    const auto doc = QJsonDocument::fromJson(text.toByteArray());
    return doc.isNull() ? QVariant() : doc.toVariant();
}

QVariantMap shapeEntry(QVariantMap row) {
    row.insert(QStringLiteral("position"), parse(row.value(QStringLiteral("position"))));
    row.insert(QStringLiteral("extra"), parse(row.value(QStringLiteral("extra"))));
    row.insert(QStringLiteral("genres"),
               row.value(QStringLiteral("genres")).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts));
    return row;
}

const QString kEntrySelect = QStringLiteral(R"(
    SELECT s.id, s.media, s.source, s.source_url AS sourceUrl, s.title, s.cover, s.description,
           s.author, s.status, s.genres, s.extra,
           l.category, l.added_at AS addedAt, l.new_units AS newUnits, l.known_units AS knownUnits,
           l.latest_title AS latestTitle,
           p.unit_key AS unitKey, p.unit_title AS unitTitle, p.unit_number AS unitNumber, p.unit_url AS unitUrl,
           p.position, p.percent, p.completed, p.updated_at AS progressAt
)");

} // namespace

Library::Library(QObject *parent) : QObject(parent) {}

Library *Library::instance() {
    static auto *library = [] {
        auto *created = new Library;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return library;
}

void Library::notifyChanged() {
    ++m_revision;
    emit changed();
}

QString Library::idFor(const QString &media, const QString &source, const QString &key) {
    return QStringLiteral("%1:%2:%3").arg(media, source, key);
}

int Library::unreadUpdates() const {
    return Database::scalar(QStringLiteral("SELECT COALESCE(SUM(new_units), 0) FROM library")).toInt();
}

bool Library::isSaved(const QString &seriesId) const {
    return Database::scalar(QStringLiteral("SELECT 1 FROM library WHERE series_id = ?"), {seriesId}).isValid();
}

QString Library::category(const QString &seriesId) const {
    return Database::scalar(QStringLiteral("SELECT category FROM library WHERE series_id = ?"), {seriesId}).toString();
}

void Library::upsertSeries(const QVariantMap &s) {
    const auto id = s.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;
    auto genres = s.value(QStringLiteral("genres"));
    const auto genreText = genres.typeId() == QMetaType::QStringList || genres.typeId() == QMetaType::QVariantList
                               ? genres.toStringList().join(QStringLiteral(", "))
                               : genres.toString();
    // COALESCE keeps richer details already stored when a sparse summary
    // (for example from a search result) is saved again.
    Database::exec(QStringLiteral(R"(
        INSERT INTO series(id, media, source, source_url, title, cover, description, author, status, genres, extra, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            title = COALESCE(NULLIF(excluded.title, ''), series.title),
            cover = COALESCE(NULLIF(excluded.cover, ''), series.cover),
            description = COALESCE(NULLIF(excluded.description, ''), series.description),
            author = COALESCE(NULLIF(excluded.author, ''), series.author),
            status = COALESCE(NULLIF(excluded.status, ''), series.status),
            genres = COALESCE(NULLIF(excluded.genres, ''), series.genres),
            extra = COALESCE(NULLIF(excluded.extra, ''), series.extra),
            updated_at = excluded.updated_at
    )"),
                   {id, s.value(QStringLiteral("media")).toString(), s.value(QStringLiteral("source")).toString(),
                    s.value(QStringLiteral("sourceUrl")).toString(), s.value(QStringLiteral("title")).toString(),
                    s.value(QStringLiteral("cover")).toString(), s.value(QStringLiteral("description")).toString(),
                    s.value(QStringLiteral("author")).toString(), s.value(QStringLiteral("status")).toString(),
                    genreText, json(s.value(QStringLiteral("extra"))), Database::now()});
}

void Library::storeUnits(const QString &seriesId, const QVariantList &units) {
    if (units.isEmpty())
        return;
    Database::transaction([&] {
        Database::query(QStringLiteral("DELETE FROM units WHERE series_id = ?"), {seriesId});
        int index = 0;
        for (const auto &value : units) {
            const auto u = value.toMap();
            Database::query(QStringLiteral("INSERT OR REPLACE INTO units(series_id, unit_key, idx, number, title, url, "
                                           "released_at) VALUES(?, ?, ?, ?, ?, ?, ?)"),
                            {seriesId, u.value(QStringLiteral("key")).toString(), index++,
                             u.value(QStringLiteral("number")).toString(), u.value(QStringLiteral("title")).toString(),
                             u.value(QStringLiteral("url")).toString(), u.value(QStringLiteral("released")).toString()});
        }
    });
}

QVariantList Library::cachedUnits(const QString &seriesId) {
    return Database::query(QStringLiteral("SELECT unit_key AS key, number, title, url, released_at AS released "
                                          "FROM units WHERE series_id = ? ORDER BY idx"),
                           {seriesId});
}

void Library::save(const QVariantMap &series, const QString &category) {
    upsertSeries(series);
    const auto id = series.value(QStringLiteral("id")).toString();
    const auto known = Database::scalar(QStringLiteral("SELECT COUNT(*) FROM units WHERE series_id = ?"), {id}).toInt();
    Database::transaction([&] {
        Database::query(QStringLiteral("INSERT INTO library(series_id, category, added_at, known_units, changed_at) "
                                       "VALUES(?, ?, ?, ?, ?) ON CONFLICT(series_id) DO UPDATE SET "
                                       "category = excluded.category, changed_at = excluded.changed_at"),
                        {id, category, Database::now(), known, Database::now()});
        Database::query(QStringLiteral("DELETE FROM library_removed WHERE series_id = ?"), {id});
    });
    notifyChanged();
    emit saved(id);
}

void Library::remove(const QString &seriesId) {
    // The removal is kept so file sync can remove it on other devices too.
    Database::transaction([&] {
        Database::query(QStringLiteral("DELETE FROM library WHERE series_id = ?"), {seriesId});
        Database::query(QStringLiteral("INSERT OR REPLACE INTO library_removed(series_id, removed_at) VALUES(?, ?)"),
                        {seriesId, Database::now()});
    });
    notifyChanged();
    emit removed(seriesId);
}

void Library::setCategory(const QString &seriesId, const QString &category) {
    Database::exec(QStringLiteral("UPDATE library SET category = ?, changed_at = ? WHERE series_id = ?"),
                   {category, Database::now(), seriesId});
    notifyChanged();
}

void Library::markSeen(const QString &seriesId) {
    Database::exec(QStringLiteral("UPDATE library SET new_units = 0, known_units = "
                                  "(SELECT COUNT(*) FROM units WHERE series_id = ?) WHERE series_id = ? AND new_units > 0"),
                   {seriesId, seriesId});
    notifyChanged();
}

Task *Library::entries(const QString &media) {
    return Task::run([media]() -> QVariant {
        const auto rows = Database::query(kEntrySelect + QStringLiteral(R"(
            FROM library l JOIN series s ON s.id = l.series_id
            LEFT JOIN progress p ON p.series_id = s.id AND p.updated_at =
                (SELECT MAX(updated_at) FROM progress WHERE series_id = s.id)
            WHERE (? = '' OR s.media = ?)
            GROUP BY s.id
            ORDER BY l.new_units > 0 DESC, COALESCE(p.updated_at, l.added_at) DESC
        )"),
                                          {media, media});
        QVariantList out;
        for (const auto &row : rows)
            out.append(shapeEntry(row.toMap()));
        return out;
    });
}

Task *Library::history(const QString &media, int limit) {
    return Task::run([media, limit]() -> QVariant {
        const auto rows = Database::query(kEntrySelect + QStringLiteral(R"(
            FROM progress p JOIN series s ON s.id = p.series_id
            LEFT JOIN library l ON l.series_id = s.id
            WHERE (? = '' OR s.media = ?) AND p.updated_at =
                (SELECT MAX(updated_at) FROM progress WHERE series_id = s.id)
            GROUP BY s.id
            ORDER BY p.updated_at DESC LIMIT ?
        )"),
                                          {media, media, limit});
        QVariantList out;
        for (const auto &row : rows)
            out.append(shapeEntry(row.toMap()));
        return out;
    });
}

Task *Library::continueItems(const QString &media, int limit) { return history(media, limit); }

void Library::forget(const QString &seriesId) {
    Database::exec(QStringLiteral("DELETE FROM progress WHERE series_id = ?"), {seriesId});
    notifyChanged();
}

void Library::clearHistory(const QString &media) {
    Database::exec(QStringLiteral("DELETE FROM progress WHERE (? = '' OR series_id LIKE ? || ':%')"), {media, media});
    notifyChanged();
}

void Library::recordProgress(const QVariantMap &series, const QVariantMap &unit, const QVariantMap &position,
                             double percent, bool completed) {
    const auto seriesId = series.value(QStringLiteral("id")).toString();
    const auto key = unit.value(QStringLiteral("key")).toString();
    if (seriesId.isEmpty() || key.isEmpty())
        return;
    upsertSeries(series);
    const bool wasCompleted =
        Database::scalar(QStringLiteral("SELECT completed FROM progress WHERE series_id = ? AND unit_key = ?"), {seriesId, key})
            .toBool();
    Database::exec(QStringLiteral(R"(
        INSERT INTO progress(series_id, unit_key, unit_title, unit_number, unit_url, position, percent, completed, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(series_id, unit_key) DO UPDATE SET
            unit_title = excluded.unit_title, unit_number = excluded.unit_number, unit_url = excluded.unit_url,
            position = CASE
                WHEN excluded.completed OR excluded.percent >= progress.percent OR progress.position IS NULL THEN excluded.position
                ELSE progress.position
            END,
            percent = MAX(progress.percent, excluded.percent),
            completed = MAX(progress.completed, excluded.completed),
            updated_at = excluded.updated_at
    )"),
                   {seriesId, key, unit.value(QStringLiteral("title")).toString(),
                    unit.value(QStringLiteral("number")).toString(), unit.value(QStringLiteral("url")).toString(),
                    json(position), std::clamp(percent, 0.0, 1.0), completed ? 1 : 0, Database::now()});
    // Finished just now, by reading or watching (not marked by hand).
    if (completed && !wasCompleted) {
        Activity::unitFinished(seriesId.section(QLatin1Char(':'), 0, 0));
        Trackers::unitFinished(seriesId);
    }
    notifyChanged();
}

void Library::markUnits(const QVariantMap &series, const QVariantList &units, bool completed) {
    const auto seriesId = series.value(QStringLiteral("id")).toString();
    upsertSeries(series);
    Database::transaction([&] {
        for (const auto &value : units) {
            const auto u = value.toMap();
            if (completed)
                Database::query(QStringLiteral(R"(
                    INSERT INTO progress(series_id, unit_key, unit_title, unit_number, unit_url, percent, completed, updated_at)
                    VALUES(?, ?, ?, ?, ?, 1, 1, ?)
                    ON CONFLICT(series_id, unit_key) DO UPDATE SET completed = 1, percent = 1
                )"),
                                {seriesId, u.value(QStringLiteral("key")).toString(),
                                 u.value(QStringLiteral("title")).toString(), u.value(QStringLiteral("number")).toString(),
                                 u.value(QStringLiteral("url")).toString(), Database::now() - 1});
            else
                Database::query(QStringLiteral("DELETE FROM progress WHERE series_id = ? AND unit_key = ?"),
                                {seriesId, u.value(QStringLiteral("key")).toString()});
        }
    });
    // Marked as read by hand: lists on AniList and MyAnimeList follow.
    if (completed)
        Trackers::unitFinished(seriesId);
    notifyChanged();
}

QVariantMap Library::progressMap(const QString &seriesId) const {
    QVariantMap out;
    for (const auto &value : Database::query(
             QStringLiteral("SELECT unit_key, percent, completed, position FROM progress WHERE series_id = ?"), {seriesId})) {
        const auto row = value.toMap();
        out.insert(row.value(QStringLiteral("unit_key")).toString(),
                   QVariantMap{{"percent", row.value(QStringLiteral("percent"))},
                               {"completed", row.value(QStringLiteral("completed")).toBool()},
                               {"position", parse(row.value(QStringLiteral("position")))}});
    }
    return out;
}

QVariantMap Library::lastProgress(const QString &seriesId) const {
    auto row = Database::one(QStringLiteral("SELECT unit_key AS unitKey, unit_title AS unitTitle, unit_number AS unitNumber, "
                                            "unit_url AS unitUrl, position, percent, completed, updated_at AS updatedAt "
                                            "FROM progress WHERE series_id = ? ORDER BY updated_at DESC LIMIT 1"),
                             {seriesId});
    if (!row.isEmpty())
        row.insert(QStringLiteral("position"), parse(row.value(QStringLiteral("position"))));
    return row;
}

QVariantMap Library::seriesRow(const QString &seriesId) const {
    auto row = Database::one(QStringLiteral("SELECT id, media, source, source_url AS sourceUrl, title, cover, description, "
                                            "author, status, genres, extra FROM series WHERE id = ?"),
                             {seriesId});
    if (!row.isEmpty())
        row = shapeEntry(row);
    return row;
}

Task *Library::checkUpdates() {
    return Task::run([this]() -> QVariant {
        const auto rows = Database::query(QStringLiteral(
            "SELECT s.id, s.media, s.source, s.source_url, s.title, l.known_units FROM library l "
            "JOIN series s ON s.id = l.series_id WHERE s.media IN ('comic', 'novel')"));
        QList<std::function<QVariantMap()>> jobs;
        for (const auto &value : rows) {
            const auto row = value.toMap();
            jobs.append([row]() -> QVariantMap {
                const auto media = row.value(QStringLiteral("media")).toString();
                const auto url = row.value(QStringLiteral("source_url")).toString();
                const auto units = media == QLatin1String("comic") ? Comics::fetchUnits(url)
                                                                    : Novels::fetchUnits(url);
                return {{"id", row.value(QStringLiteral("id"))}, {"units", units}};
            });
        }
        QStringList updated;
        int total = 0;
        const auto results = parallel(jobs);
        for (int i = 0; i < results.size(); ++i) {
            if (!results[i].ok())
                continue;
            const auto row = rows.at(i).toMap();
            const auto id = row.value(QStringLiteral("id")).toString();
            const auto units = results[i].value->value(QStringLiteral("units")).toList();
            if (units.isEmpty())
                continue;
            storeUnits(id, units);
            const int known = row.value(QStringLiteral("known_units")).toInt();
            const int fresh = known > 0 ? std::max(0, int(units.size()) - known) : 0;
            Database::exec(QStringLiteral("UPDATE library SET last_checked = ?, new_units = new_units + ?, "
                                          "known_units = ?, latest_title = ? WHERE series_id = ?"),
                           {Database::now(), fresh, units.size(), units.last().toMap().value(QStringLiteral("title")), id});
            if (fresh > 0) {
                total += fresh;
                updated.append(row.value(QStringLiteral("title")).toString());
            }
        }
        QMetaObject::invokeMethod(this, [this, total, updated] {
            notifyChanged();
            if (total > 0)
                emit updatesFound(total, updated);
        });
        return QVariantMap{{"checked", rows.size()}, {"newUnits", total}, {"titles", updated}};
    });
}

QVariantMap Library::stats() const {
    return {{"saved", Database::scalar(QStringLiteral("SELECT COUNT(*) FROM library")).toInt()},
            {"read", Database::scalar(QStringLiteral("SELECT COUNT(*) FROM progress WHERE completed = 1")).toInt()},
            {"series", Database::scalar(QStringLiteral("SELECT COUNT(DISTINCT series_id) FROM progress")).toInt()}};
}

} // namespace sc
