#include "mcps2/octave_noise.hpp"

namespace mcps2 {
namespace {
int64_t java_long_floor(double value) {
    int64_t integral;
    if (value != value) integral = 0;
    else if (value >= 0x1p63) integral = signed64(0x7fffffffffffffffULL);
    else if (value <= -0x1p63) integral = signed64(0x8000000000000000ULL);
    else integral = static_cast<int64_t>(value);
    return value < double(integral) ? signed64(uint64_t(integral) - 1) : integral;
}
}
double PerlinNoise::wrap(double value) {
    return value - double(java_long_floor(value / 33554432.0 + 0.5)) * 33554432.0;
}
double PerlinNoise::edge_value(double edge) const {
    double result = 0, weight = value_factor_;
    for (size_t i = 0; i < count_; ++i) {
        if (octaves_[i].field) result += octaves_[i].amplitude * edge * weight;
        weight /= 2.0;
    }
    return result;
}
double PerlinNoise::sample(double x, double y, double z, double y_scale, double y_max, bool fixed_y) const {
    assert(ready_);
    double result = 0, frequency = input_factor_, weight = value_factor_;
    for (size_t i = 0; i < count_; ++i) {
        const auto& octave = octaves_[i];
        if (octave.field) {
            const double selected_y = fixed_y ? -octave.field->offset(1) : wrap(y * frequency);
            const double value = octave.field->sample(wrap(x * frequency), selected_y, wrap(z * frequency), y_scale * frequency, y_max * frequency);
            result += octave.amplitude * value * weight;
        }
        frequency *= 2.0; weight /= 2.0;
    }
    return result;
}
double NormalNoise::sample(double x, double y, double z) const {
    assert(ready_);
    const double sx = x * 1.0181268882175227, sy = y * 1.0181268882175227, sz = z * 1.0181268882175227;
    return (first_.sample(x, y, z) + second_.sample(sx, sy, sz)) * factor_;
}
}
