#include "mcps2/seed_hash.hpp"
#include "mcps2/java_bits.hpp"
#include <cstddef>

namespace mcps2 {
namespace {
// RFC 1321's mathematical sine table. Authored loop implementation, no Mojang source.
constexpr uint32_t sine_table[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
constexpr uint8_t shifts[4][4] = {{7,12,17,22},{5,9,14,20},{4,11,16,23},{6,10,15,21}};
class SeedDigest {
public:
    void byte(uint8_t value) {
        block_[used_++] = value; ++length_;
        if (used_ == 64) { compress(); used_ = 0; }
    }
    Seed128 finish() {
        const uint64_t bit_length = length_ * 8;
        byte(0x80);
        while (used_ != 56) byte(0);
        for (unsigned i = 0; i < 8; ++i) byte(uint8_t(bit_length >> (i * 8)));
        uint64_t halves[2]{};
        for (unsigned i = 0; i < 16; ++i) {
            const uint8_t value = uint8_t(state_[i / 4] >> ((i % 4) * 8));
            halves[i / 8] = (halves[i / 8] << 8) | value;
        }
        return {halves[0], halves[1]};
    }
private:
    void compress() {
        uint32_t words[16]{};
        for (unsigned i = 0; i < 64; ++i) words[i / 4] |= uint32_t(block_[i]) << ((i % 4) * 8);
        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        for (unsigned i = 0; i < 64; ++i) {
            const unsigned round = i / 16;
            uint32_t function; unsigned index;
            if (round == 0) { function = (b & c) | (~b & d); index = i; }
            else if (round == 1) { function = (d & b) | (~d & c); index = (5 * i + 1) % 16; }
            else if (round == 2) { function = b ^ c ^ d; index = (3 * i + 5) % 16; }
            else { function = c ^ (b | ~d); index = (7 * i) % 16; }
            const uint32_t sum = a + function + sine_table[i] + words[index];
            const unsigned shift = shifts[round][i % 4];
            const uint32_t rotated = (sum << shift) | (sum >> (32 - shift));
            a = d; d = c; c = b; b += rotated;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    }
    uint32_t state_[4] = {0x67452301,0xefcdab89,0x98badcfe,0x10325476};
    uint8_t block_[64]{};
    unsigned used_ = 0;
    uint64_t length_ = 0;
};
void codepoint(SeedDigest& digest, uint32_t value) {
    if (value < 0x80) digest.byte(uint8_t(value));
    else if (value < 0x800) {
        digest.byte(uint8_t(0xc0 | (value >> 6))); digest.byte(uint8_t(0x80 | (value & 63)));
    } else if (value < 0x10000) {
        digest.byte(uint8_t(0xe0 | (value >> 12))); digest.byte(uint8_t(0x80 | ((value >> 6) & 63)));
        digest.byte(uint8_t(0x80 | (value & 63)));
    } else {
        digest.byte(uint8_t(0xf0 | (value >> 18))); digest.byte(uint8_t(0x80 | ((value >> 12) & 63)));
        digest.byte(uint8_t(0x80 | ((value >> 6) & 63))); digest.byte(uint8_t(0x80 | (value & 63)));
    }
}
}
uint32_t java_string_hash(std::u16string_view text) {
    uint32_t value = 0;
    for (char16_t unit : text) value = value * 31 + uint32_t(unit);
    return value;
}
Seed128 seed_from_utf8(std::string_view bytes) {
    SeedDigest digest;
    for (unsigned char value : bytes) digest.byte(value);
    return digest.finish();
}
Seed128 seed_from_java_string(std::u16string_view text) {
    SeedDigest digest;
    for (size_t i = 0; i < text.size(); ++i) {
        uint32_t value = uint32_t(text[i]);
        if (value >= 0xd800 && value <= 0xdbff && i + 1 < text.size()
            && text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff) {
            const uint32_t second = uint32_t(text[++i]);
            value = 0x10000 + ((value - 0xd800) << 10) + second - 0xdc00;
        } else if (value >= 0xd800 && value <= 0xdfff) value = '?';
        codepoint(digest, value);
    }
    return digest.finish();
}
uint64_t positional_seed(int32_t x, int32_t y, int32_t z) {
    uint64_t value = sign_extend32(uint32_t(x) * 3129871u) ^ (uint64_t(int64_t(z)) * 116129781ULL) ^ uint64_t(int64_t(y));
    value = value * value * 42317861ULL + value * 11;
    return (value >> 16) | ((value >> 63) ? 0xffff000000000000ULL : 0);
}
}
