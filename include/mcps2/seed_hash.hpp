#pragma once
#include <cstdint>
#include <string_view>

namespace mcps2 {
struct Seed128 { uint64_t low, high; };
uint32_t java_string_hash(std::u16string_view text);
// MD5 is a compatibility transform for seed names, with big-endian digest halves.
Seed128 seed_from_utf8(std::string_view bytes);
// Java UTF-8 encoding: valid surrogate pairs combine; unmatched units become '?'.
Seed128 seed_from_java_string(std::u16string_view text);
uint64_t positional_seed(int32_t x, int32_t y, int32_t z);
}
