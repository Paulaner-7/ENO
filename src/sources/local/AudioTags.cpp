#include "AudioTags.h"

#include <QFile>
#include <QStringDecoder>
#include <QtEndian>

namespace sc::local {

namespace {

constexpr qint64 kMaxBlock = 32 * 1024 * 1024;

quint32 be32(const char *p) { return qFromBigEndian<quint32>(p); }
quint32 le32(const char *p) { return qFromLittleEndian<quint32>(p); }
quint32 syncsafe(const char *p) {
    return (quint32(uchar(p[0]) & 0x7f) << 21) | (quint32(uchar(p[1]) & 0x7f) << 14) | (quint32(uchar(p[2]) & 0x7f) << 7) |
           (quint32(uchar(p[3]) & 0x7f));
}

QByteArray readAt(QIODevice &file, qint64 at, qint64 length) {
    if (length <= 0 || length > kMaxBlock || !file.seek(at))
        return {};
    return file.read(length);
}

// "3/12" → 3.
int leadingNumber(const QString &text) { return text.section(QLatin1Char('/'), 0, 0).trimmed().toInt(); }

void addArtists(AudioTags &tags, const QString &value) {
    for (const auto &part : value.split(QChar(0), Qt::SkipEmptyParts))
        if (!part.trimmed().isEmpty() && !tags.artists.contains(part.trimmed()))
            tags.artists.append(part.trimmed());
}

// ID3v2 ----------------------------------------------------------------

QString id3Text(const QByteArray &data, int encoding) {
    switch (encoding) {
    case 1: // UTF-16, its byte order from the BOM (which is dropped)
        return QStringDecoder(QStringDecoder::Utf16)(data);
    case 2:
        return QStringDecoder(QStringDecoder::Utf16BE)(data);
    case 3:
        return QString::fromUtf8(data);
    default:
        return QString::fromLatin1(data);
    }
}

// Where an encoded, terminated string ends (past its terminator).
qsizetype id3Skip(const QByteArray &data, qsizetype from, int encoding) {
    if (encoding == 1 || encoding == 2) {
        for (qsizetype i = from; i + 1 < data.size(); i += 2)
            if (data[i] == 0 && data[i + 1] == 0)
                return i + 2;
        return data.size();
    }
    const auto end = data.indexOf('\0', from);
    return end < 0 ? data.size() : end + 1;
}

QString id3Value(const QByteArray &body) {
    if (body.isEmpty())
        return {};
    auto text = id3Text(body.mid(1), uchar(body[0]));
    while (text.endsWith(QChar(0)))
        text.chop(1);
    return text;
}

// The tag at the start of `file`; returns where the audio begins.
qint64 readId3(QIODevice &file, AudioTags &tags, bool wantArt) {
    const auto header = readAt(file, 0, 10);
    if (header.size() < 10 || !header.startsWith("ID3"))
        return 0;
    const int version = uchar(header[3]);
    const int flags = uchar(header[5]);
    const qint64 size = syncsafe(header.constData() + 6);
    const qint64 end = 10 + size + ((flags & 0x10) ? 10 : 0);
    if (version < 2 || version > 4)
        return end;
    auto data = readAt(file, 10, size);
    // Version 3 unsynchronises the whole tag: FF 00 stands for FF.
    if (version <= 3 && (flags & 0x80))
        data.replace(QByteArray("\xff\x00", 2), QByteArray("\xff", 1));
    qsizetype at = 0;
    if (version >= 3 && (flags & 0x40) && data.size() >= 4)
        at = version == 4 ? syncsafe(data.constData()) : be32(data.constData()) + 4;
    const int idLength = version == 2 ? 3 : 4;
    const int headerLength = version == 2 ? 6 : 10;
    int pictureType = -1;
    while (at + headerLength <= data.size()) {
        const auto id = data.mid(at, idLength);
        if (id[0] == 0)
            break;
        qint64 length = version == 2 ? (quint32(uchar(data[at + 3])) << 16 | quint32(uchar(data[at + 4])) << 8 | uchar(data[at + 5]))
                      : version == 4 ? syncsafe(data.constData() + at + 4)
                                     : be32(data.constData() + at + 4);
        at += headerLength;
        if (length <= 0 || at + length > data.size())
            break;
        const auto body = data.mid(at, length);
        at += length;
        if (id == "TIT2" || id == "TT2") tags.title = id3Value(body);
        else if (id == "TPE1" || id == "TP1") addArtists(tags, id3Value(body));
        else if (id == "TALB" || id == "TAL") tags.album = id3Value(body);
        else if (id == "TPE2" || id == "TP2") tags.albumArtist = id3Value(body).section(QChar(0), 0, 0);
        else if (id == "TRCK" || id == "TRK") tags.track = leadingNumber(id3Value(body));
        else if (id == "TPOS" || id == "TPA") tags.disc = leadingNumber(id3Value(body));
        else if (id == "TLEN" || id == "TLE") tags.duration = id3Value(body).toDouble() / 1000;
        else if ((id == "USLT" || id == "ULT") && body.size() > 4 && tags.lyrics.isEmpty()) {
            const int encoding = uchar(body[0]);
            const auto text = id3Skip(body, 4, encoding);
            tags.lyrics = id3Text(body.mid(text), encoding).trimmed();
        } else if ((id == "TXXX" || id == "TXX") && body.size() > 2 && tags.lyrics.isEmpty()) {
            // Lyrics some taggers (and ffmpeg) keep as a user text frame.
            const int encoding = uchar(body[0]);
            const auto value = id3Skip(body, 1, encoding);
            const auto name = id3Text(body.mid(1, value - 1), encoding).remove(QChar(0)).toUpper();
            if (name == QLatin1String("LYRICS") || name == QLatin1String("UNSYNCEDLYRICS") || name == QLatin1String("USLT"))
                tags.lyrics = id3Text(body.mid(value), encoding).remove(QChar(0)).trimmed();
        } else if (wantArt && (id == "APIC" || id == "PIC") && body.size() > 4) {
            const int encoding = uchar(body[0]);
            qsizetype p = id == "PIC" ? 4 : body.indexOf('\0', 1) + 1;
            if (p <= 0 || p >= body.size())
                continue;
            const int type = uchar(body[p]);
            p = id3Skip(body, p + 1, encoding);
            // The front cover wins over any other picture.
            if (pictureType != 3 && p < body.size()) {
                tags.art = body.mid(p);
                pictureType = type;
            }
        }
    }
    return end;
}

// How long an MP3 plays: its Xing/Info or VBRI frame count, else the
// first frame's bitrate over the file's length.
double mp3Duration(QIODevice &file, qint64 audioStart) {
    const auto head = readAt(file, audioStart, 16 * 1024);
    for (qsizetype i = 0; i + 4 <= head.size(); ++i) {
        if (uchar(head[i]) != 0xff || (uchar(head[i + 1]) & 0xe0) != 0xe0)
            continue;
        const int b1 = uchar(head[i + 1]), b2 = uchar(head[i + 2]), b3 = uchar(head[i + 3]);
        const int versionBits = (b1 >> 3) & 3; // 0: 2.5, 2: 2, 3: 1
        const int layerBits = (b1 >> 1) & 3;   // 1: III, 2: II, 3: I
        const int bitrateIndex = b2 >> 4, rateIndex = (b2 >> 2) & 3;
        if (versionBits == 1 || layerBits == 0 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3)
            continue;
        static const int rates[3] = {44100, 48000, 32000};
        int rate = rates[rateIndex];
        if (versionBits == 2) rate /= 2;
        if (versionBits == 0) rate /= 4;
        const bool v1 = versionBits == 3;
        static const int l1[] = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448};
        static const int l2[] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
        static const int l3[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
        static const int v2l1[] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256};
        static const int v2l23[] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
        const int kbps = v1 ? (layerBits == 3 ? l1 : layerBits == 2 ? l2 : l3)[bitrateIndex]
                            : (layerBits == 3 ? v2l1 : v2l23)[bitrateIndex];
        const int samples = layerBits == 3 ? 384 : (layerBits == 1 && !v1) ? 576 : 1152;
        const bool mono = (b3 >> 6) == 3;
        const qsizetype side = v1 ? (mono ? 17 : 32) : (mono ? 9 : 17);
        const auto xing = head.mid(i + 4 + side, 12);
        if ((xing.startsWith("Xing") || xing.startsWith("Info")) && (be32(xing.constData() + 4) & 1))
            return double(be32(xing.constData() + 8)) * samples / rate;
        const auto vbri = head.mid(i + 36, 18);
        if (vbri.startsWith("VBRI"))
            return double(be32(vbri.constData() + 14)) * samples / rate;
        return double(file.size() - audioStart - i) * 8 / (kbps * 1000.0);
    }
    return 0;
}

// Vorbis comments and FLAC pictures --------------------------------------

void readPicture(const QByteArray &block, AudioTags &tags, int &pictureType) {
    if (block.size() < 32)
        return;
    const int type = int(be32(block.constData()));
    qsizetype p = 4;
    p += 4 + be32(block.constData() + p); // MIME type
    if (p + 4 > block.size()) return;
    p += 4 + be32(block.constData() + p); // description
    p += 16;                               // width, height, depth, colours
    if (p + 4 > block.size()) return;
    const qsizetype length = be32(block.constData() + p);
    p += 4;
    if (pictureType != 3 && p + length <= block.size()) {
        tags.art = block.mid(p, length);
        pictureType = type;
    }
}

void readComments(const QByteArray &data, AudioTags &tags, bool wantArt, int &pictureType) {
    if (data.size() < 8)
        return;
    qsizetype p = 4 + le32(data.constData()); // vendor
    if (p + 4 > data.size())
        return;
    quint32 count = le32(data.constData() + p);
    p += 4;
    for (; count > 0 && p + 4 <= data.size(); --count) {
        const qsizetype length = le32(data.constData() + p);
        p += 4;
        if (length < 0 || p + length > data.size())
            break;
        const auto entry = data.mid(p, length);
        p += length;
        const auto eq = entry.indexOf('=');
        if (eq <= 0)
            continue;
        const auto key = entry.left(eq).toUpper();
        if (key == "METADATA_BLOCK_PICTURE") {
            if (wantArt)
                readPicture(QByteArray::fromBase64(entry.mid(eq + 1)), tags, pictureType);
            continue;
        }
        const auto value = QString::fromUtf8(entry.mid(eq + 1)).trimmed();
        if (key == "TITLE") tags.title = value;
        else if (key == "ARTIST") addArtists(tags, value);
        else if (key == "ALBUM") tags.album = value;
        else if (key == "ALBUMARTIST" || key == "ALBUM ARTIST") tags.albumArtist = value;
        else if (key == "TRACKNUMBER") tags.track = leadingNumber(value);
        else if (key == "DISCNUMBER") tags.disc = leadingNumber(value);
        else if ((key == "LYRICS" || key == "UNSYNCEDLYRICS") && tags.lyrics.isEmpty()) tags.lyrics = value;
    }
}

void readFlac(QIODevice &file, qint64 at, AudioTags &tags, bool wantArt) {
    if (readAt(file, at, 4) != "fLaC")
        return;
    at += 4;
    int pictureType = -1;
    for (int blocks = 0; blocks < 128; ++blocks) {
        const auto header = readAt(file, at, 4);
        if (header.size() < 4)
            return;
        const int type = uchar(header[0]) & 0x7f;
        const qint64 length = quint32(uchar(header[1])) << 16 | quint32(uchar(header[2])) << 8 | uchar(header[3]);
        at += 4;
        if (type == 0) {
            const auto info = readAt(file, at, length);
            if (info.size() >= 18) {
                const auto *b = reinterpret_cast<const uchar *>(info.constData()) + 10;
                const quint32 rate = quint32(b[0]) << 12 | quint32(b[1]) << 4 | b[2] >> 4;
                const quint64 samples = quint64(b[3] & 0x0f) << 32 | quint64(b[4]) << 24 | quint64(b[5]) << 16 | quint64(b[6]) << 8 | b[7];
                if (rate > 0)
                    tags.duration = double(samples) / rate;
            }
        } else if (type == 4) {
            readComments(readAt(file, at, length), tags, wantArt, pictureType);
        } else if (type == 6 && wantArt) {
            readPicture(readAt(file, at, length), tags, pictureType);
        }
        at += length;
        if (uchar(header[0]) & 0x80)
            return;
    }
}

// Ogg: the first two packets (the stream's header, then its comments), and
// the last page's position for the length.
void readOgg(QIODevice &file, AudioTags &tags, bool wantArt) {
    QList<QByteArray> packets;
    QByteArray packet;
    qint64 at = 0;
    while (packets.size() < 2) {
        const auto header = readAt(file, at, 27);
        if (header.size() < 27 || !header.startsWith("OggS"))
            break;
        const int segments = uchar(header[26]);
        const auto table = readAt(file, at + 27, segments);
        if (table.size() < segments)
            break;
        qint64 bodyLength = 0;
        for (const char s : table)
            bodyLength += uchar(s);
        const auto body = readAt(file, at + 27 + segments, bodyLength);
        qsizetype p = 0;
        for (const char s : table) {
            packet += body.mid(p, uchar(s));
            p += uchar(s);
            if (uchar(s) < 255) {
                packets.append(packet);
                packet.clear();
                if (packets.size() == 2)
                    break;
            }
        }
        if (packet.size() > kMaxBlock)
            break;
        at += 27 + segments + bodyLength;
    }
    if (packets.isEmpty())
        return;
    const bool opus = packets[0].startsWith("OpusHead");
    double rate = 0;
    qint64 preSkip = 0;
    if (opus && packets[0].size() >= 12) {
        rate = 48000;
        preSkip = qFromLittleEndian<quint16>(packets[0].constData() + 10);
    } else if (packets[0].startsWith("\x01vorbis") && packets[0].size() >= 16) {
        rate = le32(packets[0].constData() + 12);
    }
    int pictureType = -1;
    if (packets.size() == 2) {
        if (opus && packets[1].startsWith("OpusTags"))
            readComments(packets[1].mid(8), tags, wantArt, pictureType);
        else if (packets[1].startsWith("\x03vorbis"))
            readComments(packets[1].mid(7), tags, wantArt, pictureType);
    }
    if (rate <= 0)
        return;
    const qint64 tail = qMin<qint64>(file.size(), 64 * 1024);
    const auto end = readAt(file, file.size() - tail, tail);
    const auto last = end.lastIndexOf("OggS");
    if (last >= 0 && last + 14 <= end.size()) {
        const auto granule = qFromLittleEndian<qint64>(end.constData() + last + 6);
        if (granule > preSkip)
            tags.duration = double(granule - preSkip) / rate;
    }
}

// MP4 / M4A --------------------------------------------------------------

struct Atom {
    QByteArray type;
    qsizetype body = 0;
    qsizetype end = 0;
};

QList<Atom> atoms(const QByteArray &data, qsizetype from, qsizetype to) {
    QList<Atom> out;
    while (from + 8 <= to) {
        qint64 size = be32(data.constData() + from);
        qsizetype header = 8;
        if (size == 1 && from + 16 <= to) {
            size = qFromBigEndian<qint64>(data.constData() + from + 8);
            header = 16;
        } else if (size == 0) {
            size = to - from;
        }
        if (size < header || from + size > to)
            break;
        out.append({data.mid(from + 4, 4), from + header, qsizetype(from + size)});
        from += size;
    }
    return out;
}

const Atom *child(const QList<Atom> &list, const char *type) {
    for (const auto &a : list)
        if (a.type == type)
            return &a;
    return nullptr;
}

void readMp4(QIODevice &file, AudioTags &tags, bool wantArt) {
    // The top level, read header by header: moov may sit after the audio.
    qint64 at = 0;
    QByteArray moov;
    while (at + 8 <= file.size()) {
        const auto header = readAt(file, at, 16);
        if (header.size() < 8)
            return;
        qint64 size = be32(header.constData());
        qint64 headerLength = 8;
        if (size == 1 && header.size() >= 16) {
            size = qFromBigEndian<qint64>(header.constData() + 8);
            headerLength = 16;
        } else if (size == 0) {
            size = file.size() - at;
        }
        if (size < headerLength)
            return;
        if (header.mid(4, 4) == "moov") {
            moov = readAt(file, at + headerLength, size - headerLength);
            break;
        }
        at += size;
    }
    if (moov.isEmpty())
        return;
    const auto top = atoms(moov, 0, moov.size());
    if (const auto *mvhd = child(top, "mvhd")) {
        const auto *p = moov.constData() + mvhd->body;
        const bool v1 = mvhd->end - mvhd->body >= 32 && p[0] == 1;
        const quint32 scale = be32(p + (v1 ? 20 : 12));
        const double length = v1 ? double(qFromBigEndian<quint64>(p + 24)) : double(be32(p + 16));
        if (scale > 0)
            tags.duration = length / scale;
    }
    const auto *udta = child(top, "udta");
    if (!udta)
        return;
    // Each list is kept while a pointer into it is in use.
    const auto inUdta = atoms(moov, udta->body, udta->end);
    const auto *meta = child(inUdta, "meta");
    if (!meta)
        return;
    // A full box (version and flags first), except in some QuickTime files.
    qsizetype metaBody = meta->body;
    if (metaBody + 8 <= meta->end && moov.mid(metaBody + 4, 4) != "hdlr")
        metaBody += 4;
    const auto inMeta = atoms(moov, metaBody, meta->end);
    const auto *ilst = child(inMeta, "ilst");
    if (!ilst)
        return;
    for (const auto &item : atoms(moov, ilst->body, ilst->end)) {
        const auto inItem = atoms(moov, item.body, item.end);
        const auto *data = child(inItem, "data");
        if (!data || data->end - data->body < 8)
            continue;
        const auto value = moov.mid(data->body + 8, data->end - data->body - 8);
        const auto text = [&] { return QString::fromUtf8(value).trimmed(); };
        const auto &t = item.type;
        if (t == "\xa9nam") tags.title = text();
        else if (t == "\xa9" "ART") addArtists(tags, text());
        else if (t == "\xa9" "alb") tags.album = text();
        else if (t == "aART") tags.albumArtist = text();
        else if (t == "\xa9lyr") tags.lyrics = text();
        else if (t == "trkn" && value.size() >= 4) tags.track = qFromBigEndian<quint16>(value.constData() + 2);
        else if (t == "disk" && value.size() >= 4) tags.disc = qFromBigEndian<quint16>(value.constData() + 2);
        else if (t == "covr" && wantArt && tags.art.isEmpty()) tags.art = value;
    }
}

} // namespace

bool isAudio(const QString &name) {
    static const QStringList kinds{QStringLiteral(".mp3"), QStringLiteral(".flac"), QStringLiteral(".ogg"), QStringLiteral(".oga"),
                                   QStringLiteral(".opus"), QStringLiteral(".m4a"), QStringLiteral(".aac"), QStringLiteral(".wav"),
                                   QStringLiteral(".wma"), QStringLiteral(".ape"), QStringLiteral(".wv"), QStringLiteral(".mka")};
    for (const auto &kind : kinds)
        if (name.endsWith(kind, Qt::CaseInsensitive))
            return true;
    return false;
}

AudioTags readAudioTags(QIODevice &file, const QString &name, bool wantArt) {
    AudioTags tags;
    const auto lower = name.toLower();
    if (lower.endsWith(QLatin1String(".mp3"))) {
        const auto audio = readId3(file, tags, wantArt);
        if (tags.duration <= 0)
            tags.duration = mp3Duration(file, audio);
    } else if (lower.endsWith(QLatin1String(".flac"))) {
        // Some taggers put an ID3 tag before FLAC's own.
        readFlac(file, readId3(file, tags, wantArt), tags, wantArt);
    } else if (lower.endsWith(QLatin1String(".ogg")) || lower.endsWith(QLatin1String(".oga")) || lower.endsWith(QLatin1String(".opus"))) {
        readOgg(file, tags, wantArt);
    } else if (lower.endsWith(QLatin1String(".m4a")) || lower.endsWith(QLatin1String(".mp4"))) {
        readMp4(file, tags, wantArt);
    } else if (lower.endsWith(QLatin1String(".aac"))) {
        readId3(file, tags, wantArt);
    }
    return tags;
}

AudioTags readAudioTags(const QString &path, const QString &name, bool wantArt) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return readAudioTags(file, name, wantArt);
}

} // namespace sc::local
