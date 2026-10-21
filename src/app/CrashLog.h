#pragma once

#include <QString>
#include <QVariantList>

namespace sc::crashlog {

// Crash reports people can send: the last stretch of SCANCORE's log is kept
// in memory (a ring, no disk writes while running), and a crash (a fatal
// signal, a Qt fatal error or an uncaught exception) writes it to a report
// in <app data>/crashes with what happened and where: version, system and a
// backtrace (module + offset for each frame, names where the system has
// them). The next start offers to share it.
//
// install() goes first in main(), once the application's name is set.
void install();

// The reports, newest first: [{file, when, summary, seen}].
QVariantList reports();
// One report's text.
QString read(const QString &file);
// Reports written since the last start that nobody has looked at yet.
QStringList unseen();
void markSeen(const QString &file);
void remove(const QString &file);
// Version, system and the recent log, for problems that are not crashes.
QString diagnostics();

} // namespace sc::crashlog
