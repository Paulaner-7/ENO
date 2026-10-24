#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Mihon (Tachiyomi, Komikku, TachiyomiSY) keeps its downloads as
//   downloads/<Source (lang)>/<Series>/<Scanlator>_<Chapter>_<hash>/  (pages)
//   downloads/<Source (lang)>/<Series>/<Scanlator>_<Chapter>_<hash>.cbz
// SCANCORE reads those files where they are.
namespace sc::local::mihon {

struct Chapter {
    // The folder or CBZ.
    QString path;
    // The name it was saved under, without extension and hash.
    QString name;
    // The six characters after the last underscore, when they look like
    // Mihon's hash of the chapter's address.
    QString hash;
};

struct Series {
    QString path;
    QString name;
};

struct Source {
    QString path;
    QString name;
    QString lang;
};

// Whether `dir` is a Mihon downloads folder (or one source's folder in it).
bool isDownloads(const QString &dir);
// The downloads folder `dir` is or holds (Mihon's storage folder), or
// nothing.
QString downloadsFolder(const QString &dir);
// The source folders in a downloads folder: `dir` itself when it is one.
QList<Source> sources(const QString &dir);
QList<Series> series(const Source &source);
QList<Chapter> chapters(const QString &seriesDir);

// "Name (EN)" as a name and a language.
Source parseSource(const QString &folderName);
// "Scanlator_Chapter 5_ab12cd" as it reads: the chapter's name, with the
// scanlator and hash left off when Mihon's pattern is clear.
QString chapterTitle(const QString &folderName);

// Letters and digits only, lower case: names compare equal across the
// characters Mihon cannot keep in a file name.
QString normalized(const QString &text);
// What Mihon names the folder of a chapter, without the hash.
QString expectedChapterName(const QString &title, const QString &scanlator);
// The hashes Mihon may have appended for a chapter address (it has used
// both ends of the MD5).
QStringList hashesFor(const QString &chapterUrl);

// Which of the library's chapters a folder's chapters are: indices into
// `units` by chapter index (-1: no match). Each unit is {url, title,
// scanlator, number}.
struct Unit {
    QString url;
    QString title;
    QString scanlator;
    QString number;
};
QList<int> matchChapters(const QList<Chapter> &found, const QList<Unit> &units);

} // namespace sc::local::mihon
