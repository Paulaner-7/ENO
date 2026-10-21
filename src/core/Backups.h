#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace sc::backups {

// Libraries from other apps' backups, as Importer brings them in.

// One series from elsewhere.
struct Item {
    QString media; // comic, novel or anime
    QStringList titles;
    QString cover;
    QString category = QStringLiteral("reading");
    QString sourceName; // Mihon's name for its source
    QString url;        // Mihon's address on that source (often a path)
    quint64 sourceId = 0; // Mihon's id for its source (an extension's)
    // The extension (or LNReader plugin) it came from, as SCANCORE keys it
    // ("mihon:<id>", "aniyomi:<id>", "lnreader:<id>"): restored there
    // exactly when it is installed.
    QString extensionKey;
    // The extension's own links of the chapters or episodes done, bookmarked,
    // and the one part-way (with how far, 0-1).
    QSet<QString> doneLinks;
    QSet<QString> bookmarkLinks;
    QString partLink;
    double partPercent = 0;
    QVariantMap partPosition; // as the reader or player keeps it
    int anilistId = 0;
    int malId = 0;
    QString comickHid;
    // Chapters or episodes done: the numbers when known (Mihon), else a count.
    QList<double> numbers;
    int count = 0;
    qint64 updatedAt = 0;
    QJsonObject anime; // AniList media, for anime

    QString title() const { return titles.value(0); }
};

// SCANCORE's list for a category name from another app ("Reading",
// "Plan to watch", "On hold"…); empty when it says nothing about it.
QString categoryNamed(const QString &name);

// Mihon's Backup, and Aniyomi's (the same with anime): manga (1),
// categories (2), sources (101); anime (3, or 501 in older Aniyomi
// backups), their categories (4 or 502) and sources (103 or 503). Entries
// outside the library are left out. Throws on a damaged file.
QList<Item> parseMihon(const QByteArray &data);

// LNReader's backup: a zip with NovelAndChapters/<id>.json (the novel, its
// plugin and path, and its chapters with unread, bookmark and progress) and
// Category.json ([{name, novelIds}]). Older backups put it all inside a
// data.zip. Throws when it is not one.
QList<Item> parseLNReader(const QByteArray &zip);

} // namespace sc::backups
