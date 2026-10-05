#include "noise_graph_fixture.hpp"
#include <cassert>
#include <cstdio>

namespace {
using namespace mcps2;
double probe(const void* state,DensityContext context) noexcept {
    ++*static_cast<unsigned*>(const_cast<void*>(state));return context.y;
}
}
int main() {
    using namespace mcps2;
    unsigned calls=0;DensityInput input{&calls,probe,-128,128};
    DensityNode nodes[8];DensityGraph source(nodes,8,&input,1);DensityId root;
    assert(source.append({DensityOp::input},root)==DensityResult::ok);
    assert(source.append({DensityOp::flat_cache,0},root)==DensityResult::ok);
    assert(source.append({DensityOp::flat_cache,0},root)==DensityResult::ok);
    assert(source.append({DensityOp::add,1,2},root)==DensityResult::ok);
    assert(source.append({DensityOp::constant,0,0,0,0,0,0.25},root)==DensityResult::ok);
    assert(source.append({DensityOp::interpolated,4},root)==DensityResult::ok); // unreachable
    const DensityId selected=3;
    NoiseCache entries[2];NoiseChunk chunk({-64,16,4,8,1,-32,64},entries,2);
    NoiseGraphStorage storage(source);auto arena=storage.arena();
    arena.slot_capacity=1;
    NoiseChunkGraph insufficient(source,chunk,arena);
    assert(insufficient.prepare(selected,129)==DensityResult::workspace_full);
    assert(calls==0 && chunk.cache_count()==0);
    NoiseChunkGraph binder(source,chunk,storage.arena());
    assert(binder.prepare(selected,129)==DensityResult::ok);
    assert(!binder.function().sample && calls==0 && chunk.cache_count()==0);
    assert(binder.mapped(1)==binder.mapped(2) && binder.mapped(5)==UINT32_MAX);
    assert(source.node(1)->spec.op==DensityOp::flat_cache && source.node(2)->spec.op==DensityOp::flat_cache);
    const auto r=binder.requirements();assert(r.caches==1 && r.values==4 && r.temporary==129);
    std::vector<double> values(r.values,91),temporary(r.temporary,91);
    std::vector<DensityFrame> samples(r.sample_frames);std::vector<DensityBatchFrame> batches(r.batch_frames);
    NoiseGraphWorkspace w{values.data(),values.size(),samples.data(),samples.size(),batches.data(),batches.size(),temporary.data(),temporary.size()};
    --w.sample_capacity;
    assert(binder.activate(w)==NoiseChunkResult::workspace_full);
    assert(calls==0 && chunk.cache_count()==0);
    for (double v:values) assert(v==91);
    ++w.sample_capacity;
    assert(binder.activate(w)==NoiseChunkResult::ok);
    assert(calls==4 && chunk.cache_count()==1);
    const auto f=binder.function();double value=91;
    assert(f.sample(f.state,{{-32,99,64},nullptr},value)==NoiseChunkResult::ok && value==0);
    assert(calls==4); // two equal markers use the one constructed flat cache.
    assert(binder.activate(w)==NoiseChunkResult::invalid_settings);

    // Capacity for wrapper registration is checked before constructing flat
    // caches, even when every numeric workspace is sufficient.
    NoiseChunk no_entries({-64,16,4,8,1,0,0},nullptr,0);NoiseGraphStorage other(source);
    NoiseChunkGraph full(source,no_entries,other.arena());
    assert(full.prepare(selected,129)==DensityResult::ok);
    assert(full.activate(w)==NoiseChunkResult::full && calls==4 && no_entries.cache_count()==0);

    // Constant spline is a SimpleFunction, with binary32 bounds/value; it must
    // neither be folded as DensityFunctions.Constant nor share its marker.
    DensityNode typed_nodes[7];DensityGraph typed(typed_nodes,7);
    assert(typed.append({DensityOp::constant,0,0,0,0,0,0.1},root)==DensityResult::ok);
    assert(typed.append({DensityOp::spline_constant,0,0,0,0,0,0.1},root)==DensityResult::ok);
    assert(typed.append({DensityOp::cache_once,0},root)==DensityResult::ok);
    assert(typed.append({DensityOp::cache_once,1},root)==DensityResult::ok);
    assert(typed.append({DensityOp::add,2,3},root)==DensityResult::ok);
    NoiseGraphStorage ts(typed);NoiseCache typed_entries[2];NoiseChunk tc({-64,16,4,8,1,0,0},typed_entries,2);
    NoiseChunkGraph types(typed,tc,ts.arena());
    assert(types.prepare(root,129)==DensityResult::ok && types.requirements().caches==2);
    assert(types.mapped(2)!=types.mapped(3));

    // MulOrAdd must retain its argument even when unwrapping its input reveals
    // another Constant. Reapplying the binary factory would swap signed zero.
    DensityNode special_nodes[5];DensityGraph special(special_nodes,5);
    assert(special.append({DensityOp::constant,0,0,0,0,0,-0.0},root)==DensityResult::ok);
    assert(special.append({DensityOp::reference,0},root)==DensityResult::ok);
    assert(special.append({DensityOp::constant,0,0,0,0,0,0.75},root)==DensityResult::ok);
    assert(special.append({DensityOp::add,1,2},root)==DensityResult::ok);
    assert(special.node(root)->spec.op==DensityOp::add_constant);
    NoiseGraphStorage ss(special);NoiseChunk sc({-64,16,4,8,1,0,0},nullptr,0);
    NoiseChunkGraph specialization(special,sc,ss.arena());
    assert(specialization.prepare(root,129)==DensityResult::ok);
    const auto* mapped=specialization.graph().node(specialization.root());
    assert(mapped->spec.op==DensityOp::add_constant && mapped->spec.p0==0.75);
    assert(graph_bits(specialization.graph().node(mapped->spec.a)->spec.p0)==0x8000000000000000ULL);
    std::puts("NoiseChunk graph arena, preflight, sharing and type checks passed");
}
