#pragma once

#include <QString>

namespace sc {

// Keeps the screen on and the computer from going idle while something is
// worth watching (a playing video): the screen would otherwise dim, lock or
// sleep mid-episode. Android: the window's FLAG_KEEP_SCREEN_ON. Linux: the
// desktop's screensaver inhibit over D-Bus (org.freedesktop.ScreenSaver, or
// the portal's Inhibit in a sandbox). Windows: SetThreadExecutionState.
// macOS: an IOPM assertion against display sleep. Each holder has its own
// key; the screen stays on while any holds it. GUI thread only.
namespace keepAwake {

void hold(const QString &holder, bool on);

} // namespace keepAwake
} // namespace sc
