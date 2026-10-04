#pragma once
#include <cstddef>
#include <cstdint>

namespace mcps2 {
// External memory ownership allows chunk arenas/pools. Values never cross a 64-bit word.
class BitStorage {
public:
    static size_t required_words(unsigned bits, uint32_t entries) {
        if (bits == 0) return 0;
        if (bits > 32) return SIZE_MAX;
        const unsigned per = 64 / bits;
        return (size_t(entries) + per - 1) / per;
    }
    bool bind(unsigned bits, uint32_t entries, uint64_t* words, size_t capacity, bool clear = true);
    bool get(uint32_t index, uint32_t& value) const;
    bool set(uint32_t index, uint32_t value);
    bool get_and_set(uint32_t index, uint32_t value, uint32_t& previous);
    unsigned bits() const { return bits_; }
    uint32_t size() const { return entries_; }
    size_t word_count() const { return required_words(bits_, entries_); }
private:
    uint64_t* words_ = nullptr;
    uint64_t mask_ = 0;
    uint32_t entries_ = 0;
    unsigned bits_ = 0, per_word_ = 0;
};
}
