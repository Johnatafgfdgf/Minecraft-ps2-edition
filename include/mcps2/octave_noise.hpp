#pragma once
#include "mcps2/improved_noise.hpp"
#include "mcps2/random.hpp"
#include <cmath>
#include <cstdio>
#include <optional>
#include <cstddef>

namespace mcps2 {
struct NoiseOctave {
    double amplitude = 0;
    std::optional<ImprovedNoise> field;
};
// The caller owns contiguous octave storage. Initialization and sampling allocate no heap.
class PerlinNoise {
public:
    template<class R> bool initialize(R& source, int32_t first, const double* amplitudes,
                                    size_t count, NoiseOctave* storage, size_t capacity, bool modern = true) {
        if (count > capacity || count > 2147483647u || (count && (!storage || !amplitudes))) return false;
        ready_ = false; octaves_ = storage; count_ = count; first_ = first;
        for (size_t i = 0; i < count; ++i) { storage[i].amplitude = amplitudes[i]; storage[i].field.reset(); }
        if (modern) {
            const auto factory = source.fork_positional();
            for (size_t i = 0; i < count; ++i) {
                if (storage[i].amplitude == 0.0) continue;
                char name[32];
                const int32_t number = signed32(uint32_t(first) + uint32_t(i));
                const int length = std::snprintf(name, sizeof(name), "octave_%ld", static_cast<long>(number));
                auto child = factory.from_hash_ascii(std::string_view(name, size_t(length)));
                storage[i].field.emplace(child);
            }
        } else {
            const int32_t zero = signed32(0u - uint32_t(first));
            ImprovedNoise initial(source);
            if (zero >= 0 && size_t(zero) < count && storage[zero].amplitude != 0.0) storage[zero].field = initial;
            for (int32_t i = signed32(uint32_t(zero) - 1); i >= 0; --i) {
                if (size_t(i) < count && storage[i].amplitude != 0.0) storage[i].field.emplace(source);
                else for (unsigned skipped = 0; skipped < 262; ++skipped) (void)source.next_int();
            }
            for (size_t i = 0; i < count; ++i) if ((storage[i].amplitude != 0.0) != bool(storage[i].field)) return false;
            if (int64_t(zero) < int64_t(count) - 1) return false; // Original Legacy constructors reject positive octaves.
        }
        input_factor_ = std::ldexp(1.0, first);
        value_factor_ = std::ldexp(1.0, int(count) - 1) / (std::ldexp(1.0, int(count)) - 1.0);
        ready_ = true; max_value_ = edge_value(2.0);
        return true;
    }
    double sample(double x, double y, double z, double y_scale = 0, double y_max = 0, bool fixed_y = false) const;
    double max_value() const { assert(ready_); return max_value_; }
    double max_broken_value(double offset) const { assert(ready_); return edge_value(offset + 2.0); }
    const ImprovedNoise* octave(size_t reversed_index) const {
        if (!ready_ || reversed_index >= count_) return nullptr;
        const auto& field = octaves_[count_ - 1 - reversed_index].field;
        return field ? &*field : nullptr;
    }
    size_t size() const { return count_; }
    bool ready() const { return ready_; }
    static double wrap(double value);
private:
    double edge_value(double edge) const;
    NoiseOctave* octaves_ = nullptr;
    size_t count_ = 0;
    int32_t first_ = 0;
    double input_factor_ = 0, value_factor_ = 0, max_value_ = 0;
    bool ready_ = false;
};
class NormalNoise {
public:
    template<class R> bool initialize(R& source, int32_t first, const double* amplitudes, size_t count,
                                     NoiseOctave* first_storage, NoiseOctave* second_storage, size_t capacity, bool modern = true) {
        ready_ = false;
        if (count && first_storage == second_storage) return false;
        if (!first_.initialize(source, first, amplitudes, count, first_storage, capacity, modern)
            || !second_.initialize(source, first, amplitudes, count, second_storage, capacity, modern)) return false;
        int32_t lowest = 2147483647, highest = signed32(0x80000000u);
        for (size_t i = 0; i < count; ++i) if (amplitudes[i] != 0.0) {
            if (int32_t(i) < lowest) lowest = int32_t(i);
            if (int32_t(i) > highest) highest = int32_t(i);
        }
        const int32_t span_plus_one = signed32(uint32_t(highest) - uint32_t(lowest) + 1);
        const double deviation = 0.1 * (1.0 + 1.0 / double(span_plus_one));
        factor_ = (1.0 / 6.0) / deviation;
        max_value_ = (first_.max_value() + second_.max_value()) * factor_;
        ready_ = true; return true;
    }
    double sample(double x, double y, double z) const;
    double max_value() const { assert(ready_); return max_value_; }
    bool ready() const { return ready_; }
private:
    PerlinNoise first_, second_;
    double factor_ = 0, max_value_ = 0;
    bool ready_ = false;
};
}
