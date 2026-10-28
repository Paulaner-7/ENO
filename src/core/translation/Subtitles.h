#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace sc::translation {

// One subtitle line, its times in milliseconds from the start of the video.
struct SubtitleCue {
    qint64 start = 0;
    qint64 end = 0;
    QString text;
};

// The cues of a WebVTT, SubRip (SRT) or ASS/SSA track, in file order. Blocks
// without text (headers, styles, comments) are skipped.
QList<SubtitleCue> parseSubtitles(const QByteArray &data);

// The cues as a WebVTT track, which a browser plays as a text track.
QString toWebVtt(const QList<SubtitleCue> &cues);

// One cue's text with its styling removed and its lines joined by a space,
// ready to be translated.
QString plainSubtitleText(const QString &text);

} // namespace sc::translation
