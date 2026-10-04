#include "mcps2/bit_storage.hpp"
#include <cstring>

namespace mcps2 {
bool BitStorage::bind(unsigned bits, uint32_t entries, uint64_t* words, size_t capacity, bool clear) {
    const size_t count = required_words(bits, entries);
    if (bits > 32 || count > capacity || (count && !words)) return false;
    words_ = words; entries_ = entries; bits_ = bits;
    per_word_ = bits ? 64 / bits : 0;
    mask_ = bits ? ((uint64_t(1) << bits) - 1) : 0;
    if (clear && count) std::memset(words, 0, count * sizeof(uint64_t));
    return true;
}
bool BitStorage::get(uint32_t index, uint32_t& value) const {
    if (index >= entries_) return false;
    if (!bits_) { value = 0; return true; }
    const uint32_t word = index / per_word_;
    const unsigned shift = (index % per_word_) * bits_;
    value = uint32_t((words_[word] >> shift) & mask_);
    return true;
}
bool BitStorage::set(uint32_t index, uint32_t value) {
    if (index >= entries_ || uint64_t(value) > mask_) return false;
    if (!bits_) return true;
    const uint32_t word = index / per_word_;
    const unsigned shift = (index % per_word_) * bits_;
    words_[word] = (words_[word] & ~(mask_ << shift)) | (uint64_t(value) << shift);
    return true;
}
bool BitStorage::get_and_set(uint32_t index, uint32_t value, uint32_t& previous) {
    if (index >= entries_ || uint64_t(value) > mask_) return false;
    return get(index, previous) && set(index, value);
}
}
