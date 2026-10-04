#pragma once
#include <cstdint>
#include <cassert>

namespace mcps2 {
// A value-sampling ImprovedNoise kernel, constructed from the equivalent random stream.
// Derivative sampling and higher-level octave/density composition remain separate tasks.
class ImprovedNoise {
public:
    template<class Random> explicit ImprovedNoise(Random& random) {
        for (double& offset : offsets_) offset = random.next_double() * 256.0;
        for (unsigned i = 0; i < 256; ++i) permutation_[i] = uint8_t(i);
        for (unsigned i = 0; i < 256; ++i) {
            int32_t relative = 0;
            const bool accepted = random.next_int(int32_t(256 - i), relative);
            assert(accepted);
            (void)accepted;
            const unsigned selected = i + unsigned(relative);
            const uint8_t old = permutation_[i];
            permutation_[i] = permutation_[selected]; permutation_[selected] = old;
        }
    }
    double sample(double x, double y, double z, double y_scale = 0.0, double y_max = 0.0) const;
    double offset(unsigned axis) const { assert(axis < 3); return offsets_[axis]; }
private:
    double offsets_[3]{};
    uint8_t permutation_[256]{};
    uint8_t lookup(uint32_t index) const { return permutation_[index & 255]; }
};
}
