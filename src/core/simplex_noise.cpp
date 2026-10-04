#include "mcps2/simplex_noise.hpp"
#include "mcps2/random.hpp"
#include "mcps2/java_math.hpp"
#include "mcps2/java_float.hpp"
#include <cmath>

namespace mcps2 {
namespace {
int32_t floor_java(double v) noexcept {
    int32_t n;
    if (v != v) n = 0;
    else if (v >= 2147483647.0) n = 2147483647;
    else if (v <= -2147483648.0) n = signed32(0x80000000u);
    else n = int32_t(v);
    return v < double(n) ? signed32(uint32_t(n) - 1) : n;
}
double corner(unsigned gradient,double x,double y) noexcept {
    static constexpr int8_t basis[12][3] = {
        {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
        {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
        {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1}
    };
    double amount = ((0.5 - x * x) - y * y) - 0.0 * 0.0;
    if (amount < 0) return 0;
    amount *= amount;
    const auto& g = basis[gradient];
    return (amount * amount) * ((double(g[0]) * x + double(g[1]) * y) + double(g[2]) * 0.0);
}
}
double SimplexNoise::sample(double x,double y) const noexcept {
    assert(ready_);
    const double skew = 0.5 * (std::sqrt(3.0) - 1.0), unskew = (3.0 - std::sqrt(3.0)) / 6.0;
    const double diagonal = (x + y) * skew;
    const int32_t ix = floor_java(x + diagonal), iy = floor_java(y + diagonal);
    const double shift = double(signed32(uint32_t(ix) + uint32_t(iy))) * unskew;
    const double x0 = x - (double(ix) - shift), y0 = y - (double(iy) - shift);
    const unsigned dx = x0 > y0 ? 1 : 0, dy = x0 > y0 ? 0 : 1;
    const double x1 = x0 - double(dx) + unskew, y1 = y0 - double(dy) + unskew;
    const double x2 = x0 - 1 + 2 * unskew, y2 = y0 - 1 + 2 * unskew;
    const unsigned px = uint32_t(ix) & 255, py = uint32_t(iy) & 255;
    const double a = corner(perm(px + perm(py)) % 12,x0,y0);
    const double b = corner(perm(px + dx + perm(py + dy)) % 12,x1,y1);
    const double c = corner(perm(px + 1 + perm(py + 1)) % 12,x2,y2);
    return 70 * ((a + b) + c);
}
void EndIslandDensity::initialize(uint64_t seed) noexcept {
    LegacyRandom source(seed);
    for (unsigned i = 0; i < 17292; ++i) (void)source.next_int();
    noise_.initialize(source);
}
float EndIslandDensity::height(int32_t x,int32_t z) const noexcept {
    using namespace java_float;
    assert(ready());
    const int32_t half_x = x / 2, half_z = z / 2, rem_x = x % 2, rem_z = z % 2;
    const int32_t distance = signed32(uint32_t(x) * uint32_t(x) + uint32_t(z) * uint32_t(z));
    float result = java_clamp(subtract(100.0f,multiply(square_root(round(double(distance))),8.0f)),-100.0f,80.0f);
    for (int32_t dx = -12; dx <= 12; ++dx) for (int32_t dz = -12; dz <= 12; ++dz) {
        const int64_t cx = int64_t(half_x + dx), cz = int64_t(half_z + dz);
        if (cx * cx + cz * cz <= 4096 || !(noise_.sample(double(cx),double(cz)) < double(-0.9f))) continue;
        const float scale = add(remainder(add(multiply(absolute(round(double(cx))),3439.0f),multiply(absolute(round(double(cz))),147.0f)),13.0f),9.0f);
        const float rx = round(double(rem_x - dx * 2)), rz = round(double(rem_z - dz * 2));
        const float value = java_clamp(subtract(100.0f,multiply(square_root(add(multiply(rx,rx),multiply(rz,rz))),scale)),-100.0f,80.0f);
        result = java_maximum(result,value);
    }
    return result;
}
}
