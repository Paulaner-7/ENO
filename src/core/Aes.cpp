#include "Aes.h"

#include <QCryptographicHash>

#include <array>
#include <cstdint>

namespace sc {

namespace {

using Block = std::array<uint8_t, 16>;

constexpr uint8_t kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59,
    0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1,
    0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83,
    0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
    0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c,
    0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
    0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee,
    0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6,
    0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9,
    0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1,
    0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

struct Tables {
    uint8_t inv[256];
    Tables() {
        for (int i = 0; i < 256; ++i)
            inv[kSbox[i]] = uint8_t(i);
    }
};
const Tables &tables() {
    static const Tables t;
    return t;
}

uint8_t xtime(uint8_t x) { return uint8_t((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }
uint8_t mul(uint8_t x, uint8_t y) {
    uint8_t r = 0;
    while (y) {
        if (y & 1)
            r ^= x;
        x = xtime(x);
        y >>= 1;
    }
    return r;
}

// 15 round keys of 16 bytes from a 32-byte key.
std::array<uint8_t, 240> expand(const uint8_t *key) {
    std::array<uint8_t, 240> w{};
    std::copy(key, key + 32, w.begin());
    uint8_t rcon = 1;
    for (int i = 8; i < 60; ++i) {
        uint8_t t[4] = {w[(i - 1) * 4], w[(i - 1) * 4 + 1], w[(i - 1) * 4 + 2], w[(i - 1) * 4 + 3]};
        if (i % 8 == 0) {
            const uint8_t first = t[0];
            t[0] = uint8_t(kSbox[t[1]] ^ rcon);
            t[1] = kSbox[t[2]];
            t[2] = kSbox[t[3]];
            t[3] = kSbox[first];
            rcon = xtime(rcon);
        } else if (i % 8 == 4) {
            for (auto &b : t)
                b = kSbox[b];
        }
        for (int j = 0; j < 4; ++j)
            w[i * 4 + j] = uint8_t(w[(i - 8) * 4 + j] ^ t[j]);
    }
    return w;
}

void decryptBlock(Block &s, const std::array<uint8_t, 240> &w) {
    const auto &inv = tables().inv;
    const auto addKey = [&](int round) {
        for (int i = 0; i < 16; ++i)
            s[i] ^= w[round * 16 + i];
    };
    addKey(14);
    for (int round = 13; round >= 0; --round) {
        // InvShiftRows (column-major state: s[c*4 + r]).
        Block t = s;
        for (int r = 1; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                s[((c + r) % 4) * 4 + r] = t[c * 4 + r];
        for (auto &b : s)
            b = inv[b];
        addKey(round);
        if (round == 0)
            break;
        for (int c = 0; c < 4; ++c) {
            const uint8_t a0 = s[c * 4], a1 = s[c * 4 + 1], a2 = s[c * 4 + 2], a3 = s[c * 4 + 3];
            s[c * 4] = mul(a0, 14) ^ mul(a1, 11) ^ mul(a2, 13) ^ mul(a3, 9);
            s[c * 4 + 1] = mul(a0, 9) ^ mul(a1, 14) ^ mul(a2, 11) ^ mul(a3, 13);
            s[c * 4 + 2] = mul(a0, 13) ^ mul(a1, 9) ^ mul(a2, 14) ^ mul(a3, 11);
            s[c * 4 + 3] = mul(a0, 11) ^ mul(a1, 13) ^ mul(a2, 9) ^ mul(a3, 14);
        }
    }
}

} // namespace

std::optional<QByteArray> aes256CbcDecrypt(const QByteArray &key, const QByteArray &iv, const QByteArray &data) {
    if (key.size() != 32 || iv.size() != 16 || data.isEmpty() || data.size() % 16)
        return std::nullopt;
    const auto w = expand(reinterpret_cast<const uint8_t *>(key.constData()));
    QByteArray out(data.size(), Qt::Uninitialized);
    Block previous;
    std::copy(iv.begin(), iv.end(), previous.begin());
    for (qsizetype at = 0; at < data.size(); at += 16) {
        Block block;
        std::copy(data.begin() + at, data.begin() + at + 16, block.begin());
        Block cipher = block;
        decryptBlock(block, w);
        for (int i = 0; i < 16; ++i)
            out[at + i] = char(block[i] ^ previous[i]);
        previous = cipher;
    }
    const int pad = uint8_t(out.back());
    if (pad < 1 || pad > 16)
        return std::nullopt;
    for (int i = 1; i <= pad; ++i)
        if (uint8_t(out[out.size() - i]) != pad)
            return std::nullopt;
    out.chop(pad);
    return out;
}

std::optional<QByteArray> cryptoJsDecrypt(const QByteArray &passphrase, const QByteArray &salt, const QByteArray &data) {
    QByteArray derived, previous;
    while (derived.size() < 48) {
        previous = QCryptographicHash::hash(previous + passphrase + salt, QCryptographicHash::Md5);
        derived += previous;
    }
    return aes256CbcDecrypt(derived.left(32), derived.mid(32, 16), data);
}

} // namespace sc
