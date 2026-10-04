#pragma once
#include <cstdint>
#include <cstring>

namespace mcps2 {
inline int32_t signed32(uint32_t bits) { int32_t v; std::memcpy(&v, &bits, sizeof(v)); return v; }
inline int64_t signed64(uint64_t bits) { int64_t v; std::memcpy(&v, &bits, sizeof(v)); return v; }
inline uint64_t sign_extend32(uint32_t bits) {
    return (bits & 0x80000000u) ? (0xffffffff00000000ULL | bits) : uint64_t(bits);
}
inline int32_t unpack_signed(uint64_t value, unsigned width) {
    const uint64_t mask = (uint64_t(1) << width) - 1;
    const int64_t positive = static_cast<int64_t>(value & mask);
    return static_cast<int32_t>(positive - ((value & (uint64_t(1) << (width - 1))) ? (int64_t(1) << width) : 0));
}
inline uint64_t rotate_left(uint64_t value, unsigned shift) {
    return (value << shift) | (value >> (64 - shift));
}
static_assert(sizeof(float) == 4 && sizeof(double) == 8, "Java random output requires binary32/binary64");
}
