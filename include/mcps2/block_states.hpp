#pragma once
#include <cstddef>
#include <cstdint>

namespace mcps2 {
// Immutable view of a privately generated MCSR registry. No ownership/allocation.
class BlockStates {
public:
    static constexpr uint32_t invalid_id = 0xffffffffu;
    bool bind(const void* memory, size_t bytes);
    bool valid() const { return memory_ != nullptr; }
    uint32_t block_count() const { return blocks_; }
    uint32_t state_count() const { return states_; }
    const char* block_name(uint32_t block) const;
    uint32_t find_block(const char* name) const;
    uint32_t default_state(uint32_t block) const;
    uint32_t state_block(uint32_t state) const;
    uint32_t property_count(uint32_t state) const;
    bool property_at(uint32_t state, uint32_t index, const char*& name, const char*& value) const;
    const char* property(uint32_t state, const char* name) const;
    bool with_property(uint32_t state, const char* name, const char* value, uint32_t& result) const;
private:
    uint32_t load32(size_t offset) const;
    uint16_t load16(size_t offset) const;
    const char* string(uint32_t offset) const;
    uint32_t pair_id(uint32_t state, uint32_t index) const;
    const uint8_t* memory_ = nullptr;
    size_t bytes_ = 0, block_offset_ = 0, state_offset_ = 0, pair_offset_ = 0, ref_offset_ = 0, string_offset_ = 0;
    uint32_t blocks_ = 0, states_ = 0, pairs_ = 0, refs_ = 0, string_bytes_ = 0;
};
}
