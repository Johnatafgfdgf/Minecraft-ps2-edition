#include "mcps2/noise_chunk.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>

namespace {
using namespace mcps2;
struct Probe {
    unsigned calls=0;
    static NoiseChunkResult sample(void* state,const NoiseChunkContext& c,double& value) noexcept {
        ++static_cast<Probe*>(state)->calls;const auto p=c.position();value=double(p.y)+double(p.x)*0.25+double(p.z)*0.125;return NoiseChunkResult::ok;
    }
    static NoiseChunkResult fill(void* state,double* out,size_t n,NoiseChunkProvider& p) noexcept {
        return p.fill_direct(out,n,{state,sample,fill});
    }
    NoiseChunkFunction function() noexcept { return {this,sample,fill}; }
};
uint64_t bits(double value) { uint64_t result;std::memcpy(&result,&value,8);return result; }
}
int main() {
    using namespace mcps2;
    Probe probe;
    NoiseCache entries[4];NoiseChunk chunk({-64,384,4,8,4,-17,-33},entries,4);
    assert(chunk.status()==NoiseChunkResult::ok);
    assert(chunk.required_values(NoiseCacheKind::interpolated)==490);
    assert(chunk.required_values(NoiseCacheKind::flat)==25);
    assert(chunk.required_values(NoiseCacheKind::cell)==128);
    assert(chunk.required_values(NoiseCacheKind::once)==128);
    NoiseCache* cache=reinterpret_cast<NoiseCache*>(uintptr_t(1));
    double guarded[27];for (double& value:guarded) value=91;
    assert(chunk.wrap(NoiseCacheKind::flat,probe.function(),guarded+1,24,cache)==NoiseChunkResult::workspace_full);
    assert(!cache && probe.calls==0);
    for (double value:guarded) assert(value==91);
    assert(chunk.wrap(NoiseCacheKind::flat,probe.function(),guarded+1,25,cache)==NoiseChunkResult::ok);
    assert(probe.calls==25 && guarded[0]==91 && guarded[26]==91);
    assert(chunk.first_cell_x()==-5 && chunk.first_quart_x()==-5 && chunk.first_cell_z()==-9 && chunk.first_quart_z()==-9);
    NoiseChunkContext context;
    assert(chunk.cell_provider().for_index(-1,context)==NoiseChunkResult::ok);
    assert(chunk.state().in_x==3 && chunk.state().in_y==8 && chunk.state().in_z==3);
    double small[2]={91,91};const auto before=chunk.state();
    assert(chunk.cell_provider().fill_direct(small,2,probe.function())==NoiseChunkResult::workspace_full);
    assert(chunk.state().array_index==before.array_index && small[0]==91 && small[1]==91);
    NoiseChunk invalid({0,16,0,8,4,0,0},nullptr,0);
    assert(invalid.status()==NoiseChunkResult::invalid_settings);
    assert(invalid.cell_provider().for_index(0,context)==NoiseChunkResult::invalid_settings);
    assert(invalid.initialize_first_x()==NoiseChunkResult::invalid_settings);
    NoiseChunk overflow({0,INT_MAX,INT_MAX,INT_MAX,INT_MAX,0,0},nullptr,0);
    assert(overflow.status()==NoiseChunkResult::invalid_settings);
    NoiseChunk empty({0,16,4,8,1,0,0},nullptr,0);
    assert(empty.wrap(NoiseCacheKind::column,probe.function(),nullptr,0,cache)==NoiseChunkResult::full && !cache);
    assert(empty.stop()==NoiseChunkResult::inactive);

    // Connect a real point graph to the chunk filler without allocating in
    // sample/fill. The reference suite separately covers numeric bit parity.
    DensityNode nodes[1];DensityFrame frames[1];DensityGraph graph(nodes,1);DensityId root;
    assert(graph.append({DensityOp::y_gradient,0,0,0,-65,104,-1.3,2.7},root)==DensityResult::ok);
    NoiseChunkDensityField field{&graph,root,frames,1};
    NoiseCache arena[3];NoiseChunk interpolator({-64,24,4,8,2,-17,-33},arena,3);
    double slices[24],once_buffer[128],cell_buffer[128];NoiseCache *interpolated,*once,*cell;
    assert(interpolator.wrap(NoiseCacheKind::interpolated,field.function(),slices,24,interpolated)==NoiseChunkResult::ok);
    assert(interpolator.wrap(NoiseCacheKind::once,interpolated->function(),once_buffer,128,once)==NoiseChunkResult::ok);
    assert(interpolator.wrap(NoiseCacheKind::cell,once->function(),cell_buffer,128,cell)==NoiseChunkResult::ok);
    auto f=interpolated->function();double result=91;
    assert(bits(f.minimum())==bits(graph.node(root)->minimum) && bits(f.maximum())==bits(graph.node(root)->maximum));
    assert(f.sample(f.state,interpolator.context(),result)==NoiseChunkResult::inactive && result==91);
    assert(interpolator.initialize_first_x()==NoiseChunkResult::ok);
    assert(interpolator.initialize_first_x()==NoiseChunkResult::inactive);
    assert(interpolator.advance_x(0)==NoiseChunkResult::ok);
    const auto stable=interpolator.state();
    assert(interpolator.select_yz(-1,0)==NoiseChunkResult::invalid_index);
    assert(interpolator.state().array_counter==stable.array_counter);
    assert(interpolator.select_yz(2,0)==NoiseChunkResult::ok);
    interpolator.update_y(-43,0.625);interpolator.update_x(-19,0.25);interpolator.update_z(-34,0.5);
    assert(f.sample(f.state,interpolator.context(),result)==NoiseChunkResult::ok);
    assert(bits(result)==0xbfe8eff1753eb662ULL);
    f=cell->function();assert(f.sample(f.state,interpolator.context(),result)==NoiseChunkResult::ok);
    assert(bits(result)==0xbfe8eff1753eb662ULL);
    assert(interpolator.stop()==NoiseChunkResult::ok);
    assert(f.sample(f.state,interpolator.context(),result)==NoiseChunkResult::inactive);
    field.frames=nullptr;f=field.function();result=91;
    assert(f.sample(f.state,{{0,0,0},nullptr},result)==NoiseChunkResult::workspace_full && result==91);
    std::puts("NoiseChunk resource tests passed: external arenas, guards, geometry overflow, state errors and DensityGraph bridge.");
}
