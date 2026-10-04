#include "mcps2/java_float.hpp"
#include <cmath>
#include <cstring>

namespace mcps2::java_float {
namespace {
float from_bits(uint32_t bits) noexcept { float value;std::memcpy(&value,&bits,sizeof(value));return value; }
uint32_t bits(float value) noexcept { uint32_t result;std::memcpy(&result,&value,sizeof(result));return result; }
uint32_t rounded_significand(uint64_t significand,unsigned shift) noexcept {
    const uint64_t retained=significand>>shift,discarded=significand&((uint64_t(1)<<shift)-1);
    const uint64_t half=uint64_t(1)<<(shift-1);
    return uint32_t(retained+(discarded>half || (discarded==half && (retained&1))));
}
}
float round(double value) noexcept {
    uint64_t raw;std::memcpy(&raw,&value,sizeof(raw));
    const uint32_t sign=uint32_t(raw>>32)&0x80000000u,exponent=uint32_t(raw>>52)&0x7ff;
    const uint64_t fraction=raw&0xfffffffffffffULL;
    if (exponent==0x7ff) return from_bits(sign|0x7f800000u|(fraction?uint32_t(fraction>>29)|0x400000u:0));
    if (!exponent) return from_bits(sign); // Even the largest binary64 subnormal is below binary32.
    int power=int(exponent)-1023;const uint64_t significand=fraction|(uint64_t(1)<<52);
    if (power>127) return from_bits(sign|0x7f800000u);
    if (power < -126) {
        if (power < -150) return from_bits(sign);
        return from_bits(sign|rounded_significand(significand,unsigned(-power-97)));
    }
    uint32_t retained=rounded_significand(significand,29);
    if (retained==0x1000000u) { retained>>=1;++power; }
    if (power>127) return from_bits(sign|0x7f800000u);
    return from_bits(sign|(uint32_t(power+127)<<23)|(retained&0x7fffffu));
}
// Binary32 operands have at most 24 significand bits. Binary64 intermediates
// retain the precision needed for these operations before the explicit rounding.
float add(float a,float b) noexcept { return round(double(a)+double(b)); }
float subtract(float a,float b) noexcept { return round(double(a)-double(b)); }
float multiply(float a,float b) noexcept { return round(double(a)*double(b)); }
float divide(float a,float b) noexcept { return round(double(a)/double(b)); }
float remainder(float a,float b) noexcept { return round(std::fmod(double(a),double(b))); }
float square_root(float value) noexcept { return round(std::sqrt(double(value))); }
float negate(float value) noexcept { return from_bits(bits(value)^0x80000000u); }
float absolute(float value) noexcept { return from_bits(bits(value)&0x7fffffffu); }
bool is_nan(float value) noexcept { return (bits(value)&0x7fffffffu)>0x7f800000u; }
bool negative(float value) noexcept { return (bits(value)&0x80000000u)!=0; }
bool equal(float a,float b) noexcept {
    const uint32_t x=bits(a),y=bits(b);
    if ((x&0x7fffffffu)>0x7f800000u || (y&0x7fffffffu)>0x7f800000u) return false;
    return x==y || ((x|y)&0x7fffffffu)==0;
}
bool less(float a,float b) noexcept {
    const uint32_t x=bits(a),y=bits(b);
    if ((x&0x7fffffffu)>0x7f800000u || (y&0x7fffffffu)>0x7f800000u || ((x|y)&0x7fffffffu)==0) return false;
    const uint32_t ordered_x=x&0x80000000u?~x:x|0x80000000u,ordered_y=y&0x80000000u?~y:y|0x80000000u;
    return ordered_x<ordered_y;
}
bool less_equal(float a,float b) noexcept { return less(a,b) || equal(a,b); }
}
