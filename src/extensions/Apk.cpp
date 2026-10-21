#include "Apk.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QtCore/private/qzipreader_p.h>

#include <optional>

namespace sc::ext {

namespace {

constexpr quint32 kV2 = 0x7109871a;
constexpr quint32 kV3 = 0xf05368c0;
constexpr quint32 kV31 = 0x1b93ad61;

// Little-endian, length-prefixed fields of the APK Signing Block.
class Reader {
public:
    explicit Reader(QByteArrayView data) : m_data(data) {}
    bool atEnd() const { return m_at >= m_data.size(); }
    bool ok() const { return m_ok; }
    quint64 number(int bytes) {
        if (m_data.size() - m_at < bytes) {
            m_ok = false;
            m_at = m_data.size();
            return 0;
        }
        quint64 value = 0;
        for (int i = 0; i < bytes; ++i)
            value |= quint64(uchar(m_data[m_at + i])) << (8 * i);
        m_at += bytes;
        return value;
    }
    QByteArrayView take(quint64 size) {
        if (quint64(m_data.size() - m_at) < size) {
            m_ok = false;
            m_at = m_data.size();
            return {};
        }
        const auto out = m_data.sliced(m_at, qsizetype(size));
        m_at += qsizetype(size);
        return out;
    }
    // A uint32 length and that many bytes.
    QByteArrayView prefixed() { return take(number(4)); }

private:
    QByteArrayView m_data;
    qsizetype m_at = 0;
    bool m_ok = true;
};

quint64 le(QByteArrayView data, qsizetype at, int bytes) {
    quint64 value = 0;
    for (int i = 0; i < bytes; ++i)
        value |= quint64(uchar(data[at + i])) << (8 * i);
    return value;
}

QString sha256(QByteArrayView der) {
    return QString::fromLatin1(QCryptographicHash::hash(der, QCryptographicHash::Sha256).toHex());
}

// The values of the APK Signing Block, by id.
std::optional<QHash<quint32, QByteArrayView>> signingBlock(QByteArrayView apk) {
    // End of central directory: 22 bytes plus a comment of up to 64 KB.
    qsizetype eocd = -1;
    for (qsizetype at = apk.size() - 22; at >= 0 && at >= apk.size() - 22 - 65535; --at)
        if (le(apk, at, 4) == 0x06054b50 && qsizetype(le(apk, at + 20, 2)) == apk.size() - at - 22) {
            eocd = at;
            break;
        }
    if (eocd < 0)
        return std::nullopt;
    const auto directory = qsizetype(le(apk, eocd + 16, 4));
    if (directory < 32 || directory > eocd || apk.sliced(directory - 16, 16) != QByteArrayView("APK Sig Block 42"))
        return std::nullopt;
    const auto size = le(apk, directory - 24, 8);
    if (size < 24 || size > quint64(directory - 8))
        return std::nullopt;
    const auto start = directory - qsizetype(size) - 8;
    if (le(apk, start, 8) != size)
        return std::nullopt;
    QHash<quint32, QByteArrayView> values;
    Reader pairs(apk.sliced(start + 8, qsizetype(size) - 24));
    while (!pairs.atEnd() && pairs.ok()) {
        const auto length = pairs.number(8);
        Reader pair(pairs.take(length));
        const auto id = quint32(pair.number(4));
        const auto value = pair.take(length - 4);
        if (pair.ok())
            values.insert(id, value);
    }
    return values;
}

// v2 and v3 share the layout up to the certificates: signers, each with
// signed data holding digests, then certificates.
QStringList schemeSigners(QByteArrayView value) {
    QStringList out;
    Reader all(value);
    Reader signers(all.prefixed());
    while (!signers.atEnd() && signers.ok()) {
        Reader signer(signers.prefixed());
        Reader signedData(signer.prefixed());
        signedData.prefixed(); // digests
        Reader certificates(signedData.prefixed());
        const auto first = certificates.prefixed();
        if (signedData.ok() && certificates.ok() && !first.isEmpty())
            out.append(sha256(first));
    }
    return out;
}

// One DER element: its tag, and where its content starts and ends.
struct Element {
    uchar tag = 0;
    qsizetype start = 0;   // first byte of the element
    qsizetype content = 0; // first byte of its content
    qsizetype end = 0;     // one past its last byte
};

std::optional<Element> element(QByteArrayView der, qsizetype at) {
    if (at + 2 > der.size())
        return std::nullopt;
    Element e;
    e.start = at;
    e.tag = uchar(der[at]);
    qsizetype length = uchar(der[at + 1]);
    qsizetype header = 2;
    if (length & 0x80) {
        const int bytes = int(length & 0x7f);
        if (bytes < 1 || bytes > 4 || at + 2 + bytes > der.size())
            return std::nullopt;
        length = 0;
        for (int i = 0; i < bytes; ++i)
            length = (length << 8) | uchar(der[at + 2 + i]);
        header += bytes;
    }
    e.content = at + header;
    e.end = e.content + length;
    if (e.end > der.size())
        return std::nullopt;
    return e;
}

} // namespace

QByteArray pkcs7Certificate(const QByteArray &der) {
    // ContentInfo ::= SEQUENCE { contentType OID, content [0] EXPLICIT
    // SignedData ::= SEQUENCE { version, digestAlgorithms SET, contentInfo,
    // certificates [0] IMPLICIT SET OF Certificate, ... } }
    const auto info = element(der, 0);
    if (!info || info->tag != 0x30)
        return {};
    const auto type = element(der, info->content);
    if (!type)
        return {};
    const auto wrapper = element(der, type->end);
    if (!wrapper || wrapper->tag != 0xa0)
        return {};
    const auto signedData = element(der, wrapper->content);
    if (!signedData || signedData->tag != 0x30)
        return {};
    auto at = signedData->content;
    for (int skip = 0; skip < 3; ++skip) {
        const auto e = element(der, at);
        if (!e)
            return {};
        at = e->end;
    }
    const auto certificates = element(der, at);
    if (!certificates || certificates->tag != 0xa0)
        return {};
    const auto first = element(der, certificates->content);
    if (!first || first->tag != 0x30)
        return {};
    return der.mid(first->start, first->end - first->start);
}

QStringList apkSigners(const QByteArray &apk) {
    if (const auto block = signingBlock(apk)) {
        for (const auto id : {kV31, kV3, kV2})
            if (block->contains(id))
                if (auto signers = schemeSigners(block->value(id)); !signers.isEmpty())
                    return signers;
    }
    // v1: the PKCS#7 signature files in META-INF.
    if (!apk.startsWith("PK") || !apk.right(65557).contains(QByteArrayView("PK\x05\x06")))
        return {};
    QByteArray copy = apk;
    QBuffer buffer(&copy);
    buffer.open(QIODevice::ReadOnly);
    QZipReader zip(&buffer);
    if (!zip.isReadable())
        return {};
    QStringList out;
    for (const auto &info : zip.fileInfoList()) {
        const auto name = info.filePath.toUpper();
        if (!name.startsWith(QLatin1String("META-INF/")) || name.count(QLatin1Char('/')) != 1 ||
            !(name.endsWith(QLatin1String(".RSA")) || name.endsWith(QLatin1String(".DSA")) || name.endsWith(QLatin1String(".EC"))))
            continue;
        const auto certificate = pkcs7Certificate(zip.fileData(info.filePath));
        if (!certificate.isEmpty() && !out.contains(sha256(certificate)))
            out.append(sha256(certificate));
    }
    return out;
}

QStringList apkSignersOfFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return apkSigners(file.readAll());
}

} // namespace sc::ext
