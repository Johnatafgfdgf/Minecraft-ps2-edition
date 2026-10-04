#pragma once
#include "mcps2/java_bits.hpp"

namespace mcps2 {
struct BlockPos {
    int32_t x = 0, y = 0, z = 0;
    uint64_t pack() const {
        return (uint64_t(uint32_t(x) & 0x3ffffffu) << 38)
             | (uint64_t(uint32_t(z) & 0x3ffffffu) << 12) | (uint32_t(y) & 0xfffu);
    }
    static BlockPos unpack(uint64_t value) {
        return {unpack_signed(value >> 38, 26), unpack_signed(value, 12), unpack_signed(value >> 12, 26)};
    }
};
struct SectionPos {
    int32_t x = 0, y = 0, z = 0;
    uint64_t pack() const {
        return (uint64_t(uint32_t(x) & 0x3fffffu) << 42)
             | (uint64_t(uint32_t(z) & 0x3fffffu) << 20) | (uint32_t(y) & 0xfffffu);
    }
    static SectionPos unpack(uint64_t value) {
        return {unpack_signed(value >> 42, 22), unpack_signed(value, 20), unpack_signed(value >> 20, 22)};
    }
};
inline int32_t block_to_section(int32_t coordinate) {
    const int32_t truncated = coordinate / 16;
    return truncated - (coordinate % 16 < 0 ? 1 : 0);
}
inline uint16_t section_relative(BlockPos pos) {
    return uint16_t(((uint32_t(pos.x) & 15) << 8) | ((uint32_t(pos.z) & 15) << 4) | (uint32_t(pos.y) & 15));
}
inline uint16_t section_index(unsigned x, unsigned y, unsigned z) {
    return uint16_t((y << 8) | (z << 4) | x);
}
inline uint64_t chunk_pos(int32_t x, int32_t z) {
    return uint64_t(uint32_t(x)) | (uint64_t(uint32_t(z)) << 32);
}
}
