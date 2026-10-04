#pragma once
#include "mcps2/java_bits.hpp"
#include <cassert>
#include <type_traits>

namespace mcps2 {
// Java integer overflow is expressed using unsigned arithmetic, not C++ signed UB.
uint64_t mix_stafford13(uint64_t value);

template<class Source> class BitRandom {
public:
    int32_t next_int() { return signed32(self().next_bits(32)); }
    bool next_int(int32_t bound, int32_t& value) {
        if (bound <= 0) return false; // Equivalent rejection, without C++ exceptions on EE.
        const auto limit = uint32_t(bound);
        uint32_t draw = self().next_bits(31);
        if ((limit & (limit - 1)) == 0) {
            value = static_cast<int32_t>((uint64_t(draw) * limit) >> 31);
            return true;
        }
        uint32_t candidate = draw % limit;
        while ((draw - candidate + limit - 1) & 0x80000000u) {
            draw = self().next_bits(31);
            candidate = draw % limit;
        }
        value = static_cast<int32_t>(candidate);
        return true;
    }
    uint64_t next_long() {
        const uint64_t high = uint64_t(self().next_bits(32)) << 32;
        const uint32_t low = self().next_bits(32);
        // The low Java int is sign-extended before addition, rather than concatenated.
        return high + sign_extend32(low);
    }
    bool next_boolean() { return self().next_bits(1) != 0; }
    float next_float() { return float(self().next_bits(24)) * 0x1p-24f; }
    double next_double() {
        const uint64_t high = uint64_t(self().next_bits(26)) << 27;
        const uint64_t low = self().next_bits(27);
        return double(high + low) * 0x1p-53;
    }
private:
    Source& self() { return *static_cast<Source*>(this); }
};

class LegacyRandom : public BitRandom<LegacyRandom> {
public:
    explicit LegacyRandom(uint64_t seed = 0) { set_seed(seed); }
    void set_seed(uint64_t seed) { state_ = (seed ^ 0x5deece66dULL) & 0xffffffffffffULL; }
    uint32_t next_bits(unsigned bits) {
        assert(bits >= 1 && bits <= 32);
        state_ = (state_ * 0x5deece66dULL + 11) & 0xffffffffffffULL;
        return uint32_t(state_ >> (48 - bits));
    }
    LegacyRandom fork() { return LegacyRandom(next_long()); }
    uint64_t state() const { return state_; }
private:
    uint64_t state_ = 0;
};

class XoroshiroRandom {
public:
    explicit XoroshiroRandom(uint64_t seed = 0) { set_seed(seed); }
    XoroshiroRandom(uint64_t low, uint64_t high) { set_state(low, high); }
    void set_seed(uint64_t seed);
    void set_state(uint64_t low, uint64_t high);
    uint64_t next_long();
    int32_t next_int() { return signed32(uint32_t(next_long())); }
    bool next_int(int32_t bound, int32_t& value);
    bool next_boolean() { return (next_long() & 1) != 0; }
    float next_float() { return float(next_long() >> 40) * 0x1p-24f; }
    double next_double() { return double(next_long() >> 11) * 0x1p-53; }
    XoroshiroRandom fork() {
        const uint64_t low = next_long();
        const uint64_t high = next_long();
        return XoroshiroRandom(low, high);
    }
    uint64_t low() const { return low_; }
    uint64_t high() const { return high_; }
private:
    uint64_t low_ = 0, high_ = 0;
};

template<class Source> class WorldgenRandom : public BitRandom<WorldgenRandom<Source>> {
public:
    explicit WorldgenRandom(uint64_t seed = 0) : source_(seed) {}
    void set_seed(uint64_t seed) { source_.set_seed(seed); }
    uint32_t next_bits(unsigned bits) {
        assert(bits >= 1 && bits <= 32);
        ++count_;
        if constexpr (std::is_same_v<Source, LegacyRandom>) return source_.next_bits(bits);
        else return uint32_t(source_.next_long() >> (64 - bits));
    }
    uint32_t count() const { return count_; }
    uint64_t decoration_seed(uint64_t seed, int32_t x, int32_t z) {
        set_seed(seed);
        const uint64_t a = this->next_long() | 1;
        const uint64_t b = this->next_long() | 1;
        const uint64_t derived = (uint64_t(int64_t(x)) * a + uint64_t(int64_t(z)) * b) ^ seed;
        set_seed(derived);
        return derived;
    }
    void feature_seed(uint64_t seed, int32_t index, int32_t step) {
        const uint32_t product = uint32_t(step) * 10000u;
        set_seed(seed + uint64_t(int64_t(index)) + sign_extend32(product));
    }
    void large_feature_seed(uint64_t seed, int32_t x, int32_t z) {
        set_seed(seed);
        const uint64_t a = this->next_long();
        const uint64_t b = this->next_long();
        set_seed((uint64_t(int64_t(x)) * a) ^ (uint64_t(int64_t(z)) * b) ^ seed);
    }
    void large_feature_with_salt(uint64_t seed, int32_t x, int32_t z, int32_t salt) {
        set_seed(uint64_t(int64_t(x)) * 341873128712ULL + uint64_t(int64_t(z)) * 132897987541ULL
                 + seed + uint64_t(int64_t(salt)));
    }
private:
    Source source_;
    uint32_t count_ = 0;
};

LegacyRandom slime_chunk_random(int32_t x, int32_t z, uint64_t world_seed, uint64_t salt);
}
