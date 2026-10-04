#pragma once
#include "mcps2/java_bits.hpp"
#include "mcps2/seed_hash.hpp"
#include <cassert>
#include <type_traits>

namespace mcps2 {
// Java integer overflow is expressed using unsigned arithmetic, not C++ signed UB.
uint64_t mix_stafford13(uint64_t value);
class LegacyPositionalFactory;
class XoroshiroPositionalFactory;

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
    LegacyPositionalFactory fork_positional();
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
    XoroshiroPositionalFactory fork_positional();
private:
    uint64_t low_ = 0, high_ = 0;
};

class LegacyPositionalFactory {
public:
    explicit LegacyPositionalFactory(uint64_t seed) : seed_(seed) {}
    LegacyRandom at(int32_t x, int32_t y, int32_t z) const { return LegacyRandom(positional_seed(x, y, z) ^ seed_); }
    LegacyRandom from_seed(uint64_t value) const { return LegacyRandom(value); }
    LegacyRandom from_hash(std::u16string_view text) const { return LegacyRandom(sign_extend32(java_string_hash(text)) ^ seed_); }
    LegacyRandom from_hash_ascii(std::string_view text) const {
        uint32_t value = 0;
        for (unsigned char unit : text) { assert(unit < 128); value = value * 31 + unit; }
        return LegacyRandom(sign_extend32(value) ^ seed_);
    }
private:
    uint64_t seed_;
};
class XoroshiroPositionalFactory {
public:
    XoroshiroPositionalFactory(uint64_t low, uint64_t high) : low_(low), high_(high) {}
    XoroshiroRandom at(int32_t x, int32_t y, int32_t z) const { return XoroshiroRandom(positional_seed(x, y, z) ^ low_, high_); }
    XoroshiroRandom from_seed(uint64_t value) const { return XoroshiroRandom(value ^ low_, value ^ high_); }
    XoroshiroRandom from_hash(std::u16string_view text) const {
        const Seed128 seed = seed_from_java_string(text);
        return XoroshiroRandom(seed.low ^ low_, seed.high ^ high_);
    }
    XoroshiroRandom from_hash_ascii(std::string_view text) const {
        for (unsigned char unit : text) { assert(unit < 128); (void)unit; }
        const Seed128 seed = seed_from_utf8(text);
        return XoroshiroRandom(seed.low ^ low_, seed.high ^ high_);
    }
private:
    uint64_t low_, high_;
};
inline LegacyPositionalFactory LegacyRandom::fork_positional() { return LegacyPositionalFactory(next_long()); }
inline XoroshiroPositionalFactory XoroshiroRandom::fork_positional() {
    const uint64_t low = next_long(), high = next_long();
    return XoroshiroPositionalFactory(low, high);
}

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
    auto fork_positional() { return source_.fork_positional(); }
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
