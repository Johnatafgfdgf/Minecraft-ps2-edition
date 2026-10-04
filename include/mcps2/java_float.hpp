#pragma once
#include <cstdint>

namespace mcps2::java_float {
// Keep storage/ABI as binary32, but never use EE single-precision arithmetic.
// Each operation rounds explicitly to nearest, ties to even, using integer bits.
float round(double value) noexcept;
float add(float a,float b) noexcept;
float subtract(float a,float b) noexcept;
float multiply(float a,float b) noexcept;
float divide(float a,float b) noexcept;
float remainder(float a,float b) noexcept;
float square_root(float value) noexcept;
float negate(float value) noexcept;
float absolute(float value) noexcept;
bool is_nan(float value) noexcept;
bool negative(float value) noexcept;
bool equal(float a,float b) noexcept;
bool less(float a,float b) noexcept;
bool less_equal(float a,float b) noexcept;
}
