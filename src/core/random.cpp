#include "mcps2/random.hpp"

namespace mcps2 {
uint64_t mix_stafford13(uint64_t value) {
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
void XoroshiroRandom::set_seed(uint64_t seed) {
    const uint64_t base = seed ^ 0x6a09e667f3bcc909ULL;
    set_state(mix_stafford13(base), mix_stafford13(base + 0x9e3779b97f4a7c15ULL));
}
void XoroshiroRandom::set_state(uint64_t low, uint64_t high) {
    low_ = low; high_ = high;
    if ((low_ | high_) == 0) { low_ = 0x9e3779b97f4a7c15ULL; high_ = 0x6a09e667f3bcc909ULL; }
}
uint64_t XoroshiroRandom::next_long() {
    const uint64_t a = low_, b = high_, xor_state = a ^ b;
    const uint64_t output = rotate_left(a + b, 17) + a;
    low_ = rotate_left(a, 49) ^ xor_state ^ (xor_state << 21);
    high_ = rotate_left(xor_state, 28);
    return output;
}
bool XoroshiroRandom::next_int(int32_t bound, int32_t& value) {
    if (bound <= 0) return false;
    const uint32_t limit = uint32_t(bound);
    uint64_t product = uint64_t(uint32_t(next_int())) * limit;
    if (uint32_t(product) < limit) {
        const uint32_t threshold = (0u - limit) % limit;
        while (uint32_t(product) < threshold) product = uint64_t(uint32_t(next_int())) * limit;
    }
    value = static_cast<int32_t>(product >> 32);
    return true;
}
LegacyRandom slime_chunk_random(int32_t x, int32_t z, uint64_t seed, uint64_t salt) {
    const uint32_t ux = uint32_t(x), uz = uint32_t(z);
    const uint64_t derived = seed + sign_extend32(ux * ux * 4987142u) + sign_extend32(ux * 5947611u)
        + sign_extend32(uz * uz) * 4392871ULL + sign_extend32(uz * 389711u);
    return LegacyRandom(derived ^ salt);
}
}
