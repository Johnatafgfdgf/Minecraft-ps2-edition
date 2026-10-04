#include "mcps2/improved_noise.hpp"
#include "mcps2/java_bits.hpp"

namespace mcps2 {
namespace {
// Mathematical gradient basis for 3D improved Perlin noise (including repeated corners).
constexpr int8_t basis[16][3] = {
    {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
    {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
    {0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1},
    {1,1,0}, {0,-1,1}, {-1,1,0}, {0,-1,-1}
};
int32_t java_floor(double value) {
    int32_t truncated;
    if (value != value) truncated = 0;
    else if (value >= 2147483647.0) truncated = 2147483647;
    else if (value <= -2147483648.0) truncated = signed32(0x80000000u);
    else truncated = static_cast<int32_t>(value);
    return value < double(truncated) ? signed32(uint32_t(truncated) - 1) : truncated;
}
double smooth(double value) { return value * value * value * (value * (value * 6.0 - 15.0) + 10.0); }
double interpolate(double amount, double first, double second) { return first + amount * (second - first); }
double gradient(uint8_t index, double x, double y, double z) {
    const auto& axis = basis[index & 15];
    return double(axis[0]) * x + double(axis[1]) * y + double(axis[2]) * z;
}
}
double ImprovedNoise::sample(double x, double y, double z, double y_scale, double y_max) const {
    const double input[3] = {x + offsets_[0], y + offsets_[1], z + offsets_[2]};
    const int32_t cell[3] = {java_floor(input[0]), java_floor(input[1]), java_floor(input[2])};
    double fraction[3];
    for (unsigned axis = 0; axis < 3; ++axis) fraction[axis] = input[axis] - double(cell[axis]);
    double stepped_y = fraction[1];
    if (y_scale != 0.0) {
        const double limited = y_max >= 0.0 && y_max < fraction[1] ? y_max : fraction[1];
        // The original constant is widened from binary32, not a binary64 1e-7 literal.
        const double step = double(java_floor(limited / y_scale + double(1.0e-7f))) * y_scale;
        stepped_y -= step;
    }
    double corners[8];
    for (unsigned dz = 0; dz < 2; ++dz) {
        for (unsigned dy = 0; dy < 2; ++dy) {
            for (unsigned dx = 0; dx < 2; ++dx) {
                const uint32_t ix = uint32_t(cell[0]) + dx, iy = uint32_t(cell[1]) + dy, iz = uint32_t(cell[2]) + dz;
                const uint8_t hash = lookup(uint32_t(lookup(uint32_t(lookup(ix)) + iy)) + iz);
                corners[(dz << 2) | (dy << 1) | dx] = gradient(hash, fraction[0] - double(dx), stepped_y - double(dy), fraction[2] - double(dz));
            }
        }
    }
    const double fx = smooth(fraction[0]), fy = smooth(fraction[1]), fz = smooth(fraction[2]);
    const double near_y0 = interpolate(fx, corners[0], corners[1]);
    const double near_y1 = interpolate(fx, corners[2], corners[3]);
    const double far_y0 = interpolate(fx, corners[4], corners[5]);
    const double far_y1 = interpolate(fx, corners[6], corners[7]);
    return interpolate(fz, interpolate(fy, near_y0, near_y1), interpolate(fy, far_y0, far_y1));
}
}
