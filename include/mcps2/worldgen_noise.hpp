#pragma once
#include "mcps2/octave_noise.hpp"
#include "mcps2/blended_noise.hpp"
#include <string_view>

namespace mcps2 {
// Independent numeric binding corresponding to RandomState's noise-wiring visitor.
// Noise instances and octave pools are owned and shared by the caller.
class WorldgenNoiseFactory {
public:
    WorldgenNoiseFactory(uint64_t seed, bool legacy) noexcept
        : seed_(seed), legacy_(legacy), old_(LegacyRandom(seed).fork_positional()),
          modern_(XoroshiroRandom(seed).fork_positional()) {}
    bool normal(NormalNoise& field, std::string_view resource, int32_t first,
                const double* amplitudes, size_t count, NoiseOctave* pool, size_t capacity) const noexcept {
        if (legacy_ && (resource == "minecraft:temperature" || resource == "minecraft:vegetation")) {
            const double climate[] = {1,1};
            if (!pool || capacity < 4) return false;
            LegacyRandom source(seed_ + (resource == "minecraft:vegetation" ? 1 : 0));
            return field.initialize(source, -7, climate, 2, pool, pool + 2, 2, false);
        }
        if (legacy_ && resource == "minecraft:offset") {
            const double zero = 0;
            if (!pool || capacity < 2) return false;
            auto source = old_.from_hash_ascii(resource);
            return field.initialize(source, 0, &zero, 1, pool, pool + 1, 1, true);
        }
        if (count > capacity / 2 || (count && !pool)) return false;
        NoiseOctave* second = count ? pool + count : pool;
        if (legacy_) {
            auto source = old_.from_hash_ascii(resource);
            return field.initialize(source, first, amplitudes, count, pool, second, count, true);
        }
        auto source = modern_.from_hash_ascii(resource);
        return field.initialize(source, first, amplitudes, count, pool, second, count, true);
    }
    bool blended(BlendedNoise& field, const BlendedNoiseParameters& parameters,
                 NoiseOctave* pool, size_t capacity) const noexcept {
        if (legacy_) {
            LegacyRandom source(seed_);
            return field.initialize(source, parameters, pool, capacity);
        }
        auto source = modern_.from_hash_ascii("minecraft:terrain");
        return field.initialize(source, parameters, pool, capacity);
    }
private:
    uint64_t seed_;
    bool legacy_;
    LegacyPositionalFactory old_;
    XoroshiroPositionalFactory modern_;
};
}
