#pragma once
#include <cstddef>
#include <cstdint>
#include <cassert>

namespace mcps2 {
class SimplexNoise {
public:
    template<class R> void initialize(R& source) noexcept {
        for (double& offset : offsets_) offset = source.next_double() * 256;
        for (unsigned i = 0; i < 256; ++i) permutation_[i] = uint8_t(i);
        for (unsigned i = 0; i < 256; ++i) {
            int32_t delta = 0; const bool accepted = source.next_int(int32_t(256 - i),delta); assert(accepted); (void)accepted;
            const unsigned other = i + unsigned(delta);
            const uint8_t before = permutation_[i]; permutation_[i] = permutation_[other]; permutation_[other] = before;
        }
        ready_ = true;
    }
    double sample(double x,double y) const noexcept;
    double offset(unsigned axis) const noexcept { assert(axis < 3 && ready_); return offsets_[axis]; }
    bool ready() const noexcept { return ready_; }
private:
    unsigned perm(unsigned i) const noexcept { return permutation_[i & 255]; }
    uint8_t permutation_[256]{};
    double offsets_[3]{};
    bool ready_ = false;
};
class EndIslandDensity {
public:
    void initialize(uint64_t seed) noexcept;
    float height(int32_t x,int32_t z) const noexcept;
    double sample(int32_t x,int32_t z) const noexcept { return (double(height(x / 8,z / 8)) - 8) / 128; }
    bool ready() const noexcept { return noise_.ready(); }
    static constexpr double minimum = -0.84375, maximum = 0.5625;
private:
    SimplexNoise noise_;
};
}
