#pragma once
#include <cstdint>

namespace mcps2 {
// Presentation may use a different rate. Elapsed simulation time is never discarded.
class TickClock {
public:
    static constexpr uint64_t tick_us = 50000;
    void advance(uint64_t elapsed_us) { pending_us_ += elapsed_us; }
    bool consume() {
        if (pending_us_ < tick_us) return false;
        pending_us_ -= tick_us;
        ++tick_;
        return true;
    }
    uint64_t tick() const { return tick_; }
    uint64_t pending_us() const { return pending_us_; }
private:
    uint64_t pending_us_ = 0;
    uint64_t tick_ = 0;
};
}
