#pragma once

#include <QList>
#include <QRectF>
#include <QString>

namespace sc::translation {

// A line of text found on a page, in the picture's pixels.
struct TextLine {
    QRectF box;
    QString text;
};

// Lines that belong together (a speech bubble, a caption): what is
// translated as one and drawn over as one.
struct TextBlock {
    QRectF box;
    QString text;
    // Written top to bottom in columns (Japanese and Chinese manga).
    bool vertical = false;
};

// Groups lines into blocks: horizontal lines one under the other and
// overlapping sideways, or vertical columns side by side and overlapping
// up and down. Text is joined in reading order: lines top to bottom, columns
// right to left; with spaces unless the language writes without them (zh,
// ja). Blocks come in reading order (top to bottom; right to left for
// vertical writing).
QList<TextBlock> groupLines(const QList<TextLine> &lines, const QString &language);

// Whether a language writes words without spaces between them.
bool writesWithoutSpaces(const QString &language);

} // namespace sc::translation
