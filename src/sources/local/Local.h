#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>

// Comics and novels from files on this device: folders of images, CBZ/ZIP,
// CBR/RAR and 7z archives, PDF and EPUB. The folders chosen in Settings →
// Sources are what the "On this device" sources list and search; any of
// these files also opens on its own (from the file manager, say).
namespace sc::local {

// The folders chosen in Settings (the localFolders setting).
QStringList folders();

bool isImage(const QString &name);
// An archive or document that holds a chapter's pages.
bool isPagesFile(const QString &name);
bool isEpub(const QString &name);

// "Chapter 2" before "Chapter 10".
void naturalSort(QStringList &names);
// A file or folder name as a title: no extension, underscores as spaces.
QString titleFrom(const QString &path);

// The pages of a chapter, as addresses the image loader reads: image files
// of a folder, pages unpacked from an archive into the cache, or one
// "pdfpage:" address per PDF page.
QStringList pages(const QString &path);
// A picture for the cover: the first page of the first chapter.
QString cover(const QString &path);

// A file in the cache that belongs to one version of `path` (an EPUB's
// cover, say); its folder exists.
QString cacheFile(const QString &path, const QString &name);

// One file inside a ZIP (EPUB and CBZ), or nothing.
QByteArray zipEntry(const QString &archive, const QString &name);
QStringList zipEntries(const QString &archive);
// Gzip data unpacked (a Mihon backup, a MyAnimeList export), or nothing.
QByteArray gunzip(const QByteArray &data);
// A gzip stream of `data` (Mihon backups are gzipped protobuf).
QByteArray gzip(const QByteArray &data);
// The files in a zip archive held in memory, by their path inside it
// (folders left out); files over `maxFile` bytes are skipped. Empty when it
// is not a zip.
QHash<QString, QByteArray> unzip(const QByteArray &zip, qint64 maxFile = 64 * 1024 * 1024);

// PDF pages are drawn by the app (Qt PDF), when it has it.
struct PdfRenderer {
    std::function<int(const QString &path)> pageCount;
    std::function<QByteArray(const QString &path, int page, int width)> render;
};
void setPdfRenderer(PdfRenderer renderer);
bool canRenderPdf();
// The PNG for a "pdfpage:" address.
QByteArray renderPdfPage(const QString &address);

} // namespace sc::local
