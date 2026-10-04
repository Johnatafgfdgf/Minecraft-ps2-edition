#include "mcps2/block_states.hpp"
#include <cstring>

namespace mcps2 {
uint32_t BlockStates::load32(size_t offset) const {
    const auto* p = memory_ + offset;
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint16_t BlockStates::load16(size_t offset) const {
    return uint16_t(uint16_t(memory_[offset]) | (uint16_t(memory_[offset + 1]) << 8));
}
const char* BlockStates::string(uint32_t offset) const {
    if (offset >= string_bytes_) return nullptr;
    const char* start = reinterpret_cast<const char*>(memory_ + string_offset_ + offset);
    return std::memchr(start, 0, string_bytes_ - offset) ? start : nullptr;
}
bool BlockStates::bind(const void* memory, size_t bytes) {
    *this = BlockStates{};
    if (!memory || bytes < 32 || std::memcmp(memory, "MCSR", 4) != 0) return false;
    BlockStates view;
    view.memory_ = static_cast<const uint8_t*>(memory); view.bytes_ = bytes;
    if (view.load32(4) != 1 || view.load32(28) != 3955) return false;
    view.blocks_ = view.load32(8); view.states_ = view.load32(12);
    view.pairs_ = view.load32(16); view.refs_ = view.load32(20); view.string_bytes_ = view.load32(24);
    if (!view.blocks_ || !view.states_ || !view.string_bytes_ || view.pairs_ > 65536) return false;
    const uint64_t block_offset = 32, state_offset = block_offset + uint64_t(view.blocks_) * 16;
    const uint64_t pair_offset = state_offset + uint64_t(view.states_) * 12;
    const uint64_t ref_offset = pair_offset + uint64_t(view.pairs_) * 8;
    const uint64_t string_offset = ref_offset + uint64_t(view.refs_) * 2;
    if (string_offset + view.string_bytes_ != bytes) return false;
    view.block_offset_ = size_t(block_offset); view.state_offset_ = size_t(state_offset);
    view.pair_offset_ = size_t(pair_offset); view.ref_offset_ = size_t(ref_offset); view.string_offset_ = size_t(string_offset);
    for (uint32_t i = 0; i < view.pairs_; ++i) {
        if (!view.string(view.load32(view.pair_offset_ + size_t(i) * 8))
            || !view.string(view.load32(view.pair_offset_ + size_t(i) * 8 + 4))) return false;
    }
    for (uint32_t i = 0; i < view.refs_; ++i)
        if (view.load16(view.ref_offset_ + size_t(i) * 2) >= view.pairs_) return false;
    for (uint32_t i = 0; i < view.blocks_; ++i) {
        const size_t offset = view.block_offset_ + size_t(i) * 16;
        const uint32_t begin = view.load32(offset + 8), count = view.load32(offset + 12), def = view.load32(offset + 4);
        if (!view.block_name(i) || !count || uint64_t(begin) + count > view.states_ || def < begin || uint64_t(def) >= uint64_t(begin) + count) return false;
        for (uint32_t s = begin; s < begin + count; ++s) if (view.state_block(s) != i) return false;
    }
    for (uint32_t i = 0; i < view.states_; ++i) {
        const size_t offset = view.state_offset_ + size_t(i) * 12;
        const uint32_t block = view.load32(offset), start = view.load32(offset + 4), count = view.load32(offset + 8);
        if (block >= view.blocks_ || uint64_t(start) + count > view.refs_) return false;
        const size_t b = view.block_offset_ + size_t(block) * 16;
        if (i < view.load32(b + 8) || uint64_t(i) >= uint64_t(view.load32(b + 8)) + view.load32(b + 12)) return false;
        for (uint32_t p = 0; p < count; ++p) {
            const char *name, *value;
            if (!view.property_at(i, p, name, value)) return false;
            for (uint32_t q = 0; q < p; ++q) {
                const char *other_name, *other_value;
                if (!view.property_at(i, q, other_name, other_value) || std::strcmp(name, other_name) == 0) return false;
            }
        }
    }
    *this = view;
    return true;
}
const char* BlockStates::block_name(uint32_t block) const {
    return block < blocks_ ? string(load32(block_offset_ + size_t(block) * 16)) : nullptr;
}
uint32_t BlockStates::find_block(const char* name) const {
    if (!name) return invalid_id;
    for (uint32_t i = 0; i < blocks_; ++i) if (std::strcmp(block_name(i), name) == 0) return i;
    return invalid_id;
}
uint32_t BlockStates::default_state(uint32_t block) const {
    return block < blocks_ ? load32(block_offset_ + size_t(block) * 16 + 4) : invalid_id;
}
uint32_t BlockStates::state_block(uint32_t state) const {
    return state < states_ ? load32(state_offset_ + size_t(state) * 12) : invalid_id;
}
uint32_t BlockStates::property_count(uint32_t state) const {
    return state < states_ ? load32(state_offset_ + size_t(state) * 12 + 8) : 0;
}
uint32_t BlockStates::pair_id(uint32_t state, uint32_t index) const {
    const uint32_t start = load32(state_offset_ + size_t(state) * 12 + 4);
    return load16(ref_offset_ + size_t(start + index) * 2);
}
bool BlockStates::property_at(uint32_t state, uint32_t index, const char*& name, const char*& value) const {
    if (state >= states_ || index >= property_count(state)) return false;
    const uint32_t id = pair_id(state, index);
    if (id >= pairs_) return false;
    name = string(load32(pair_offset_ + size_t(id) * 8));
    value = string(load32(pair_offset_ + size_t(id) * 8 + 4));
    return name && value;
}
const char* BlockStates::property(uint32_t state, const char* name) const {
    if (!name) return nullptr;
    for (uint32_t i = 0; i < property_count(state); ++i) {
        const char *key, *value;
        if (property_at(state, i, key, value) && std::strcmp(key, name) == 0) return value;
    }
    return nullptr;
}
bool BlockStates::with_property(uint32_t state, const char* name, const char* value, uint32_t& result) const {
    if (!valid() || !name || !value || !property(state, name)) return false;
    const uint32_t block = state_block(state), count = property_count(state);
    const size_t offset = block_offset_ + size_t(block) * 16;
    const uint32_t begin = load32(offset + 8), end = begin + load32(offset + 12);
    for (uint32_t candidate = begin; candidate < end; ++candidate) {
        if (property_count(candidate) != count) continue;
        bool match = true;
        for (uint32_t i = 0; i < count; ++i) {
            const char *key, *old_value;
            if (!property_at(state, i, key, old_value)) return false;
            const char* desired = std::strcmp(key, name) == 0 ? value : old_value;
            const char* actual = property(candidate, key);
            if (!actual || std::strcmp(desired, actual) != 0) { match = false; break; }
        }
        if (match) { result = candidate; return true; }
    }
    return false;
}
}
