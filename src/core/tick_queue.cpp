#include "mcps2/tick_queue.hpp"

namespace mcps2 {
namespace {
template<class T> int compare(T a, T b) { return a < b ? -1 : (a > b ? 1 : 0); }
}
int intra_tick_order(const ScheduledTick& a, const ScheduledTick& b) {
    const int priority = compare(a.priority, b.priority);
    return priority ? priority : compare(a.sub_tick_order, b.sub_tick_order);
}
int drain_order(const ScheduledTick& a, const ScheduledTick& b) {
    const int time = compare(a.trigger_tick, b.trigger_tick);
    return time ? time : intra_tick_order(a, b);
}
bool ChunkTickQueue::contains(uint32_t type, uint64_t position) const {
    for (size_t i = 0; i < size_; ++i)
        if (memory_[i].type == type && memory_[i].position == position) return true;
    return false;
}
ScheduleResult ChunkTickQueue::schedule(const ScheduledTick& tick) {
    if (tick.priority < -3 || tick.priority > 3) return ScheduleResult::invalid_priority;
    if (contains(tick.type, tick.position)) return ScheduleResult::duplicate;
    if (size_ == capacity_) return ScheduleResult::full;
    size_t hole = size_++;
    while (hole) {
        const size_t parent = (hole - 1) / 2;
        if (drain_order(tick, memory_[parent]) >= 0) break;
        memory_[hole] = memory_[parent]; hole = parent;
    }
    memory_[hole] = tick;
    return ScheduleResult::inserted;
}
bool ChunkTickQueue::poll(ScheduledTick& result) {
    if (!size_) return false;
    result = memory_[0];
    const ScheduledTick moved = memory_[--size_];
    if (!size_) return true;
    size_t hole = 0;
    while (hole < size_ / 2) {
        size_t child = hole * 2 + 1;
        if (child + 1 < size_ && drain_order(memory_[child + 1], memory_[child]) < 0) ++child;
        if (drain_order(moved, memory_[child]) <= 0) break;
        memory_[hole] = memory_[child]; hole = child;
    }
    memory_[hole] = moved;
    return true;
}
bool ChunkTickQueue::poll_due(int64_t now, ScheduledTick& result) {
    return size_ && memory_[0].trigger_tick <= now && poll(result);
}
}
