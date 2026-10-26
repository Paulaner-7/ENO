#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

namespace sc::archive {

// Unpacking the optional downloads (Piper, ONNX Runtime) in the app, so no
// system tool is needed (a Flatpak may have none). Each writes under `to`
// without the archive's top folder; `keep` (a path inside it) says which
// entries are wanted (all when unset). Throws sc::Error.
using Keep = std::function<bool(const QString &)>;

// A .tar.gz: files keep their mode, the libraries' links stay links.
void untarGz(const QByteArray &data, const QString &to, const Keep &keep = {});
void unzip(const QByteArray &data, const QString &to, const Keep &keep = {});

} // namespace sc::archive
