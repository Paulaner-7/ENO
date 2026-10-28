#include "Subtitles.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace sc::translation {

namespace {

// "HH:MM:SS.mmm", "MM:SS,mmm" or ASS's "H:MM:SS.cc", in milliseconds. -1
// when it is not a time.
qint64 milliseconds(const QString &value) {
    static const QRegularExpression pattern(QStringLiteral(R"((?:(\d+):)?(\d{1,2}):(\d{1,2})(?:[.,](\d{1,3}))?)"));
    const auto match = pattern.match(value.trimmed());
    if (!match.hasMatch())
        return -1;
    const qint64 hours = match.captured(1).toLongLong();
    const qint64 minutes = match.captured(2).toLongLong();
    const qint64 seconds = match.captured(3).toLongLong();
    const auto fraction = match.captured(4).leftJustified(3, QLatin1Char('0')).left(3);
    return ((hours * 60 + minutes) * 60 + seconds) * 1000 + fraction.toLongLong();
}

QString decodeEntities(QString text) {
    text.replace(QLatin1String("&nbsp;"), QStringLiteral(" "));
    text.replace(QLatin1String("&amp;"), QStringLiteral("&"));
    text.replace(QLatin1String("&lt;"), QStringLiteral("<"));
    text.replace(QLatin1String("&gt;"), QStringLiteral(">"));
    text.replace(QLatin1String("&quot;"), QStringLiteral("\""));
    text.replace(QLatin1String("&apos;"), QStringLiteral("'"));

    static const QRegularExpression numeric(QStringLiteral("&#(x?)([0-9A-Fa-f]+);"));
    QString out;
    int last = 0;
    auto matches = numeric.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        out += text.mid(last, match.capturedStart() - last);
        bool ok = false;
        const uint code = match.captured(2).toUInt(&ok, match.captured(1).isEmpty() ? 10 : 16);
        if (ok && code > 0 && code <= 0x10FFFF)
            out += code <= 0xFFFF ? QString(QChar(char16_t(code))) : QString::fromUcs4(&code, 1);
        else
            out += match.captured(0);
        last = match.capturedEnd();
    }
    out += text.mid(last);
    return out;
}

// A cue's text without the format's markup: WebVTT/SRT tags, ASS overrides,
// HTML line breaks and character references.
QString withoutMarkup(QString text) {
    static const QRegularExpression lineBreak(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tag(QStringLiteral("</?(?:i|b|u|s|em|strong|c|v|ruby|rt|lang|font|span)(?:[. ][^>]*)?>"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression karaoke(QStringLiteral("<\\d{1,2}:\\d{2}(?::\\d{2})?[.,]\\d{1,3}>"));
    static const QRegularExpression override(QStringLiteral("\\{[^}]*\\}"));
    static const QRegularExpression blankLines(QStringLiteral("\\n\\s*\\n+"));
    text.replace(lineBreak, QStringLiteral("\n"));
    text.remove(tag);
    text.remove(karaoke);
    text.remove(override);
    text = decodeEntities(text);
    text.replace(blankLines, QStringLiteral("\n"));
    QStringList lines;
    for (auto line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (!line.isEmpty())
            lines.append(line);
    }
    return lines.join(QLatin1Char('\n'));
}

void parseCueBlocks(const QString &text, QList<SubtitleCue> &out) {
    static const QRegularExpression blockGap(QStringLiteral("\\n\\s*\\n"));
    for (const auto &block : text.split(blockGap, Qt::SkipEmptyParts)) {
        const auto lines = block.split(QLatin1Char('\n'));
        int timing = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains(QLatin1String("-->"))) {
                timing = i;
                break;
            }
        }
        if (timing < 0)
            continue;
        const auto arrow = lines[timing].indexOf(QLatin1String("-->"));
        const auto start = milliseconds(lines[timing].left(arrow));
        auto rest = lines[timing].mid(arrow + 3).trimmed();
        const auto space = rest.indexOf(QRegularExpression(QStringLiteral("\\s")));
        const auto end = milliseconds(space < 0 ? rest : rest.left(space));
        if (start < 0 || end <= start)
            continue;
        QStringList body;
        for (int i = timing + 1; i < lines.size(); ++i)
            body.append(lines[i]);
        const auto cue = withoutMarkup(body.join(QLatin1Char('\n')));
        if (cue.isEmpty())
            continue;
        out.append({start, end, cue});
    }
}

int fieldIndex(const QStringList &format, const QString &name, int fallback) {
    for (int i = 0; i < format.size(); ++i)
        if (format[i].compare(name, Qt::CaseInsensitive) == 0)
            return i;
    return fallback;
}

void parseAss(const QString &text, QList<SubtitleCue> &out) {
    static const QStringList defaultFormat = {QStringLiteral("Layer"), QStringLiteral("Start"),       QStringLiteral("End"),
                                              QStringLiteral("Style"), QStringLiteral("Name"),        QStringLiteral("MarginL"),
                                              QStringLiteral("MarginR"), QStringLiteral("MarginV"),    QStringLiteral("Effect"),
                                              QStringLiteral("Text")};
    bool events = false;
    QStringList format;
    for (const auto &raw : text.split(QLatin1Char('\n'))) {
        const auto line = raw.trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            events = line.compare(QLatin1String("[Events]"), Qt::CaseInsensitive) == 0;
            continue;
        }
        if (!events)
            continue;
        if (line.startsWith(QLatin1String("Format:"), Qt::CaseInsensitive)) {
            format = line.mid(7).split(QLatin1Char(','), Qt::KeepEmptyParts);
            for (auto &field : format)
                field = field.trimmed();
            continue;
        }
        if (!line.startsWith(QLatin1String("Dialogue:"), Qt::CaseInsensitive))
            continue;
        const auto fields = format.isEmpty() ? defaultFormat : format;
        const auto parts = line.mid(9).trimmed().split(QLatin1Char(','), Qt::KeepEmptyParts);
        if (parts.size() < 3)
            continue;
        const int startIndex = fieldIndex(fields, QStringLiteral("Start"), 1);
        const int endIndex = fieldIndex(fields, QStringLiteral("End"), 2);
        const int styleIndex = fieldIndex(fields, QStringLiteral("Style"), 3);
        const int nameIndex = fieldIndex(fields, QStringLiteral("Name"), 4);
        const int textIndex = fieldIndex(fields, QStringLiteral("Text"), fields.size() - 1);
        if (startIndex >= parts.size() || endIndex >= parts.size() || textIndex >= parts.size())
            continue;
        const auto start = milliseconds(parts[startIndex]);
        const auto end = milliseconds(parts[endIndex]);
        if (start < 0 || end <= start)
            continue;
        auto body = parts.mid(textIndex).join(QLatin1Char(','));
        // Skip ASS vector drawing commands ({\p1}m 0 0 l 100 0 ... {\p0}).
        static const QRegularExpression drawingTag(QStringLiteral(R"(\{[^}]*\\p[1-9][^}]*\})"));
        if (drawingTag.match(body).hasMatch())
            continue;
        // Skip frame-by-frame typeset signs/karaoke effects and non-dialogue styles.
        const QString style = styleIndex < parts.size() ? parts[styleIndex].trimmed() : QString();
        const QString actor = nameIndex < parts.size() ? parts[nameIndex].trimmed() : QString();
        static const QRegularExpression signStyle(QStringLiteral(R"(^(?:signs?|ts|typeset|op|ed|kara|romaji|kanji|songs?|title|episode\s*title|logo|credits?)$)"),
                                                  QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression posTag(QStringLiteral(R"(\{[^}]*\\(?:pos|move|org|fad)\s*\([^}]*\})"));
        static const QRegularExpression trackTag(QStringLiteral(R"(\{=[0-9]+\})"));
        if (signStyle.match(style).hasMatch() || trackTag.match(body).hasMatch() ||
            (end - start <= 150 && posTag.match(body).hasMatch()) ||
            (actor.compare(QLatin1String("Sign"), Qt::CaseInsensitive) == 0 && posTag.match(body).hasMatch()))
            continue;
        body.replace(QLatin1String("\\N"), QStringLiteral("\n"));
        body.replace(QLatin1String("\\n"), QStringLiteral("\n"));
        body.replace(QLatin1String("\\h"), QStringLiteral(" "));
        const auto cue = withoutMarkup(body);
        if (cue.isEmpty())
            continue;
        out.append({start, end, cue});
    }
    std::stable_sort(out.begin(), out.end(), [](const SubtitleCue &a, const SubtitleCue &b) {
        return a.start < b.start;
    });
    QList<SubtitleCue> mergedOut;
    mergedOut.reserve(out.size());
    for (const auto &c : std::as_const(out)) {
        bool merged = false;
        for (qsizetype k = mergedOut.size() - 1; k >= std::max<qsizetype>(0, mergedOut.size() - 16); --k) {
            if (mergedOut[k].text == c.text && c.start <= mergedOut[k].end + 500 && c.end >= mergedOut[k].start) {
                mergedOut[k].start = std::min(mergedOut[k].start, c.start);
                mergedOut[k].end = std::max(mergedOut[k].end, c.end);
                merged = true;
                break;
            }
        }
        if (!merged)
            mergedOut.append(c);
    }
    out = std::move(mergedOut);
}

QString stamp(qint64 milliseconds) {
    const qint64 value = std::max<qint64>(0, milliseconds);
    const qint64 hours = value / 3600000;
    const qint64 minutes = (value / 60000) % 60;
    const qint64 seconds = (value / 1000) % 60;
    const qint64 fraction = value % 1000;
    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(fraction, 3, 10, QLatin1Char('0'));
}

} // namespace

QList<SubtitleCue> parseSubtitles(const QByteArray &data) {
    QString text = QString::fromUtf8(data);
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    text.replace(QLatin1String("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QList<SubtitleCue> out;
    if (text.contains(QLatin1String("[Script Info]"), Qt::CaseInsensitive) || text.contains(QLatin1String("[Events]"), Qt::CaseInsensitive))
        parseAss(text, out);
    else
        parseCueBlocks(text, out);
    return out;
}

QString toWebVtt(const QList<SubtitleCue> &cues) {
    QString out = QStringLiteral("WEBVTT\n\n");
    for (auto cue : cues) {
        if (cue.end <= cue.start)
            continue;
        auto text = cue.text;
        text.replace(QLatin1Char('\r'), QString());
        text.replace(QRegularExpression(QStringLiteral("\\n\\s*\\n+")), QStringLiteral("\n"));
        text.replace(QLatin1String("-->"), QStringLiteral("→"));
        text = text.trimmed();
        if (text.isEmpty())
            continue;
        out += stamp(cue.start) + QStringLiteral(" --> ") + stamp(cue.end) + QLatin1Char('\n') + text + QStringLiteral("\n\n");
    }
    return out;
}

QString plainSubtitleText(const QString &text) {
    auto out = text;
    out.replace(QLatin1Char('\n'), QLatin1Char(' '));
    out.replace(QLatin1Char('\t'), QLatin1Char(' '));
    return out.simplified();
}

} // namespace sc::translation
