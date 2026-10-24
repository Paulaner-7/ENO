#include "Local.h"
#include "LocalSources.h"
#include "MihonLayout.h"

#include "core/Database.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace sc::local {

namespace {

QString pathOf(const QString &url) {
    const QUrl parsed(url);
    return parsed.isLocalFile() ? parsed.toLocalFile() : url;
}

// What a series folder holds as chapters: archives, PDFs and folders of
// images; or the folder's own images as one chapter.
QStringList chapterPaths(const QString &series) {
    const QFileInfo info(series);
    if (!info.isDir())
        return {series};
    QStringList chapters;
    for (const auto &entry : QDir(series).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
        if (entry.isDir()) {
            if (!QDir(entry.absoluteFilePath()).entryList({QStringLiteral("*")}, QDir::Files).filter(
                     QRegularExpression(QStringLiteral(R"(\.(jpe?g|png|webp|gif|avif|bmp|jxl)$)"),
                                        QRegularExpression::CaseInsensitiveOption)).isEmpty())
                chapters.append(entry.absoluteFilePath());
        } else if (isPagesFile(entry.fileName())) {
            chapters.append(entry.absoluteFilePath());
        }
    }
    naturalSort(chapters);
    if (chapters.isEmpty() && !pages(series).isEmpty())
        chapters.append(series);
    return chapters;
}

// A path as the address the source lists (Android folders are content://
// addresses already).
QString urlFor(const QString &path) {
    return path.startsWith(QLatin1String("content:")) ? path : QUrl::fromLocalFile(path).toString();
}

// A path to show: Android's content:// addresses as the folder they name
// ("Mihon/downloads/…").
QString displayPath(const QString &path) {
    if (!path.startsWith(QLatin1String("content:")))
        return QDir::toNativeSeparators(path);
    const auto at = path.lastIndexOf(QLatin1String("/document/"));
    const auto tree = path.lastIndexOf(QLatin1String("/tree/"));
    const auto from = at >= 0 ? at + 10 : tree >= 0 ? tree + 6 : -1;
    if (from < 0)
        return path;
    // "primary:Mihon/downloads": the storage's own name left off.
    const auto document = QUrl::fromPercentEncoding(path.mid(from).toUtf8());
    return document.section(QLatin1Char(':'), 1);
}

// Series folders of Mihon's downloads that no library entry has (the rest
// open from their library entry, offline), from the last link; the folder's
// own when it has not run yet for `root`.
QStringList mihonSeries(const QString &root, const QString &downloads) {
    QStringList out;
    if (Database::pref(QStringLiteral("mihon.link")).toMap().value(QStringLiteral("roots")).toMap().contains(root)) {
        for (const auto &value : Database::pref(QStringLiteral("mihon.others")).toList())
            if (value.toMap().value(QStringLiteral("root")).toString() == root)
                out.append(value.toMap().value(QStringLiteral("path")).toString());
        return out;
    }
    for (const auto &source : mihon::sources(downloads))
        for (const auto &series : mihon::series(source))
            out.append(series.path);
    return out;
}

// Whether a series folder is one of Mihon's (its chapters are named so).
bool isMihonSeries(const QString &path) {
    for (const auto &value : Database::pref(QStringLiteral("mihon.others")).toList())
        if (value.toMap().value(QStringLiteral("path")).toString() == path)
            return true;
    const QFileInfo info(path);
    return !path.startsWith(QLatin1String("content:")) && info.isDir() && !mihon::parseSource(info.dir().dirName()).lang.isEmpty();
}

class LocalComicSource : public comics::Source {
public:
    using comics::Source::Source;

    bool owns(const QUrl &url) const override { return url.isLocalFile() || url.scheme() == QLatin1String("content"); }

    // Each folder or comic file directly inside a chosen folder is a series.
    QList<comics::Summary> search(const QString &query) override {
        const auto words = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QList<comics::Summary> out;
        for (const auto &root : folders()) {
            QStringList entries;
            const auto mihonFolder = mihon::downloadsFolder(root);
            const bool downloads = !mihonFolder.isEmpty();
            if (downloads) {
                entries = mihonSeries(root, mihonFolder);
            } else {
                for (const auto &entry : QDir(root).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
                    if (entry.isDir() || isPagesFile(entry.fileName()))
                        entries.append(entry.absoluteFilePath());
            }
            naturalSort(entries);
            for (const auto &path : entries) {
                const auto title = titleFrom(path);
                const auto lower = title.toLower();
                if (!std::all_of(words.begin(), words.end(), [&](const QString &w) { return lower.contains(w); }))
                    continue;
                comics::Summary summary;
                summary.title = title;
                summary.url = urlFor(path);
                if (!downloads || out.size() < 80)
                    summary.cover = cover(path);
                summary.type = downloads ? QStringLiteral("Mihon") : QFileInfo(path).isDir() ? QStringLiteral("Folder") : QFileInfo(path).suffix().toUpper();
                summary.source = key();
                out.append(summary);
            }
        }
        return out;
    }

    QList<comics::Chapter> chapters(const QString &seriesUrl) override {
        const auto series = pathOf(seriesUrl);
        if (!QFileInfo::exists(series))
            throw Error(QStringLiteral("%1 is no longer on this device.").arg(displayPath(series)), 404);
        QList<comics::Chapter> out;
        // Mihon's chapter folders: named as the chapters, in order.
        if (isMihonSeries(series)) {
            auto found = mihon::chapters(series);
            if (!found.isEmpty()) {
                for (const auto &c : found) {
                    comics::Chapter chapter;
                    chapter.title = mihon::chapterTitle(c.name);
                    chapter.number = comics::chapterNumberFrom(chapter.title);
                    chapter.url = urlFor(c.path);
                    out.append(chapter);
                }
                std::stable_sort(out.begin(), out.end(), [](const comics::Chapter &a, const comics::Chapter &b) {
                    return a.number.toDouble() < b.number.toDouble();
                });
                return out;
            }
        }
        const auto paths = chapterPaths(series);
        for (int i = 0; i < paths.size(); ++i) {
            comics::Chapter chapter;
            chapter.title = paths.size() == 1 && paths[i] == series ? titleFrom(series) : titleFrom(paths[i]);
            const auto number = comics::chapterNumberFrom(chapter.title);
            // Names without a number keep their place in the folder.
            chapter.number = number == QLatin1String("0") ? QString::number(i + 1) : number;
            chapter.url = urlFor(paths[i]);
            out.append(chapter);
        }
        return out;
    }

    QList<comics::Panel> panels(const QString &chapterUrl) override {
        QList<comics::Panel> out;
        for (const auto &page : pages(pathOf(chapterUrl)))
            out.append({page, 0, 0});
        return out;
    }

    std::optional<comics::Details> details(const QString &url) override {
        const auto path = pathOf(url);
        comics::Details details;
        details.title = titleFrom(path);
        details.cover = cover(path);
        details.description = displayPath(path);
        return details;
    }

    QString imageReferer() const override { return {}; }
};

} // namespace

std::unique_ptr<comics::Source> createComicSource(const comics::SourceConfig &config) {
    return std::make_unique<LocalComicSource>(config);
}

} // namespace sc::local
