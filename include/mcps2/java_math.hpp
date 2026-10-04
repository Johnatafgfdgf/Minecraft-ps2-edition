#pragma once
#include <cmath>
#include <type_traits>
#include "mcps2/java_float.hpp"

namespace mcps2 {
template<class T> T java_minimum(T a,T b) noexcept {
    if constexpr (std::is_same_v<T,float>) {
        if (java_float::is_nan(a)) return a;
        if (java_float::equal(a,0) && java_float::equal(b,0) && java_float::negative(b)) return b;
        return java_float::less_equal(a,b)?a:b;
    } else {
    const double x=double(a),y=double(b);
    if (x != x) return a;
    if (x == 0 && y == 0 && std::signbit(y)) return b;
    return x <= y ? a : b;
    }
}
template<class T> T java_maximum(T a,T b) noexcept {
    if constexpr (std::is_same_v<T,float>) {
        if (java_float::is_nan(a)) return a;
        if (java_float::equal(a,0) && java_float::equal(b,0) && java_float::negative(a)) return b;
        return java_float::less_equal(b,a)?a:b;
    } else {
    const double x=double(a),y=double(b);
    if (x != x) return a;
    if (x == 0 && y == 0 && std::signbit(x)) return b;
    return x >= y ? a : b;
    }
}
template<class T> T java_clamp(T v,T low,T high) noexcept {
    if constexpr (std::is_same_v<T,float>) return java_float::less(v,low)?low:java_minimum(v,high);
    else return v < low ? low : java_minimum(v,high);
}
}
