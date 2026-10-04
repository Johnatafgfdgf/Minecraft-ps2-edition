#pragma once
#include <cstddef>
#include <cstdint>

namespace mcps2 {
struct ScheduledTick {
    uint64_t position = 0;
    int64_t trigger_tick = 0, sub_tick_order = 0;
    uint32_t type = 0;
    int8_t priority = 0; // -3 through +3: smaller runs first.
};
int drain_order(const ScheduledTick& a, const ScheduledTick& b);
int intra_tick_order(const ScheduledTick& a, const ScheduledTick& b);
enum class ScheduleResult { inserted, duplicate, full, invalid_priority };

// This is a per-chunk pending queue, not the cross-chunk LevelTicks execution order.
class ChunkTickQueue {
public:
    ChunkTickQueue(ScheduledTick* memory, size_t capacity) : memory_(memory), capacity_(memory ? capacity : 0) {}
    ScheduleResult schedule(const ScheduledTick& tick);
    bool contains(uint32_t type, uint64_t position) const;
    const ScheduledTick* peek() const { return size_ ? memory_ : nullptr; }
    bool poll(ScheduledTick& result);
    bool poll_due(int64_t now, ScheduledTick& result);
    size_t size() const { return size_; }
private:
    ScheduledTick* memory_;
    size_t capacity_, size_ = 0;
};
}
