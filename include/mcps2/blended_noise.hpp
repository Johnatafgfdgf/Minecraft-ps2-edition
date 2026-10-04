#pragma once
#include "mcps2/octave_noise.hpp"

namespace mcps2 {
struct BlendedNoiseParameters {
    double xz_scale, y_scale, xz_factor, y_factor, smear_scale;
};
class BlendedNoise {
public:
    static constexpr size_t required_octaves = 40;
    template<class R> bool initialize(R& source, const BlendedNoiseParameters& parameters,
                                     NoiseOctave* storage, size_t capacity) {
        if (!storage || capacity < required_octaves) return false;
        ready_ = false;
        double amplitudes[16];
        for (double& amplitude : amplitudes) amplitude = 1.0;
        if (!lower_.initialize(source, -15, amplitudes, 16, storage, 16, false)
            || !upper_.initialize(source, -15, amplitudes, 16, storage + 16, 16, false)
            || !selector_.initialize(source, -7, amplitudes, 8, storage + 32, 8, false)) return false;
        parameters_ = parameters;
        xz_multiplier_ = 684.412 * parameters.xz_scale;
        y_multiplier_ = 684.412 * parameters.y_scale;
        max_value_ = lower_.max_broken_value(y_multiplier_);
        ready_ = true; return true;
    }
    template<class R> bool reseed(R& source, NoiseOctave* storage, size_t capacity) {
        if (!ready_) return false;
        return initialize(source, parameters_, storage, capacity);
    }
    double sample(int32_t x, int32_t y, int32_t z) const;
    double max_value() const { assert(ready_); return max_value_; }
    double min_value() const { return -max_value(); }
    bool ready() const { return ready_; }
private:
    PerlinNoise lower_, upper_, selector_;
    BlendedNoiseParameters parameters_{};
    double xz_multiplier_ = 0, y_multiplier_ = 0, max_value_ = 0;
    bool ready_ = false;
};
}
