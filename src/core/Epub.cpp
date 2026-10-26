#include "Epub.h"

#include <QBuffer>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <QXmlStreamReader>
#include <QtCore/private/qzipwriter_p.h>

namespace sc {

namespace {

QString escaped(const QString &text) { return text.toHtmlEscaped(); }

// Markup from a source, if it is XHTML once wrapped: entities XML does not
// know become characters, and only inline tags are left.
QString inlineMarkup(const QString &html) {
    auto out = html;
    out.replace(QStringLiteral("&nbsp;"), QStringLiteral("&#160;"));
    static const QRegularExpression br(QStringLiteral(R"(<br\s*>)"), QRegularExpression::CaseInsensitiveOption);
    out.replace(br, QStringLiteral("<br/>"));
    QXmlStreamReader xml(QStringLiteral("<p xmlns=\"http://www.w3.org/1999/xhtml\">") + out + QStringLiteral("</p>"));
    static const QSet<QString> allowed{"p", "b", "i", "em", "strong", "u", "s", "br", "span", "sub", "sup", "small", "a"};
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement && !allowed.contains(xml.name().toString().toLower()))
            return {};
    }
    return xml.hasError() ? QString() : out;
}

QString chapterXhtml(const EpubChapter &chapter, const QString &language) {
    QString body;
    body += QStringLiteral("<h2>%1</h2>\n").arg(escaped(chapter.title));
    for (const auto &value : chapter.blocks) {
        const auto block = value.toMap();
        const auto kind = block.value(QStringLiteral("kind")).toString();
        if (kind == QLatin1String("break")) {
            body += QStringLiteral("<p class=\"break\">⁂</p>\n");
            continue;
        }
        auto inner = inlineMarkup(block.value(QStringLiteral("html")).toString());
        if (inner.isEmpty())
            inner = escaped(block.value(QStringLiteral("text")).toString());
        if (inner.trimmed().isEmpty())
            continue;
        if (kind == QLatin1String("heading"))
            body += QStringLiteral("<h3>%1</h3>\n").arg(inner);
        else if (kind == QLatin1String("quote"))
            body += QStringLiteral("<blockquote><p>%1</p></blockquote>\n").arg(inner);
        else
            body += QStringLiteral("<p>%1</p>\n").arg(inner);
    }
    return QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                          "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"%1\" lang=\"%1\">\n"
                          "<head><title>%2</title><link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"/></head>\n"
                          "<body>\n%3</body>\n</html>\n")
        .arg(language, escaped(chapter.title), body);
}

} // namespace

QByteArray writeEpub(const EpubBook &book) {
    const auto language = book.language.isEmpty() ? QStringLiteral("en") : book.language;
    const auto id = book.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : book.id;
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QZipWriter zip(&buffer);
    // The mimetype first and stored, as the format asks.
    zip.setCompressionPolicy(QZipWriter::NeverCompress);
    zip.addFile(QStringLiteral("mimetype"), QByteArrayLiteral("application/epub+zip"));
    zip.setCompressionPolicy(QZipWriter::AlwaysCompress);
    zip.addFile(QStringLiteral("META-INF/container.xml"),
                QByteArrayLiteral("<?xml version=\"1.0\"?>\n<container version=\"1.0\" "
                                  "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n<rootfiles><rootfile "
                                  "full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>\n"
                                  "</container>\n"));
    zip.addFile(QStringLiteral("OEBPS/style.css"),
                QByteArrayLiteral("body { line-height: 1.5; }\np { margin: 0 0 0.8em; }\nh2 { margin: 1em 0; }\n"
                                  ".break { text-align: center; }\nblockquote { font-style: italic; margin: 0 1.5em; }\n"));
    QString manifest, spine, nav;
    const bool png = book.cover.startsWith("\x89PNG");
    if (!book.cover.isEmpty()) {
        const auto name = png ? QStringLiteral("cover.png") : QStringLiteral("cover.jpg");
        zip.addFile(QStringLiteral("OEBPS/") + name, book.cover);
        manifest += QStringLiteral("<item id=\"cover-image\" href=\"%1\" media-type=\"%2\" properties=\"cover-image\"/>\n")
                        .arg(name, png ? QStringLiteral("image/png") : QStringLiteral("image/jpeg"));
    }
    for (int i = 0; i < book.chapters.size(); ++i) {
        const auto file = QStringLiteral("chapter-%1.xhtml").arg(i + 1, 4, 10, QLatin1Char('0'));
        zip.addFile(QStringLiteral("OEBPS/") + file, chapterXhtml(book.chapters[i], language).toUtf8());
        manifest += QStringLiteral("<item id=\"c%1\" href=\"%2\" media-type=\"application/xhtml+xml\"/>\n").arg(i + 1).arg(file);
        spine += QStringLiteral("<itemref idref=\"c%1\"/>\n").arg(i + 1);
        nav += QStringLiteral("<li><a href=\"%1\">%2</a></li>\n").arg(file, escaped(book.chapters[i].title));
    }
    zip.addFile(QStringLiteral("OEBPS/nav.xhtml"),
                QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                               "xmlns:epub=\"http://www.idpf.org/2007/ops\"><head><title>%1</title></head>\n<body><nav "
                               "epub:type=\"toc\"><h1>%1</h1><ol>\n%2</ol></nav></body></html>\n")
                    .arg(escaped(book.title), nav)
                    .toUtf8());
    const auto modified = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddThh:mm:ssZ"));
    zip.addFile(QStringLiteral("OEBPS/content.opf"),
                QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<package xmlns=\"http://www.idpf.org/2007/opf\" "
                               "version=\"3.0\" unique-identifier=\"book-id\">\n<metadata "
                               "xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n<dc:identifier id=\"book-id\">%1</dc:identifier>\n"
                               "<dc:title>%2</dc:title>\n<dc:creator>%3</dc:creator>\n<dc:language>%4</dc:language>\n"
                               "<meta property=\"dcterms:modified\">%5</meta>\n</metadata>\n<manifest>\n<item id=\"nav\" "
                               "href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n<item id=\"css\" "
                               "href=\"style.css\" media-type=\"text/css\"/>\n%6</manifest>\n<spine>\n%7</spine>\n</package>\n")
                    .arg(escaped(id), escaped(book.title), escaped(book.author.isEmpty() ? QStringLiteral("Unknown") : book.author),
                         escaped(language), modified, manifest, spine)
                    .toUtf8());
    zip.close();
    return bytes;
}

} // namespace sc
