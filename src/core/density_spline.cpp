#include "mcps2/density_spline.hpp"
#include "mcps2/java_math.hpp"
#include "mcps2/java_float.hpp"
#include <limits>

namespace mcps2 {
using namespace java_float;
namespace {
float lerp(float amount,float from,float to) noexcept { return add(from,multiply(amount,subtract(to,from))); }
}
size_t spline_upper(const DensitySpline& spline,float coordinate) noexcept {
    size_t low = 0, high = spline.count;
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (less(coordinate,spline.points[middle].location)) high = middle;
        else low = middle + 1;
    }
    return low;
}
float spline_extrapolate(float coordinate,const DensitySplinePoint& point,float value) noexcept {
    return equal(point.derivative,0) ? value : add(value,multiply(point.derivative,subtract(coordinate,point.location)));
}
float spline_segment(float coordinate,const DensitySplinePoint& left,const DensitySplinePoint& right,float a,float b) noexcept {
    const float width = subtract(right.location,left.location);
    const float amount = divide(subtract(coordinate,left.location),width);
    const float low_tangent = subtract(multiply(left.derivative,width),subtract(b,a));
    const float high_tangent = add(multiply(negate(right.derivative),width),subtract(b,a));
    return add(lerp(amount,a,b),multiply(multiply(amount,subtract(1.0f,amount)),lerp(amount,low_tangent,high_tangent)));
}
void spline_bounds(const DensitySpline& s,const DensityNode* nodes,float coordinate_min,float coordinate_max,
                   float& minimum,float& maximum) noexcept {
    minimum = std::numeric_limits<float>::infinity(); maximum = -std::numeric_limits<float>::infinity();
    const auto& first = s.points[0]; const auto& last = s.points[s.count - 1];
    if (less(coordinate_min,first.location)) {
        const float a = spline_extrapolate(coordinate_min,first,round(nodes[first.value].minimum));
        const float b = spline_extrapolate(coordinate_min,first,round(nodes[first.value].maximum));
        minimum = java_minimum(minimum,java_minimum(a,b)); maximum = java_maximum(maximum,java_maximum(a,b));
    }
    if (less(last.location,coordinate_max)) {
        const float a = spline_extrapolate(coordinate_max,last,round(nodes[last.value].minimum));
        const float b = spline_extrapolate(coordinate_max,last,round(nodes[last.value].maximum));
        minimum = java_minimum(minimum,java_minimum(a,b)); maximum = java_maximum(maximum,java_maximum(a,b));
    }
    for (size_t i = 0; i < s.count; ++i) {
        const auto& value = nodes[s.points[i].value];
        minimum = java_minimum(minimum,round(value.minimum)); maximum = java_maximum(maximum,round(value.maximum));
    }
    for (size_t i = 0; i + 1 < s.count; ++i) {
        const auto& a = s.points[i]; const auto& b = s.points[i + 1];
        if (equal(a.derivative,0) && equal(b.derivative,0)) continue;
        const float width = subtract(b.location,a.location);
        const float a_low = round(nodes[a.value].minimum), a_high = round(nodes[a.value].maximum);
        const float b_low = round(nodes[b.value].minimum), b_high = round(nodes[b.value].maximum);
        const float left = multiply(a.derivative,width), right = multiply(b.derivative,width);
        const float low = java_minimum(a_low,b_low), high = java_maximum(a_high,b_high);
        const float low_left = add(subtract(left,b_high),a_low), high_left = add(subtract(left,b_low),a_high);
        const float low_right = subtract(add(negate(right),b_low),a_high), high_right = subtract(add(negate(right),b_high),a_low);
        const float correction_low = java_minimum(low_left,low_right), correction_high = java_maximum(high_left,high_right);
        minimum = java_minimum(minimum,add(low,multiply(0.25f,correction_low)));
        maximum = java_maximum(maximum,add(high,multiply(0.25f,correction_high)));
    }
}
}
