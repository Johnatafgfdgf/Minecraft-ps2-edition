#include "mcps2/random.hpp"
#include "mcps2/position.hpp"
#include "mcps2/bit_storage.hpp"
#include "mcps2/tick_queue.hpp"
#include "mcps2/tick_clock.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <limits>

int main() {
    using namespace mcps2;
    TickClock clock;
    clock.advance(49999); assert(!clock.consume());
    clock.advance(1); assert(clock.consume() && clock.tick() == 1);
    clock.advance(510000); unsigned ticks = 0;
    while (clock.consume()) ++ticks;
    assert(ticks == 10 && clock.pending_us() == 10000);
    assert(block_to_section(-1) == -1 && block_to_section(-17) == -2);
    assert(block_to_section(std::numeric_limits<int32_t>::min()) == -134217728);
    for (int x : {-33554432, -17, -1, 0, 16, 33554431}) {
        BlockPos p{x, -64, -x - 1}; const auto q = BlockPos::unpack(p.pack());
        assert(p.x == q.x && p.y == q.y && p.z == q.z);
    }
    LegacyRandom random(0), control(0); int32_t value = 456;
    assert(!random.next_int(0, value) && value == 456);
    assert(!random.next_int(-1, value));
    assert(random.next_long() == control.next_long());
    XoroshiroRandom xoro(0, 0);
    assert((xoro.low() | xoro.high()) != 0);
    for (int32_t bound : {1, 2, 3, 1000, 1073741825, 2147483647}) {
        for (int i = 0; i < 1000; ++i) {
            assert(random.next_int(bound, value) && value >= 0 && value < bound);
            assert(xoro.next_int(bound, value) && value >= 0 && value < bound);
        }
    }
    for (unsigned bits = 1; bits <= 32; ++bits) {
        const size_t count = BitStorage::required_words(bits, 4096);
        std::vector<uint64_t> storage(count + 1, 0xdeadbeefULL);
        BitStorage data;
        assert(!data.bind(bits, 4096, storage.data(), count - 1));
        assert(data.bind(bits, 4096, storage.data(), count));
        const uint32_t mask = uint32_t((uint64_t(1) << bits) - 1);
        for (uint32_t i = 0; i < 4096; ++i) assert(data.set(i, (i * 2654435761u) & mask));
        for (uint32_t i = 0; i < 4096; ++i) {
            uint32_t actual = 0;
            assert(data.get(i, actual) && actual == ((i * 2654435761u) & mask));
        }
        assert(storage[count] == 0xdeadbeefULL);
        uint32_t read = 0;
        assert(!data.get(4096, read) && !data.set(4096, 0));
        if (bits < 32) assert(!data.set(0, mask + 1));
    }
    BitStorage zero;
    assert(zero.bind(0, 4096, nullptr, 0));
    uint32_t read = 7; assert(zero.get(10, read) && read == 0 && !zero.set(10, 1));
    ScheduledTick memory[3]; ChunkTickQueue queue(memory, 3);
    assert(queue.schedule({1, 10, 1, 1, 0}) == ScheduleResult::inserted);
    assert(queue.schedule({1, 1, 2, 1, -3}) == ScheduleResult::duplicate);
    assert(queue.schedule({2, 10, 3, 1, -1}) == ScheduleResult::inserted);
    assert(queue.schedule({3, 10, 0, 2, -1}) == ScheduleResult::inserted);
    assert(queue.schedule({4, 10, 4, 2, 0}) == ScheduleResult::full);
    ScheduledTick tick;
    assert(!queue.poll_due(9, tick));
    assert(queue.poll_due(10, tick) && tick.position == 3);
    assert(queue.poll(tick) && tick.position == 2);
    assert(queue.poll(tick) && tick.position == 1);
    assert(!queue.poll(tick));
    assert(queue.schedule({1, 20, 10, 1, 0}) == ScheduleResult::inserted);
    std::puts("Core tests passed: clock debt, coordinate edges, RNG bounds, storage guards, tick identity/order.");
}
