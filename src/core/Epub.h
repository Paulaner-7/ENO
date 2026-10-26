#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantList>

namespace sc {

// A novel as an EPUB 3 book for e-readers (downloaded chapters exported).
struct EpubChapter {
    QString title;
    // The reader's blocks: {kind: paragraph | heading | quote | break,
    // text, html}.
    QVariantList blocks;
};

struct EpubBook {
    QString id; // stable, e.g. the series id
    QString title;
    QString author;
    QString language; // BCP 47, "en" when unknown
    QByteArray cover; // JPEG or PNG, or empty
    QList<EpubChapter> chapters;
};

// The book as EPUB bytes. A block's own markup is kept where it is well
// formed (bold, italics); otherwise its text is written.
QByteArray writeEpub(const EpubBook &book);

} // namespace sc
