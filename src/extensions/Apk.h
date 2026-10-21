#pragma once

#include <QByteArray>
#include <QStringList>

namespace sc::ext {

// The SHA-256 fingerprints (lower-case hex) of the certificates an APK is
// signed with, as Mihon compares them with a repository's signing key: from
// the APK Signing Block (scheme v3, else v2), else from the v1 JAR
// signature. Empty for an unsigned or unreadable file.
//
// This reads which key signed the file; it does not check the signature
// itself. The extension hosts do that before running anything (the JVM
// verifies on desktop, the package manager on Android).
QStringList apkSigners(const QByteArray &apk);
QStringList apkSignersOfFile(const QString &path);

// The first certificate in a PKCS#7 SignedData blob (META-INF/*.RSA), DER.
QByteArray pkcs7Certificate(const QByteArray &der);

} // namespace sc::ext
