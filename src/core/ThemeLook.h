#pragma once

#include <QJsonObject>

namespace sc::theme {

// The look a .sct theme file (version 2) carries beyond its colours and
// background, under "look":
//   {"mode": "dark", "accentName": "violet",
//    "settings": {"navStyle": "dock", "roundness": "round", "textScale": 1.1,
//                 "readerRailCurrent": "#000000", …},
//    "pages": {"home": {"order": […], "hidden": {…}, "look": {…}}, …}}
// Only known settings with sensible values and known pages are kept, so a
// shared file cannot set anything else; the same check runs on export and
// import.
QJsonObject sanitizeLook(const QJsonObject &look);

} // namespace sc::theme
