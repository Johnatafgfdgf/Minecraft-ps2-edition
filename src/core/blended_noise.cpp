#include "mcps2/blended_noise.hpp"

namespace mcps2 {
double BlendedNoise::sample(int32_t x, int32_t y, int32_t z) const {
    assert(ready_);
    const double gx = double(x) * xz_multiplier_, gy = double(y) * y_multiplier_, gz = double(z) * xz_multiplier_;
    const double sx = gx / parameters_.xz_factor, sy = gy / parameters_.y_factor, sz = gz / parameters_.xz_factor;
    const double smear = y_multiplier_ * parameters_.smear_scale;
    const double selector_smear = smear / parameters_.y_factor;
    double selected = 0, scale = 1;
    for (unsigned i = 0; i < 8; ++i) {
        const auto* field = selector_.octave(i);
        if (field) selected += field->sample(PerlinNoise::wrap(sx * scale), PerlinNoise::wrap(sy * scale),
                                            PerlinNoise::wrap(sz * scale), selector_smear * scale, sy * scale) / scale;
        scale /= 2.0;
    }
    const double amount = (selected / 10.0 + 1.0) / 2.0;
    double lower = 0, upper = 0;
    scale = 1;
    for (unsigned i = 0; i < 16; ++i) {
        const double wx = PerlinNoise::wrap(gx * scale), wy = PerlinNoise::wrap(gy * scale), wz = PerlinNoise::wrap(gz * scale);
        const double vertical_step = smear * scale;
        if (!(amount >= 1.0)) {
            const auto* field = lower_.octave(i);
            if (field) lower += field->sample(wx, wy, wz, vertical_step, gy * scale) / scale;
        }
        if (!(amount <= 0.0)) {
            const auto* field = upper_.octave(i);
            if (field) upper += field->sample(wx, wy, wz, vertical_step, gy * scale) / scale;
        }
        scale /= 2.0;
    }
    const double first = lower / 512.0, second = upper / 512.0;
    const double blended = amount < 0.0 ? first : amount > 1.0 ? second : first + amount * (second - first);
    return blended / 128.0;
}
}
