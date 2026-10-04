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

    // Checked graph inputs preserve the cache's owner and failures. A saved
    // array epoch is reusable without invoking an otherwise unused provider.
    field.frames=frames;
    NoiseCache saved_arena[1];NoiseChunk saved({0,8,4,8,1,0,0},saved_arena,1);
    double saved_buffer[128],first[128],copied[128];NoiseCache* saved_once;
    assert(saved.wrap(NoiseCacheKind::once,field.function(),saved_buffer,128,saved_once)==NoiseChunkResult::ok);
    auto saved_function=saved_once->function();
    assert(saved_function.fill(saved_function.state,first,128,saved.cell_provider())==NoiseChunkResult::ok);
    NoiseChunkDensityInput binding{saved_function};DensityInput input;binding.bind(input);
    DensityNode input_node[1];DensityFrame input_frame[1];DensityBatchFrame input_batch[1];
    DensityGraph input_graph(input_node,1,&input,1);DensityId input_root;
    assert(input_graph.append({DensityOp::input,0},input_root)==DensityResult::ok);
    DensityBatchProvider unused;
    const auto counter=saved.state().array_index;
    assert(input_graph.fill(input_root,copied,128,unused,input_frame,1,input_batch,1,nullptr,0)==DensityResult::ok);
    assert(saved.state().array_index==counter);
    for (unsigned i=0;i<128;++i) assert(bits(first[i])==bits(copied[i]));
    assert(saved.cell_provider().for_index(-1,context)==NoiseChunkResult::ok);result=91;
    assert(input_graph.sample(input_root,context.position(),input_frame,1,result)==DensityResult::invalid_index && result==91);
    binding.source=cell->function();binding.bind(input);result=91;
    assert(input_graph.sample(input_root,interpolator.position(),input_frame,1,result)==DensityResult::inactive && result==91);
    std::puts("NoiseChunk resource tests passed: external arenas, guards, geometry overflow, state errors and DensityGraph bridge.");
}
