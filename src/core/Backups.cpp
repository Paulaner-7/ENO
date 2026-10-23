#include "core/Backups.h"

#include "core/Http.h"
#include "core/Proto.h"
#include "sources/local/Local.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>

#include <algorithm>

namespace sc::backups {

namespace {
// A category (Mihon and Aniyomi): its order (what entries refer to) and name.
std::pair<qint64, QString> parseCategory(QByteArrayView bytes) {
    Proto c(bytes);
    QString name;
    qint64 order = 0;
    while (c.next()) {
        if (c.field() == 1)
            name = c.text();
        else if (c.field() == 2)
            order = qint64(c.varint());
        else
            c.skip();
    }
    return {order, name};
}

// An entry's category orders: repeated varints, packed or not.
void readOrders(Proto &m, QList<qint64> &out) {
    if (m.wire() != 2) {
        out.append(qint64(m.varint()));
        return;
    }
    // Packed: the varints one after another, without tags.
    Proto values(m.bytes());
    while (values.more())
        out.append(qint64(values.varint()));
}

// Mihon's trackers by their id there, as SCANCORE names them (1 and 2,
// MyAnimeList and AniList, have fields of their own).
QString trackerName(int service) {
    switch (service) {
    case 3: return QStringLiteral("kitsu");
    case 4: return QStringLiteral("shikimori");
    case 5: return QStringLiteral("bangumi");
    case 7: return QStringLiteral("mangaupdates");
    default: return {};
    }
}

// Mihon's chapter flags (Manga.chapterFlags): sort direction 0x1 (set:
// ascending), unread only 0x2, bookmarked only 0x20.
void applyChapterFlags(qint64 flags, QVariantMap &settings) {
    if (flags & 0x1)
        settings.insert(QStringLiteral("sort"), QStringLiteral("asc"));
    if ((flags & 0x6) == 0x2)
        settings.insert(QStringLiteral("onlyUnread"), true);
    if ((flags & 0x60) == 0x20)
        settings.insert(QStringLiteral("onlyBookmarked"), true);
}

// Mihon's reading mode (the low three bits of viewer flags): 1 left to
// right, 2 right to left, 3 vertical pages, 4 webtoon, 5 continuous.
void applyViewerFlags(qint64 flags, QVariantMap &reader) {
    switch (flags & 0x7) {
    case 1:
    case 3:
        reader = {{"readerMode", "paged"}, {"readerDirection", "ltr"}};
        break;
    case 2:
        reader = {{"readerMode", "paged"}, {"readerDirection", "rtl"}};
        break;
    case 4:
    case 5:
        reader = {{"readerMode", "strip"}};
        break;
    default:
        break;
    }
}

// One manga (Mihon) or anime (Aniyomi) entry; false for entries outside the
// library, which only carry history.
bool parseEntry(QByteArrayView bytes, bool anime, quint64 &source, Item &item, QList<qint64> &categories) {
    Proto m(bytes);
    item.media = anime ? QStringLiteral("anime") : QStringLiteral("comic");
    bool favorite = true;
    float tracked = 0;
    QString title;
    qint64 viewer = 0;
    QStringList hidden;
    while (m.next()) {
        switch (m.field()) {
        case 1: source = m.varint(); break;
        case 2: item.url = m.text(); break;
        case 3: title = m.text(); break;
        case 9: item.cover = m.text(); break;
        case 13: item.addedAt = qint64(m.varint()) / 1000; break;
        case 14:
            if (!viewer)
                viewer = qint64(m.varint());
            else
                m.skip();
            break;
        case 16: {
            // BackupChapter / BackupEpisode: url 1, read or seen 4, bookmark
            // 5, last page (or second) 6, number 9, changed 11, (episodes)
            // total seconds 16.
            Proto unit(m.bytes());
            bool read = false;
            bool bookmark = false;
            double number = -1;
            qint64 changed = 0;
            double at = 0;
            double total = 0;
            QString link;
            while (unit.next()) {
                switch (unit.field()) {
                case 1: link = unit.text(); break;
                case 4: read = unit.varint() != 0; break;
                case 5: bookmark = unit.varint() != 0; break;
                case 6: at = unit.number(); break;
                case 9: number = unit.number(); break;
                case 11: changed = qint64(unit.varint()); break;
                case 16: total = unit.number(); break;
                default: unit.skip();
                }
            }
            if (bookmark && !link.isEmpty())
                item.bookmarkLinks.insert(link);
            if (read) {
                if (number >= 0)
                    item.numbers.append(number);
                if (!link.isEmpty())
                    item.doneLinks.insert(link);
                item.updatedAt = std::max(item.updatedAt, changed);
            } else if (anime && at > 0 && total > 0 && !link.isEmpty()) {
                // Aniyomi keeps milliseconds (despite the names).
                const double scale = total > 20000 ? 1000.0 : 1.0;
                item.parts.insert(link, {std::clamp(at / total, 0.0, 0.99),
                                         {{"type", "video"}, {"seconds", at / scale}, {"duration", total / scale}}});
            } else if (!anime && at > 0 && !link.isEmpty()) {
                // Mihon knows the page, not how many there are: the reader
                // opens on it and finds out.
                item.parts.insert(link, {0.01, {{"panel", qint64(at)}}});
            }
            break;
        }
        case 17: readOrders(m, categories); break;
        case 18: {
            Proto tracking(m.bytes());
            int service = 0;
            qint64 id = 0;
            float last = 0;
            while (tracking.next()) {
                if (tracking.field() == 1)
                    service = int(tracking.varint());
                else if (tracking.field() == 3 || tracking.field() == 100)
                    id = std::max<qint64>(id, qint64(tracking.varint()));
                else if (tracking.field() == 6)
                    last = float(tracking.number());
                else
                    tracking.skip();
            }
            // Mihon's and Aniyomi's tracker ids: 1 MyAnimeList, 2 AniList.
            if (service == 1)
                item.malId = int(id);
            else if (service == 2)
                item.anilistId = int(id);
            else if (const auto name = trackerName(service); !name.isEmpty() && id > 0)
                item.trackerIds.insert(name, id);
            tracked = std::max(tracked, last);
            break;
        }
        case 100: favorite = m.varint() != 0; break;
        case 101: applyChapterFlags(qint64(m.varint()), item.settings); break;
        case 103: viewer = qint64(m.varint()); break;
        case 104: {
            // BackupHistory: the chapter's url 1, last read 2 (milliseconds).
            Proto history(m.bytes());
            QString link;
            qint64 at = 0;
            while (history.next()) {
                if (history.field() == 1)
                    link = history.text();
                else if (history.field() == 2)
                    at = qint64(history.varint()) / 1000;
                else
                    history.skip();
            }
            if (!link.isEmpty() && at > 0)
                item.readAt.insert(link, std::max(item.readAt.value(link), at));
            break;
        }
        case 108: {
            const auto name = m.text().trimmed();
            if (!name.isEmpty())
                hidden.append(name);
            break;
        }
        default: m.skip();
        }
    }
    if (m.bad())
        throw Error(QStringLiteral("This backup is damaged."));
    if (!favorite || title.isEmpty())
        return false;
    item.titles = {title};
    item.count = int(tracked);
    // Chapter list and reader settings are for comics; the anime page has
    // its own.
    if (anime) {
        item.settings.clear();
    } else {
        applyViewerFlags(viewer, item.reader);
        if (!hidden.isEmpty())
            item.settings.insert(QStringLiteral("hiddenScanlators"), hidden);
    }
    for (auto it = item.readAt.cbegin(); it != item.readAt.cend(); ++it)
        item.updatedAt = std::max(item.updatedAt, it.value());
    return true;
}

// An extension repository (Mihon's BackupExtensionStore, earlier
// BackupExtensionRepos): its index 1, name 2, signing key 5.
Repository parseRepository(QByteArrayView bytes) {
    Proto r(bytes);
    Repository repo;
    while (r.next()) {
        switch (r.field()) {
        case 1: repo.url = r.text(); break;
        case 2: repo.name = r.text(); break;
        case 5: repo.signingKey = r.text(); break;
        default: r.skip();
        }
    }
    return repo;
}

} // namespace

// SCANCORE's list for a category name from another app ("Reading",
// "Plan to watch", "On hold"…); empty when it says nothing about it.
QString categoryNamed(const QString &name) {
    const auto n = name.toLower();
    if (n.contains(QLatin1String("plan")) || n.contains(QLatin1String("later")) || n.contains(QLatin1String("pendiente")))
        return QStringLiteral("planned");
    if (n.contains(QLatin1String("hold")) || n.contains(QLatin1String("pause")))
        return QStringLiteral("paused");
    if (n.contains(QLatin1String("drop")))
        return QStringLiteral("dropped");
    if (n.contains(QLatin1String("complet")) || n.contains(QLatin1String("finish")) || n.contains(QLatin1String("done")))
        return QStringLiteral("completed");
    if (n.contains(QLatin1String("read")) || n.contains(QLatin1String("watch")))
        return QStringLiteral("reading");
    return {};
}

// Mihon's Backup, and Aniyomi's (the same with anime): manga (1),
// categories (2), sources (101), extension repositories (106); anime (3, or
// 501 in older Aniyomi backups), their categories (4 or 502) and sources
// (103 or 503).
MihonBackup parseMihonBackup(const QByteArray &data) {
    QHash<quint64, QString> sourceNames;
    QHash<quint64, QString> animeSourceNames;
    QMap<qint64, QString> categoryNames;
    QMap<qint64, QString> animeCategoryNames;
    struct Entry {
        quint64 source = 0;
        Item item;
        QList<qint64> categories;
    };
    QList<Entry> manga;
    QList<Entry> anime;
    MihonBackup out;
    const auto sourceOf = [](QByteArrayView bytes) {
        Proto source(bytes);
        QString name;
        quint64 id = 0;
        while (source.next()) {
            if (source.field() == 1)
                name = source.text();
            else if (source.field() == 2)
                id = source.varint();
            else
                source.skip();
        }
        return std::pair{id, name};
    };
    Proto backup(data);
    while (backup.next()) {
        const int f = backup.field();
        if (f == 101 || f == 103 || f == 503) {
            const auto [id, name] = sourceOf(backup.bytes());
            (f == 101 ? sourceNames : animeSourceNames).insert(id, name);
        } else if (f == 2 || f == 4 || f == 502) {
            const auto [order, name] = parseCategory(backup.bytes());
            (f == 2 ? categoryNames : animeCategoryNames).insert(order, name);
        } else if (f == 1 || f == 3 || f == 501) {
            Entry entry;
            if (parseEntry(backup.bytes(), f != 1, entry.source, entry.item, entry.categories))
                (f == 1 ? manga : anime).append(entry);
        } else if (f == 106) {
            if (auto repo = parseRepository(backup.bytes()); repo.url.startsWith(QLatin1String("https://")))
                out.repositories.append(repo);
        } else {
            backup.skip();
        }
    }
    if (backup.bad())
        throw Error(QStringLiteral("This is not a Mihon, Tachiyomi or Aniyomi backup, or it is damaged."));
    // SCANCORE's list for one of the backup's, or "custom:<name>"; empty for
    // Default (no list: reading).
    const auto listFor = [](const QString &name) {
        const auto n = name.trimmed();
        if (n.isEmpty() || n.compare(QLatin1String("Default"), Qt::CaseInsensitive) == 0)
            return QString();
        const auto known = categoryNamed(n);
        return known.isEmpty() ? QStringLiteral("custom:") + n : known;
    };
    // The backup's own lists in its order (QMap: by order), comics' first.
    for (const auto *names : {&categoryNames, &animeCategoryNames})
        for (const auto &name : *names)
            if (const auto list = listFor(name); list.startsWith(QLatin1String("custom:")) && !out.lists.contains(list.mid(7), Qt::CaseInsensitive))
                out.lists.append(list.mid(7));
    const auto finish = [&](QList<Entry> &entries, const QHash<quint64, QString> &names, const QMap<qint64, QString> &categories,
                            const QString &kind) {
        for (auto &entry : entries) {
            auto &item = entry.item;
            item.sourceName = names.value(entry.source);
            item.sourceId = entry.source;
            item.extensionKey = entry.source ? QStringLiteral("%1:%2").arg(kind).arg(entry.source) : QString();
            // One list per series here: the first of its lists in the
            // backup's order; the others are reported.
            auto orders = entry.categories;
            std::sort(orders.begin(), orders.end());
            QStringList lists;
            for (const auto order : orders)
                if (const auto list = listFor(categories.value(order)); !list.isEmpty() && !lists.contains(list))
                    lists.append(list);
            if (!lists.isEmpty()) {
                item.category = lists.takeFirst();
                item.otherCategories = lists;
            }
            out.items.append(item);
        }
    };
    finish(manga, sourceNames, categoryNames, QStringLiteral("mihon"));
    finish(anime, animeSourceNames, animeCategoryNames, QStringLiteral("aniyomi"));
    return out;
}

QList<Item> parseMihon(const QByteArray &data) { return parseMihonBackup(data).items; }

// LNReader's backup: a zip with NovelAndChapters/<id>.json (the novel, its
// plugin and path, and its chapters with unread, bookmark and progress) and
// Category.json ([{name, novelIds}]). Older backups put it all inside a
// data.zip.
QList<Item> parseLNReader(const QByteArray &zip) {
    auto files = local::unzip(zip);
    if (files.contains(QStringLiteral("data.zip")))
        files.insert(local::unzip(files.value(QStringLiteral("data.zip"))));
    QHash<qint64, QString> categoryOf;
    for (const auto &value : QJsonDocument::fromJson(files.value(QStringLiteral("Category.json"))).array()) {
        const auto category = value.toObject();
        const auto name = category.value("name").toString().trimmed();
        auto list = categoryNamed(name);
        if (list.isEmpty() && !name.isEmpty() && name.compare(QLatin1String("Default"), Qt::CaseInsensitive) != 0)
            list = QStringLiteral("custom:") + name;
        if (list.isEmpty())
            continue;
        for (const auto &id : category.value("novelIds").toArray())
            if (!categoryOf.contains(id.toInteger()))
                categoryOf.insert(id.toInteger(), list);
    }
    QList<Item> out;
    bool any = false;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        if (!it.key().startsWith(QLatin1String("NovelAndChapters/")) || !it.key().endsWith(QLatin1String(".json")))
            continue;
        any = true;
        const auto novel = QJsonDocument::fromJson(it.value()).object();
        const auto plugin = novel.value("pluginId").toString();
        const auto name = novel.value("name").toString();
        const auto inLibrary = novel.value("inLibrary");
        if (name.isEmpty() || plugin.isEmpty() || novel.value("isLocal").toBool() || inLibrary == QJsonValue(false) || inLibrary == QJsonValue(0))
            continue;
        Item item;
        item.media = QStringLiteral("novel");
        item.titles = {name};
        item.url = novel.value("path").toString();
        item.sourceName = plugin;
        item.extensionKey = QStringLiteral("lnreader:") + plugin;
        if (const auto cover = novel.value("cover").toString(); cover.startsWith(QLatin1String("http")))
            item.cover = cover;
        item.category = categoryOf.value(novel.value("id").toInteger(), QStringLiteral("reading"));
        for (const auto &value : novel.value("chapters").toArray()) {
            const auto chapter = value.toObject();
            const auto link = chapter.value("path").toString();
            if (link.isEmpty())
                continue;
            if (chapter.value("bookmark").toBool() || chapter.value("bookmark").toInt() == 1)
                item.bookmarkLinks.insert(link);
            const bool unread = chapter.value("unread").toBool() || chapter.value("unread").toInt() == 1;
            const double progress = chapter.value("progress").toDouble();
            if (!unread) {
                item.doneLinks.insert(link);
                if (chapter.value("chapterNumber").isDouble())
                    item.numbers.append(chapter.value("chapterNumber").toDouble());
            } else if (progress > 0) {
                const auto percent = std::clamp(progress / 100, 0.0, 0.99);
                item.parts.insert(link, {percent, {{"type", "text"}, {"percent", percent}}});
            }
        }
        item.count = int(item.doneLinks.size());
        out.append(item);
    }
    if (!any)
        throw Error(QStringLiteral("This is not an LNReader backup."));
    return out;
}

} // namespace sc::backups
