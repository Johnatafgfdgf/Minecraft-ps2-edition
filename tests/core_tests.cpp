#include "mcps2/random.hpp"
#include "mcps2/position.hpp"
#include "mcps2/bit_storage.hpp"
#include "mcps2/tick_queue.hpp"
#include "mcps2/tick_clock.hpp"
#include "mcps2/boot_checks.hpp"
#include "mcps2/octave_noise.hpp"
#include "mcps2/blended_noise.hpp"
#include "mcps2/density_graph.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <limits>

int main() {
    using namespace mcps2;
    assert(run_boot_checks() == all_boot_checks);
    const auto empty=seed_from_utf8("");
    assert(empty.low==0xd41d8cd98f00b204ULL && empty.high==0xe9800998ecf8427eULL);
    const auto abc=seed_from_utf8("abc");
    assert(abc.low==0x900150983cd24fb0ULL && abc.high==0xd6963f7d28e17f72ULL);
    NoiseOctave octave_memory[3];
    const double sparse[3]={1,0,0.25};
    PerlinNoise field; LegacyRandom first_source(42),untouched(42);
    assert(!field.initialize(first_source,-4,sparse,3,octave_memory,2));
    assert(first_source.next_long()==untouched.next_long());
    assert(field.initialize(first_source,-4,sparse,3,octave_memory,3));
    assert(field.ready() && field.octave(0) && !field.octave(1) && field.octave(2) && !field.octave(3));
    NormalNoise combined;
    assert(!combined.initialize(first_source,-4,sparse,3,octave_memory,octave_memory,3));
    assert(!combined.ready());
    NoiseOctave blended_memory[BlendedNoise::required_octaves]; BlendedNoise blended;
    LegacyRandom limited(17),same(17);
    assert(!blended.initialize(limited,{0.25,0.125,80,160,8},blended_memory,BlendedNoise::required_octaves-1));
    assert(limited.next_long()==same.next_long() && !blended.ready());
    std::vector<DensityNode> density_nodes(2049); DensityGraph graph(density_nodes.data(),density_nodes.size());
    DensitySpec s; DensityId id=123;
    s.op=DensityOp::absolute;
    assert(graph.append(s,id)==DensityResult::invalid_reference && graph.size()==0 && id==123);
    s.op=DensityOp::constant;s.p0=-0.25;
    assert(graph.append(s,id)==DensityResult::ok);
    for (unsigned i=1;i<density_nodes.size();++i) {
        s.op=DensityOp::half_negative;s.a=id;
        assert(graph.append(s,id)==DensityResult::ok);
    }
    assert(graph.required_frames(id)==density_nodes.size());
    std::vector<DensityFrame> density_frames(graph.required_frames(id)); double density_value=17;
    assert(graph.sample(id,{0,0,0},density_frames.data(),density_frames.size()-1,density_value)==DensityResult::workspace_full);
    assert(density_value==17);
    assert(graph.sample(id,{0,0,0},density_frames.data(),density_frames.size(),density_value)==DensityResult::ok && density_value==0);
    const auto previous_id=id;
    assert(graph.append(s,id)==DensityResult::full && id==previous_id);
    assert(graph.sample(uint32_t(graph.size()),{0,0,0},density_frames.data(),density_frames.size(),density_value)==DensityResult::invalid_reference);
    // Deep value dependencies require frames too, even when the spline coordinate is a leaf.
    std::vector<DensityNode> spline_nodes(1026);std::vector<DensitySplinePoint> spline_points(1025);
    std::vector<DensitySpline> spline_bindings(1025);
    for (unsigned i=0;i<spline_points.size();++i) {
        spline_points[i]={0,0,i};spline_bindings[i]={&spline_points[i],1};
    }
    DensityGraph spline_graph(spline_nodes.data(),spline_nodes.size(),nullptr,0,nullptr,0,spline_bindings.data(),spline_bindings.size());
    s={DensityOp::constant,0,0,0,0,0,-0.25};assert(spline_graph.append(s,id)==DensityResult::ok);
    s={DensityOp::spline,0};s.from_y=1024;
    assert(spline_graph.append(s,id)==DensityResult::invalid_reference && spline_graph.size()==1);
    for (unsigned i=0;i<spline_points.size();++i) {
        s.from_y=int32_t(i);assert(spline_graph.append(s,id)==DensityResult::ok);
    }
    assert(spline_graph.required_frames(id)==spline_nodes.size());
    density_value=17;
    assert(spline_graph.sample(id,{0,0,0},density_frames.data(),spline_nodes.size()-1,density_value)==DensityResult::workspace_full && density_value==17);
    assert(spline_graph.sample(id,{0,0,0},density_frames.data(),density_frames.size(),density_value)==DensityResult::ok && density_value==-0.25);
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
