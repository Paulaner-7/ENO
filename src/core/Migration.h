#pragma once

#include <QString>
#include <QVariantList>

#include <optional>

namespace sc::migration {

// Moving a series to another source ("Move to another source"): the library
// entry, reading progress and tracker links of `fromId` go to `toId`, whose
// series row and chapter list must already be stored (the series page of
// the new source stores them when it loads).
struct Options {
    // Chapters read on the old source count as read on the new one, by
    // chapter number; everything below the furthest one read is read too,
    // since sources split and number chapters differently.
    bool progress = true;
    // Keep the old entry in the library as well (a copy, not a move).
    bool keepOld = false;
};

struct Result {
    // Chapters marked read on the new source.
    int read = 0;
    // The chapter to carry on from there, if any.
    QString nextKey;
};

Result move(const QString &fromId, const QString &toId, const Options &options = {});

// A chapter's number as a value ("12", "12.5", "0.003"); empty when it has
// none.
std::optional<double> chapterValue(const QString &number);

} // namespace sc::migration
