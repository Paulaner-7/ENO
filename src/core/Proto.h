#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <cstring>

namespace sc {

// Just enough of protobuf's wire format for Mihon's backups and extension
// store indexes: walk the fields of one message and read their values.
class Proto {
public:
    explicit Proto(QByteArrayView data) : m_data(data) {}

    bool next() {
        if (m_at >= m_data.size() || m_bad)
            return false;
        const auto tag = varint();
        m_field = int(tag >> 3);
        m_wire = int(tag & 7);
        return !m_bad;
    }
    int field() const { return m_field; }
    // 0 varint, 1 fixed64, 2 length-delimited, 5 fixed32.
    int wire() const { return m_wire; }

    quint64 varint() {
        quint64 value = 0;
        for (int shift = 0; shift < 64 && m_at < m_data.size(); shift += 7) {
            const auto byte = uchar(m_data[m_at++]);
            value |= quint64(byte & 0x7f) << shift;
            if (!(byte & 0x80))
                return value;
        }
        m_bad = true;
        return 0;
    }
    QByteArrayView bytes() {
        const auto size = qsizetype(varint());
        if (m_bad || size < 0 || size > m_data.size() - m_at) {
            m_bad = true;
            return {};
        }
        const auto out = m_data.sliced(m_at, size);
        m_at += size;
        return out;
    }
    QString text() { return QString::fromUtf8(bytes()); }
    float fixed32() {
        if (m_data.size() - m_at < 4) {
            m_bad = true;
            return 0;
        }
        quint32 raw = 0;
        for (int i = 0; i < 4; ++i)
            raw |= quint32(uchar(m_data[m_at + i])) << (8 * i);
        m_at += 4;
        float value;
        std::memcpy(&value, &raw, sizeof value);
        return value;
    }
    // A number in either encoding (floats are fixed32).
    double number() { return m_wire == 5 ? fixed32() : m_wire == 0 ? double(varint()) : (skip(), 0.0); }
    void skip() {
        switch (m_wire) {
        case 0: varint(); break;
        case 1: m_at += 8; break;
        case 2: bytes(); break;
        case 5: m_at += 4; break;
        default: m_bad = true;
        }
        if (m_at > m_data.size())
            m_bad = true;
    }
    bool bad() const { return m_bad; }
    // Bytes left (for packed repeated values, read with varint()).
    bool more() const { return m_at < m_data.size() && !m_bad; }

private:
    QByteArrayView m_data;
    qsizetype m_at = 0;
    int m_field = 0;
    int m_wire = 0;
    bool m_bad = false;
};

// Writes protobuf messages (Mihon's backups): fields in the order added.
class ProtoWriter {
public:
    void varint(int field, quint64 value) {
        tag(field, 0);
        raw(value);
    }
    void boolean(int field, bool value) { varint(field, value ? 1 : 0); }
    void bytes(int field, QByteArrayView data) {
        tag(field, 2);
        raw(quint64(data.size()));
        m_out.append(data.data(), data.size());
    }
    // Empty strings are left out, as protobuf does.
    void text(int field, const QString &value) {
        if (!value.isEmpty())
            bytes(field, value.toUtf8());
    }
    void fixed32(int field, float value) {
        tag(field, 5);
        quint32 bits;
        std::memcpy(&bits, &value, sizeof bits);
        for (int i = 0; i < 4; ++i)
            m_out.append(char(bits >> (8 * i) & 0xff));
    }
    void message(int field, const ProtoWriter &inner) { bytes(field, inner.data()); }
    const QByteArray &data() const { return m_out; }

private:
    void tag(int field, int wire) { raw(quint64(field) << 3 | quint64(wire)); }
    void raw(quint64 value) {
        do {
            const auto byte = char(value & 0x7f);
            value >>= 7;
            m_out.append(value ? char(byte | 0x80) : byte);
        } while (value);
    }
    QByteArray m_out;
};

} // namespace sc
