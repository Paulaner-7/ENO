#pragma once

#include <QByteArray>
#include <optional>

namespace sc {

// AES-256 in CBC mode, decryption only, for sites that encrypt what they
// list (chapter lists) with CryptoJS. PKCS#7 padding is checked and taken
// off; nullopt when the key does not fit (wrong key, damaged data).
std::optional<QByteArray> aes256CbcDecrypt(const QByteArray &key, const QByteArray &iv, const QByteArray &data);

// CryptoJS.AES.decrypt(data, passphrase): the key and IV come from the
// passphrase and an 8-byte salt the way OpenSSL derives them
// (EVP_BytesToKey with MD5).
std::optional<QByteArray> cryptoJsDecrypt(const QByteArray &passphrase, const QByteArray &salt, const QByteArray &data);

} // namespace sc
