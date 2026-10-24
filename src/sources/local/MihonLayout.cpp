#include "MihonLayout.h"

#include "Local.h"
#include "sources/comics/ComicSource.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

namespace sc::local::mihon {

namespace {

// "Name (EN)", "Name (en-US)", "Name (all)".
const QRegularExpression &sourcePattern() {
    static const QRegularExpression pattern(QStringLiteral(R"(^(.+) \(([A-Za-z]{2,3}(?:[-_][A-Za-z0-9]{2,4})*|all|multi|other)\)$)"),
                                            QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

const QRegularExpression &hashPattern() {
    static const QRegularExpression pattern(QStringLiteral(R"(^(.*)_([0-9a-f]{6})$)"));
    return pattern;
}

// A chapter Mihon finished: a folder (Mihon makes no other folders in a
// series' folder, so its pages are not listed here: on Android each listing
// is a call to the documents provider) or a CBZ.
bool isChapterEntry(const Entry &entry) {
    if (entry.dir)
        // Unfinished downloads wait in "_tmp" folders.
        return !entry.name.endsWith(QLatin1String("_tmp"));
    return entry.name.endsWith(QLatin1String(".cbz"), Qt::CaseInsensitive);
}

// Pages in a folder (only for telling a downloads folder apart).
bool holdsPages(const Entry &entry) {
    if (!entry.dir)
        return true;
    const auto files = entries(entry.path);
    return std::any_of(files.begin(), files.end(), [](const Entry &f) { return !f.dir && isImage(f.name); });
}

// Whether a series folder holds at least one chapter.
bool holdsChapters(const QString &seriesDir, int limit = 6) {
    int looked = 0;
    for (const auto &entry : entries(seriesDir)) {
        if (isChapterEntry(entry) && holdsPages(entry))
            return true;
        if (++looked >= limit)
            break;
    }
    return false;
}

bool holdsSeries(const QString &sourceDir) {
    int looked = 0;
    for (const auto &entry : entries(sourceDir)) {
        if (!entry.dir)
            continue;
        if (holdsChapters(entry.path))
            return true;
        if (++looked >= 8)
            break;
    }
    return false;
}

} // namespace

Source parseSource(const QString &folderName) {
    Source out;
    out.name = folderName;
    const auto match = sourcePattern().match(folderName);
    if (match.hasMatch()) {
        out.name = match.captured(1).trimmed();
        out.lang = match.captured(2).toLower();
    }
    return out;
}

QList<Source> sources(const QString &dir) {
    QList<Source> out;
    const QFileInfo info(dir);
    if (!info.isDir())
        return out;
    // The chosen folder is one source's.
    if (sourcePattern().match(info.fileName()).hasMatch() && holdsSeries(dir)) {
        auto source = parseSource(info.fileName());
        source.path = info.absoluteFilePath();
        return {source};
    }
    for (const auto &entry : entries(dir)) {
        if (!entry.dir || entry.name.endsWith(QLatin1String("_tmp")))
            continue;
        auto source = parseSource(entry.name);
        source.path = entry.path;
        out.append(source);
    }
    return out;
}

bool isDownloads(const QString &dir) {
    const QFileInfo info(dir);
    if (!info.isDir())
        return false;
    if (sourcePattern().match(info.fileName()).hasMatch() && holdsSeries(dir))
        return true;
    int looked = 0;
    for (const auto &entry : entries(dir)) {
        if (!entry.dir || (!sourcePattern().match(entry.name).hasMatch() && entry.name.compare(QLatin1String("local"), Qt::CaseInsensitive) != 0))
            continue;
        if (holdsSeries(entry.path))
            return true;
        if (++looked >= 6)
            break;
    }
    return false;
}

QString downloadsFolder(const QString &dir) {
    if (isDownloads(dir))
        return dir;
    // Mihon's storage folder: "downloads" is one of its folders. Found by
    // listing, as an Android folder's address cannot be joined with a name.
    for (const auto &entry : entries(dir))
        if (entry.dir && entry.name.compare(QLatin1String("downloads"), Qt::CaseInsensitive) == 0 && isDownloads(entry.path))
            return entry.path;
    return {};
}

QList<Series> series(const Source &source) {
    QList<Series> out;
    for (const auto &entry : entries(source.path))
        if (entry.dir)
            out.append({entry.path, entry.name});
    return out;
}

QList<Chapter> chapters(const QString &seriesDir) {
    QList<Chapter> out;
    for (const auto &entry : entries(seriesDir)) {
        if (!isChapterEntry(entry))
            continue;
        Chapter chapter;
        chapter.path = entry.path;
        auto name = entry.dir ? entry.name : entry.name.chopped(4);
        const auto match = hashPattern().match(name);
        if (match.hasMatch()) {
            chapter.hash = match.captured(2);
            name = match.captured(1);
        }
        chapter.name = name;
        out.append(chapter);
    }
    return out;
}

QString chapterTitle(const QString &folderName) {
    auto name = folderName;
    if (name.endsWith(QLatin1String(".cbz"), Qt::CaseInsensitive))
        name.chop(4);
    if (const auto match = hashPattern().match(name); match.hasMatch())
        name = match.captured(1);
    // "Scanlator_Chapter 5": the part after the first underscore when it
    // reads as a chapter by itself.
    const auto cut = name.indexOf(QLatin1Char('_'));
    if (cut > 0) {
        const auto rest = name.mid(cut + 1);
        if (comics::chapterNumberFrom(rest) != QLatin1String("0") && !rest.trimmed().isEmpty())
            name = rest;
    }
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    return name.simplified();
}

QString normalized(const QString &text) {
    QString out;
    out.reserve(text.size());
    for (const QChar c : text.normalized(QString::NormalizationForm_KC))
        if (c.isLetterOrNumber())
            out.append(c.toLower());
    return out;
}

QString expectedChapterName(const QString &title, const QString &scanlator) {
    return scanlator.trimmed().isEmpty() ? title : scanlator.trimmed() + QLatin1Char('_') + title;
}

QStringList hashesFor(const QString &chapterUrl) {
    if (chapterUrl.isEmpty())
        return {};
    const auto hex = QString::fromLatin1(QCryptographicHash::hash(chapterUrl.toUtf8(), QCryptographicHash::Md5).toHex());
    return {hex.left(6), hex.right(6)};
}

QList<int> matchChapters(const QList<Chapter> &found, const QList<Unit> &units) {
    QList<int> out(found.size(), -1);
    QSet<int> used;
    const auto take = [&](int chapter, int unit) {
        out[chapter] = unit;
        used.insert(unit);
    };
    // Mihon's own hash of the chapter's address: exact.
    QList<QStringList> hashes;
    hashes.reserve(units.size());
    for (const auto &unit : units)
        hashes.append(hashesFor(unit.url));
    for (int i = 0; i < found.size(); ++i) {
        if (found[i].hash.isEmpty())
            continue;
        for (int u = 0; u < units.size(); ++u)
            if (!used.contains(u) && hashes[u].contains(found[i].hash)) {
                take(i, u);
                break;
            }
    }
    // The name, with or without the scanlator in front.
    QHash<QString, QList<int>> byTitle;
    QHash<QString, QList<int>> byFull;
    for (int u = 0; u < units.size(); ++u) {
        byTitle[normalized(units[u].title)].append(u);
        byFull[normalized(expectedChapterName(units[u].title, units[u].scanlator))].append(u);
    }
    const auto firstFree = [&](const QList<int> &candidates) {
        for (const int u : candidates)
            if (!used.contains(u))
                return u;
        return -1;
    };
    for (int i = 0; i < found.size(); ++i) {
        if (out[i] >= 0)
            continue;
        const auto &name = found[i].name;
        int hit = firstFree(byFull.value(normalized(name)));
        if (hit < 0 && !found[i].hash.isEmpty())
            hit = firstFree(byFull.value(normalized(name + QLatin1Char('_') + found[i].hash)));
        // Every split after an underscore: the scanlator is whatever was
        // in front.
        for (qsizetype at = name.indexOf(QLatin1Char('_')); hit < 0 && at >= 0; at = name.indexOf(QLatin1Char('_'), at + 1))
            hit = firstFree(byTitle.value(normalized(name.mid(at + 1))));
        if (hit < 0)
            hit = firstFree(byTitle.value(normalized(name)));
        if (hit >= 0)
            take(i, hit);
    }
    // The chapter's number, when only one chapter here and one there have it.
    QHash<QString, QList<int>> numbers;
    for (int u = 0; u < units.size(); ++u)
        if (!used.contains(u)) {
            const auto number = units[u].number.isEmpty() ? comics::chapterNumberFrom(units[u].title) : units[u].number;
            if (number != QLatin1String("0"))
                numbers[number].append(u);
        }
    QHash<QString, QList<int>> foundNumbers;
    for (int i = 0; i < found.size(); ++i)
        if (out[i] < 0)
            foundNumbers[comics::chapterNumberFrom(chapterTitle(found[i].name))].append(i);
    foundNumbers.remove(QStringLiteral("0"));
    for (auto it = foundNumbers.begin(); it != foundNumbers.end(); ++it)
        if (it.value().size() == 1 && numbers.value(it.key()).size() == 1)
            take(it.value().first(), numbers.value(it.key()).first());
    return out;
}

} // namespace sc::local::mihon
