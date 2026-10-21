#include "Upkeep.h"

#include "Library.h"
#include "TrackerCommon.h"
#include "Trackers.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/Migration.h"
#include "core/Text.h"
#include "core/UpdateRules.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <climits>

namespace sc {

namespace {

constexpr qint64 kDay = 86400;

// A check that failed for this long is worth a look; a source whose every
// series failed this long has probably gone.
constexpr qint64 kFailingDays = 3;
constexpr qint64 kSourceGoneDays = 7;
// No new chapter for this long while the source answers: "stopped updating".
constexpr qint64 kStalledDays = 60;

// Over, or stopped for good.
bool finishedStatus(const QString &status) {
    return updates::finishedStatus(status) || status.contains(QLatin1String("cancel"), Qt::CaseInsensitive) ||
           status.contains(QLatin1String("dropped"), Qt::CaseInsensitive);
}

QString groupKey(QStringList ids) {
    ids.sort();
    return ids.join(QLatin1Char('|'));
}

QStringList apart() { return Database::pref(QStringLiteral("upkeep.apart")).toStringList(); }

QVariantList libraryRows() {
    return Database::query(QStringLiteral(
        "SELECT s.id, s.media, s.title, s.cover, s.source, s.source_url AS sourceUrl, s.status, s.extra, l.known_units AS units, "
        "h.last_ok AS lastOk, h.failing_since AS failingSince, h.failures, h.last_new AS lastNew, h.problem, "
        "(SELECT COUNT(*) FROM progress p WHERE p.series_id = s.id AND p.completed = 1) AS read, "
        "(SELECT MAX(released_at) FROM units u WHERE u.series_id = s.id) AS latest "
        "FROM library l JOIN series s ON s.id = l.series_id LEFT JOIN series_health h ON h.series_id = s.id "
        "WHERE s.media IN ('comic', 'novel') AND s.source != 'local'"));
}

QVariantMap brief(const QVariantMap &row) {
    return {{"id", row.value("id")},       {"media", row.value("media")},   {"title", row.value("title")},
            {"cover", row.value("cover")}, {"source", row.value("source")}, {"sourceUrl", row.value("sourceUrl")},
            {"read", row.value("read")},   {"units", row.value("units")},   {"problem", row.value("problem")},
            {"extra", QJsonDocument::fromJson(row.value("extra").toString().toUtf8()).object().toVariantMap()}};
}

// Library series saved more than once: the same title, AniList entry or
// catalogue entry under one media, on different sources.
QList<QVariantList> findDuplicates(const QVariantList &rows) {
    const auto links = tracking::allLinks();
    QList<QStringList> keys;
    keys.reserve(rows.size());
    for (const auto &value : rows) {
        const auto row = value.toMap();
        const auto media = row.value("media").toString() + QLatin1Char(':');
        const auto extra = QJsonDocument::fromJson(row.value("extra").toString().toUtf8()).object();
        int anilist = links.value(row.value("id").toString()).toMap().value("anilist").toMap().value("id").toInt();
        if (anilist <= 0)
            anilist = extra.value("anilist").toInt();
        QStringList own;
        if (const auto t = titleKey(row.value("title").toString()); t.size() >= 3)
            own.append(media + QStringLiteral("t:") + t);
        if (anilist > 0)
            own.append(media + QStringLiteral("a:%1").arg(anilist));
        if (const auto hid = extra.value("hid").toString(); !hid.isEmpty())
            own.append(media + QStringLiteral("h:") + hid);
        keys.append(own);
    }
    const auto skip = apart();
    QList<QVariantList> out;
    for (const auto &indexes : updates::groupsSharingKeys(keys)) {
        QVariantList group;
        QStringList ids;
        QSet<QString> sources;
        for (const int i : indexes) {
            const auto row = rows.at(i).toMap();
            group.append(brief(row));
            ids.append(row.value("id").toString());
            sources.insert(row.value("source").toString());
        }
        // The same source twice is two different series that share a name.
        if (sources.size() < 2 || skip.contains(groupKey(ids)))
            continue;
        out.append(group);
    }
    return out;
}

QVariantMap buildReport() {
    const qint64 now = Database::now();
    const auto rows = libraryRows();
    QVariantList failing;
    QVariantList stalled;
    QHash<QString, QVariantList> failingBySource;
    QHash<QString, int> answeringBySource;
    for (const auto &value : rows) {
        const auto row = value.toMap();
        const auto source = row.value("source").toString();
        const qint64 since = row.value("failingSince").toLongLong();
        if (since > 0 && now - since >= kFailingDays * kDay && row.value("failures").toInt() >= 2) {
            auto item = brief(row);
            item.insert("days", int((now - since) / kDay));
            failing.append(item);
            failingBySource[source].append(item);
            continue;
        }
        if (row.value("lastOk").toLongLong() > now - kSourceGoneDays * kDay)
            ++answeringBySource[source];
        // Answers, but nothing new for months while not finished.
        if (finishedStatus(row.value("status").toString()) || row.value("units").toInt() == 0)
            continue;
        qint64 latest = row.value("lastNew").toLongLong();
        if (latest <= 0)
            latest = updates::releaseDate(row.value("latest").toString()).value_or(0);
        if (latest > 0 && now - latest >= kStalledDays * kDay) {
            auto item = brief(row);
            item.insert("days", int((now - latest) / kDay));
            stalled.append(item);
        }
    }
    QVariantList sources;
    for (auto it = failingBySource.begin(); it != failingBySource.end(); ++it) {
        int days = INT_MAX;
        for (const auto &item : it.value())
            days = std::min(days, item.toMap().value("days").toInt());
        if (answeringBySource.value(it.key()) > 0 || days < kSourceGoneDays)
            continue;
        sources.append(QVariantMap{{"source", it.key()}, {"media", it.value().first().toMap().value("media")},
                                   {"count", it.value().size()}, {"days", days}, {"series", it.value()}});
    }
    auto byDays = [](const QVariant &a, const QVariant &b) { return a.toMap().value("days").toInt() > b.toMap().value("days").toInt(); };
    std::sort(failing.begin(), failing.end(), byDays);
    std::sort(stalled.begin(), stalled.end(), byDays);
    QVariantList duplicates;
    for (const auto &group : findDuplicates(rows))
        duplicates.append(QVariant(group));
    return {{"sources", sources}, {"failing", failing}, {"stalled", stalled}, {"duplicates", duplicates}};
}

QVariantMap allAlternatives() { return Database::pref(QStringLiteral("library.alternates")).toMap(); }

} // namespace

Upkeep::Upkeep(QObject *parent) : QObject(parent) {
    // New series can be duplicates; a moved or removed one can end one.
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(1500);
    connect(timer, &QTimer::timeout, this, &Upkeep::recount);
    connect(Library::instance(), &Library::saved, timer, qOverload<>(&QTimer::start));
    connect(Library::instance(), &Library::removed, timer, qOverload<>(&QTimer::start));
    QTimer::singleShot(4000, this, &Upkeep::recount);
}

Upkeep *Upkeep::instance() {
    static auto *upkeep = [] {
        auto *created = new Upkeep;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return upkeep;
}

void Upkeep::recount() {
    if (m_counting)
        return;
    m_counting = true;
    runInBackground([] {
        int count = 0;
        try {
            const auto report = buildReport();
            count = report.value("failing").toList().size() + report.value("duplicates").toList().size();
        } catch (const Error &e) {
            qWarning().noquote() << "Library upkeep:" << e.message();
        }
        QMetaObject::invokeMethod(instance(), [count] {
            auto *self = instance();
            self->m_counting = false;
            if (self->m_attention != count) {
                self->m_attention = count;
                emit self->changed();
            }
        });
    });
}

Task *Upkeep::report() {
    return Task::run([]() -> QVariant { return buildReport(); });
}

Task *Upkeep::merge(const QString &keepId, const QStringList &otherIds) {
    auto *task = Task::run([keepId, otherIds]() -> QVariant {
        int read = 0;
        for (const auto &other : otherIds) {
            if (other == keepId)
                continue;
            const auto row = Database::one(QStringLiteral("SELECT source, source_url, title FROM series WHERE id = ?"), {other});
            migration::Options options;
            options.progress = true;
            options.keepOld = false;
            read = std::max(read, migration::move(other, keepId, options).read);
            Trackers::moveLinks(other, keepId, false);
            // Remembered as another place the kept series can be read.
            auto all = allAlternatives();
            auto list = all.value(keepId).toList();
            list.append(QVariantMap{{"id", other}, {"source", row.value("source")}, {"sourceUrl", row.value("source_url")},
                                    {"title", row.value("title")}});
            all.insert(keepId, list.mid(std::max(0, int(list.size()) - 8)));
            Database::setPref(QStringLiteral("library.alternates"), all);
        }
        QMetaObject::invokeMethod(Library::instance(), [keepId, otherIds] {
            auto *library = Library::instance();
            library->notifyChanged();
            emit library->saved(keepId);
            for (const auto &other : otherIds)
                emit library->removed(other);
        });
        return QVariantMap{{"id", keepId}, {"read", read}};
    });
    return task;
}

QVariantList Upkeep::alternatives(const QString &seriesId) const {
    QVariantList out;
    for (const auto &value : allAlternatives().value(seriesId).toList()) {
        const auto alt = value.toMap();
        // Only series still known here (a move can be made back to them).
        if (Database::scalar(QStringLiteral("SELECT 1 FROM series WHERE id = ?"), {alt.value("id")}).isValid())
            out.append(alt);
    }
    return out;
}

void Upkeep::keepApart(const QStringList &ids) {
    auto list = apart();
    list.append(groupKey(ids));
    Database::setPref(QStringLiteral("upkeep.apart"), list.mid(std::max(0, int(list.size()) - 500)));
    recount();
}

namespace {

QJsonObject anilistData(const QString &query) {
    const auto response = Http::postJson(QUrl(QStringLiteral("https://graphql.anilist.co/")),
                                         QJsonDocument(QJsonObject{{"query", query}}).toJson(QJsonDocument::Compact),
                                         {{"Accept", "application/json"}});
    if (response.status == 429)
        throw Error(QStringLiteral("AniList is busy. Try again in a minute."), 429);
    const auto data = response.json().object().value("data").toObject();
    if (!response.ok() || data.isEmpty())
        throw Error(QStringLiteral("AniList is unavailable right now (%1).").arg(response.status), response.status);
    return data;
}

// The episodes coming for anime you follow: saved, or watched in the last
// month. Three hours in the cache.
QVariantList animeComing(qint64 from, qint64 to) {
    QList<int> ids;
    for (const auto &value : Database::query(QStringLiteral(
             "SELECT series_id AS id FROM library WHERE series_id LIKE 'anime:anilist:%' UNION "
             "SELECT DISTINCT series_id FROM progress WHERE series_id LIKE 'anime:anilist:%' AND updated_at > ?"),
                                             {Database::now() - 30 * kDay}))
        if (const int id = value.toMap().value("id").toString().section(QLatin1Char(':'), 2).toInt(); id > 0 && !ids.contains(id))
            ids.append(id);
    QVariantList out;
    QList<int> missing;
    QHash<int, QJsonObject> media;
    for (const int id : ids) {
        if (auto cached = Database::cacheGet(QStringLiteral("radar:anime:%1").arg(id)))
            media.insert(id, QJsonDocument::fromJson(*cached).object());
        else
            missing.append(id);
    }
    for (int at = 0; at < missing.size(); at += 25) {
        QStringList parts;
        for (int i = at; i < std::min(at + 25, int(missing.size())); ++i)
            parts.append(QStringLiteral("a%1: Media(id: %1) { id title { romaji english } coverImage { large } "
                                        "airingSchedule(notYetAired: true, perPage: 8) { nodes { airingAt episode } } }")
                             .arg(missing[i]));
        const auto data = anilistData(QStringLiteral("query { %1 }").arg(parts.join(QLatin1Char(' '))));
        for (int i = at; i < std::min(at + 25, int(missing.size())); ++i) {
            const auto m = data.value(QStringLiteral("a%1").arg(missing[i])).toObject();
            media.insert(missing[i], m);
            Database::cachePut(QStringLiteral("radar:anime:%1").arg(missing[i]), QJsonDocument(m).toJson(QJsonDocument::Compact), 3 * 3600);
        }
    }
    for (auto it = media.begin(); it != media.end(); ++it) {
        const auto m = it.value();
        const auto t = m.value("title").toObject();
        const auto title = t.value("english").toString(t.value("romaji").toString());
        for (const auto &node : m.value("airingSchedule").toObject().value("nodes").toArray()) {
            const auto n = node.toObject();
            const auto at = qint64(n.value("airingAt").toDouble());
            if (at < from || at >= to)
                continue;
            out.append(QVariantMap{{"media", "anime"}, {"title", title}, {"cover", m.value("coverImage").toObject().value("large").toString()},
                                   {"at", at}, {"unit", n.value("episode").toInt()}, {"anilistId", it.key()},
                                   {"seriesId", QStringLiteral("anime:anilist:%1").arg(it.key())}, {"guess", false}});
        }
    }
    return out;
}

// Comics and novels: the next chapter guessed from each series' rhythm
// (the gaps between its last releases).
QVariantList chaptersComing(qint64 from, qint64 to) {
    const qint64 now = Database::now();
    QHash<QString, QList<qint64>> dates;
    QHash<QString, double> lastNumber;
    for (const auto &value : Database::query(QStringLiteral(
             "SELECT series_id, number, released_at FROM (SELECT u.series_id, u.number, u.released_at, "
             "ROW_NUMBER() OVER (PARTITION BY u.series_id ORDER BY u.idx DESC) AS n FROM units u "
             "JOIN library l ON l.series_id = u.series_id) WHERE n <= 12"))) {
        const auto row = value.toMap();
        const auto id = row.value("series_id").toString();
        if (const auto at = updates::releaseDate(row.value("released_at").toString()))
            dates[id].append(*at);
        if (const auto number = migration::chapterValue(row.value("number").toString()))
            lastNumber[id] = std::max(lastNumber.value(id), *number);
    }
    QVariantList out;
    if (dates.isEmpty())
        return out;
    QHash<QString, QVariantMap> series;
    for (const auto &value : Database::query(QStringLiteral(
             "SELECT s.id, s.media, s.title, s.cover, s.status, s.source FROM library l JOIN series s ON s.id = l.series_id "
             "LEFT JOIN series_health h ON h.series_id = s.id "
             "WHERE s.media IN ('comic', 'novel') AND (h.failing_since IS NULL OR h.failing_since > ?)"),
                                             {now - kFailingDays * kDay}))
        series.insert(value.toMap().value("id").toString(), value.toMap());
    for (auto it = dates.begin(); it != dates.end(); ++it) {
        const auto row = series.value(it.key());
        if (row.isEmpty() || finishedStatus(row.value("status").toString()))
            continue;
        const auto r = updates::rhythm(it.value(), now);
        if (!r.known)
            continue;
        const qint64 step = qint64(r.gapDays * kDay);
        qint64 next = r.next;
        const bool late = r.late;
        const double gap = r.gapDays;
        double number = lastNumber.value(it.key());
        for (int k = 0; next < to && k < 14; ++k, next += step) {
            const qint64 at = std::max(next, now);
            number = number > 0 ? std::floor(number) + 1 : 0;
            if (at < from)
                continue;
            out.append(QVariantMap{{"media", row.value("media")}, {"title", row.value("title")}, {"cover", row.value("cover")},
                                   {"at", at}, {"unit", number}, {"seriesId", it.key()}, {"guess", true}, {"late", late && k == 0},
                                   {"every", qRound(gap)}});
        }
    }
    return out;
}

} // namespace

Task *Upkeep::radar(int days) {
    return Task::run([days]() -> QVariant {
        const qint64 now = Database::now();
        const auto start = QDateTime(QDate::currentDate(), QTime(0, 0)).toSecsSinceEpoch();
        const qint64 end = start + std::clamp(days, 1, 31) * kDay;
        auto items = chaptersComing(start, end);
        try {
            items.append(animeComing(std::max(start, now - 6 * 3600), end));
        } catch (const Error &e) {
            // Comics and novels are known here; anime need AniList.
            if (items.isEmpty())
                throw;
            qWarning().noquote() << "Radar:" << e.message();
        }
        std::sort(items.begin(), items.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value("at").toLongLong() < b.toMap().value("at").toLongLong();
        });
        QVariantList out;
        QString current;
        QVariantList bucket;
        for (const auto &value : items) {
            const auto day = QDateTime::fromSecsSinceEpoch(value.toMap().value("at").toLongLong()).date().toString(Qt::ISODate);
            if (day != current && !bucket.isEmpty()) {
                out.append(QVariantMap{{"day", current}, {"items", bucket}});
                bucket.clear();
            }
            current = day;
            bucket.append(value);
        }
        if (!bucket.isEmpty())
            out.append(QVariantMap{{"day", current}, {"items", bucket}});
        return out;
    });
}

} // namespace sc
