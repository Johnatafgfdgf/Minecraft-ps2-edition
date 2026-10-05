#include "mcps2/noise_chunk_graph.hpp"
#include <cstring>
#include <limits>
#include <initializer_list>

namespace mcps2 {
namespace {
constexpr uint32_t missing=UINT32_MAX;
uint64_t bits(double value) noexcept {
    if (value!=value) return 0x7ff8000000000000ULL; // Java record Double equality.
    uint64_t result;std::memcpy(&result,&value,sizeof(result));return result;
}
uint64_t hash_word(uint64_t hash, uint64_t value) noexcept { return (hash^value)*0x100000001b3ULL; }
uint64_t hash_key(const NoiseGraphKey& k) noexcept {
    const auto& s=k.spec;uint64_t h=0xcbf29ce484222325ULL;
    for (uint32_t v:{uint32_t(s.op),s.a,s.b,s.c,uint32_t(s.from_y),uint32_t(s.to_y),k.origin}) h=hash_word(h,v);
    for (double v:{s.p0,s.p1,k.minimum,k.maximum}) h=hash_word(h,bits(v));
    return h;
}
bool equal(const NoiseGraphKey& a,const NoiseGraphKey& b) noexcept {
    const auto& x=a.spec;const auto& y=b.spec;
    return x.op==y.op && x.a==y.a && x.b==y.b && x.c==y.c && x.from_y==y.from_y && x.to_y==y.to_y
        && bits(x.p0)==bits(y.p0) && bits(x.p1)==bits(y.p1) && bits(a.minimum)==bits(b.minimum)
        && bits(a.maximum)==bits(b.maximum) && a.origin==b.origin;
}
bool add(size_t& total,size_t value) noexcept {
    if (value>std::numeric_limits<size_t>::max()-total) return false;
    total+=value;return true;
}
bool multiply(size_t a,size_t b,size_t& value) noexcept {
    if (b && a>std::numeric_limits<size_t>::max()/b) return false;
    value=a*b;return true;
}
bool marker(DensityOp op) noexcept { return op>=DensityOp::interpolated && op<=DensityOp::cache_all_in_cell; }
// Ignore unused storage fields: original records compare their actual fields.
DensitySpec normalized(const DensitySpec& s) noexcept {
    DensitySpec v{};v.op=s.op;
    if (s.op==DensityOp::constant || s.op==DensityOp::spline_constant) v.p0=s.p0;
    else if (s.op==DensityOp::y_gradient) { v.from_y=s.from_y;v.to_y=s.to_y;v.p0=s.p0;v.p1=s.p1; }
    else if (s.op>=DensityOp::add && s.op<=DensityOp::maximum) { v.a=s.a;v.b=s.b; }
    else if (s.op==DensityOp::range_choice || s.op==DensityOp::shifted_noise) {
        v.a=s.a;v.b=s.b;v.c=s.c;v.p0=s.p0;v.p1=s.p1;
        if (s.op==DensityOp::shifted_noise) v.from_y=s.from_y;
    } else if (s.op==DensityOp::clamp) { v.a=s.a;v.p0=s.p0;v.p1=s.p1; }
    else if (s.op==DensityOp::add_constant || s.op==DensityOp::multiply_constant) { v.a=s.a;v.p0=s.p0; }
    else if (s.op==DensityOp::noise) { v.from_y=s.from_y;v.p0=s.p0;v.p1=s.p1; }
    else if ((s.op>=DensityOp::shift && s.op<=DensityOp::shift_b) || s.op==DensityOp::blended_noise || s.op==DensityOp::end_islands) v.from_y=s.from_y;
    else if (s.op==DensityOp::weird_scaled_sampler) { v.a=s.a;v.from_y=s.from_y;v.to_y=s.to_y; }
    else if (s.op==DensityOp::spline) { v.a=s.a;v.from_y=s.from_y; }
    else if (s.op==DensityOp::input || (s.op>=DensityOp::absolute && s.op<=DensityOp::squeeze)
        || marker(s.op) || s.op==DensityOp::blend_density || s.op==DensityOp::reference) v.a=s.a;
    return v;
}
}
NoiseChunkGraph::NoiseChunkGraph(const DensityGraph& source,NoiseChunk& chunk,NoiseGraphArena arena) noexcept
    : source_(source),chunk_(chunk),arena_(arena),
      graph_(arena.nodes,arena.node_capacity,arena.inputs,arena.input_capacity,source.noises(),source.noise_count(),arena.splines,arena.spline_capacity) {}
DensityId NoiseChunkGraph::mapped(DensityId source) const noexcept {
    return prepared_ && source<source_.size()?arena_.memo[source]:missing;
}
size_t NoiseChunkGraph::child_count(DensityId id) const noexcept {
    const auto& s=source_.node(id)->spec;
    if (s.op==DensityOp::spline) return 1+source_.splines()[s.from_y].count;
    if (s.op>=DensityOp::add && s.op<=DensityOp::maximum) return 2;
    if (s.op==DensityOp::range_choice || s.op==DensityOp::shifted_noise) return 3;
    return s.op==DensityOp::clamp || (s.op>=DensityOp::absolute && s.op<=DensityOp::squeeze)
        || s.op==DensityOp::add_constant || s.op==DensityOp::multiply_constant || marker(s.op)
        || s.op==DensityOp::blend_density || s.op==DensityOp::reference || s.op==DensityOp::weird_scaled_sampler ? 1:0;
}
DensityId NoiseChunkGraph::child_at(DensityId id,size_t i) const noexcept {
    const auto& s=source_.node(id)->spec;
    if (s.op==DensityOp::spline && i) return source_.splines()[s.from_y].points[i-1].value;
    return i==0?s.a:i==1?s.b:s.c;
}
size_t NoiseChunkGraph::slot(const NoiseGraphKey& key) const noexcept {
    size_t i=size_t(hash_key(key))&(arena_.slot_capacity-1);
    while (arena_.slots[i]!=missing && !equal(key,arena_.keys[arena_.slots[i]])) i=(i+1)&(arena_.slot_capacity-1);
    return i;
}
DensityResult NoiseChunkGraph::transform(DensityId id,DensityId& result) noexcept {
    DensitySpec s=source_.node(id)->spec;
    const size_t children=child_count(id);
    if (children) s.a=arena_.memo[s.a];
    if (s.op==DensityOp::reference) { result=s.a;return DensityResult::ok; }
    if (children>1 && s.op!=DensityOp::spline) s.b=arena_.memo[s.b];
    if (children>2 && s.op!=DensityOp::spline) s.c=arena_.memo[s.c];
    if (s.op==DensityOp::spline) {
        auto& spline=arena_.splines[s.from_y];const auto& original=source_.splines()[s.from_y];
        // A source spline's arrays retain their identity through mapAll. The
        // descriptor index is that identity, even for equal array contents.
        auto* points=const_cast<DensitySplinePoint*>(spline.points);
        for (size_t i=0;i<original.count;++i) points[i].value=arena_.memo[original.points[i].value];
    }
    NoiseGraphKey key{};size_t where;
    if (marker(s.op)) {
        const auto* child=graph_.node(s.a);key.spec=normalized(s);key.minimum=child->minimum;key.maximum=child->maximum;
        where=slot(key);
        if (arena_.slots[where]!=missing) { result=arena_.keys[arena_.slots[where]].result;return DensityResult::ok; }
        if (caches_>=arena_.cache_capacity) return DensityResult::full;
        auto& cache=arena_.caches[caches_];cache=NoiseGraphCache{};
        cache.kind=NoiseCacheKind(uint8_t(s.op)-uint8_t(DensityOp::interpolated));
        cache.field.graph=&graph_;cache.field.root=s.a;cache.field.limits={child->minimum,child->maximum};
        cache.input.source=cache.field.function();
        const size_t input=source_.input_count()+caches_;
        if (input>=arena_.input_capacity || input>=UINT32_MAX) return DensityResult::full;
        cache.input.bind(arena_.inputs[input]);
        DensitySpec spec{};spec.op=DensityOp::input;spec.a=uint32_t(input);
        const auto r=graph_.append(spec,result);if (r!=DensityResult::ok) return r;
        cache.values=chunk_.required_values(cache.kind);
        if (cache.kind==NoiseCacheKind::once && cache.values<largest_array_) cache.values=largest_array_;
        if (!add(requirements_.values,cache.values)) return DensityResult::full;
        ++caches_;
    } else {
        const auto r=graph_.append_transformed(s,result);if (r!=DensityResult::ok) return r;
        const auto* node=graph_.node(result);key.spec=normalized(node->spec);key.minimum=node->minimum;key.maximum=node->maximum;
        // These two classes use object identity, not record/value equality.
        if (s.op==DensityOp::blended_noise || s.op==DensityOp::end_islands) key.origin=id+1;
        where=slot(key);
        if (arena_.slots[where]!=missing) { result=arena_.keys[arena_.slots[where]].result;return DensityResult::ok; }
    }
    if (keys_>=arena_.key_capacity) return DensityResult::full;
    key.result=result;arena_.keys[keys_]=key;arena_.slots[where]=uint32_t(keys_++);
    return DensityResult::ok;
}
bool NoiseChunkGraph::add_field_requirements(const NoiseChunkDensityField& f) noexcept {
    size_t temporary;
    return multiply(graph_.required_temporary_arrays(f.root),largest_array_,temporary)
        && add(requirements_.sample_frames,graph_.required_frames(f.root))
        && add(requirements_.batch_frames,graph_.required_frames(f.root)) && add(requirements_.temporary,temporary);
}
DensityResult NoiseChunkGraph::prepare(DensityId root,size_t largest_array) noexcept {
    if (attempted_ || chunk_.status()!=NoiseChunkResult::ok) return DensityResult::invalid_operation;
    if (!source_.node(root)) return DensityResult::invalid_reference;
    const size_t n=source_.size();
    if (n>UINT32_MAX/2 || source_.input_count()>UINT32_MAX-n || largest_array>size_t(INT32_MAX)) return DensityResult::full;
    if (!arena_.nodes || arena_.node_capacity<n || !arena_.inputs || arena_.input_capacity<source_.input_count()+n
        || !arena_.caches || arena_.cache_capacity<n || !arena_.memo || arena_.memo_capacity<n
        || !arena_.visits || arena_.visit_capacity<n || !arena_.keys || arena_.key_capacity<n
        || !arena_.slots || arena_.slot_capacity<2*n || (arena_.slot_capacity&(arena_.slot_capacity-1))) return DensityResult::workspace_full;
    if (source_.spline_count()>arena_.spline_capacity || (source_.spline_count() && !arena_.splines)) return DensityResult::workspace_full;
    size_t points=0;
    for (size_t i=0;i<source_.spline_count();++i) if (!add(points,source_.splines()[i].count)) return DensityResult::full;
    if (points>arena_.point_capacity || (points && !arena_.points)) return DensityResult::workspace_full;
    attempted_=true;largest_array_=largest_array;
    const size_t cell=chunk_.required_values(NoiseCacheKind::cell),slice=size_t(chunk_.cell_count_y())+1;
    if (largest_array_<cell) largest_array_=cell;
    if (largest_array_<slice) largest_array_=slice;
    for (size_t i=0;i<n;++i) arena_.memo[i]=missing;
    for (size_t i=0;i<arena_.slot_capacity;++i) arena_.slots[i]=missing;
    for (size_t i=0;i<source_.input_count();++i) arena_.inputs[i]=source_.inputs()[i];
    size_t offset=0;
    for (size_t i=0;i<source_.spline_count();++i) {
        const auto& original=source_.splines()[i];arena_.splines[i]={arena_.points+offset,original.count};
        for (size_t j=0;j<original.count;++j) arena_.points[offset+j]=original.points[j];
        offset+=original.count;
    }
    size_t top=1;arena_.visits[0]={root,0};
    while (top) {
        auto& frame=arena_.visits[top-1];
        if (frame.child<child_count(frame.node)) {
            const DensityId child=child_at(frame.node,frame.child++);
            if (arena_.memo[child]==missing) arena_.visits[top++]={child,0};
        } else {
            const auto r=transform(frame.node,arena_.memo[frame.node]);if (r!=DensityResult::ok) return r;
            --top;
        }
    }
    root_field_.graph=&graph_;root_field_.root=arena_.memo[root];
    const auto* node=graph_.node(root_field_.root);root_field_.limits={node->minimum,node->maximum};
    requirements_.caches=caches_;
    for (size_t i=0;i<caches_;++i) if (!add_field_requirements(arena_.caches[i].field)) return DensityResult::full;
    if (!add_field_requirements(root_field_)) return DensityResult::full;
    // Requirements are entry counts, but their byte extents must also fit the
    // target's size_t before a caller sizes its arenas (32 bits on the EE).
    const size_t maximum=std::numeric_limits<size_t>::max();
    if (requirements_.values>maximum/sizeof(double) || requirements_.temporary>maximum/sizeof(double)
        || requirements_.sample_frames>maximum/sizeof(DensityFrame)
        || requirements_.batch_frames>maximum/sizeof(DensityBatchFrame)) return DensityResult::full;
    prepared_=true;return DensityResult::ok;
}
NoiseChunkResult NoiseChunkGraph::activate(NoiseGraphWorkspace w) noexcept {
    if (!prepared_ || active_ || failed_ || chunk_.state().interpolating) return NoiseChunkResult::invalid_settings;
    const auto& r=requirements_;
    if (r.caches>chunk_.cache_capacity()-chunk_.cache_count()) return NoiseChunkResult::full;
    if (w.value_capacity<r.values || (r.values && !w.values) || w.sample_capacity<r.sample_frames || !w.samples
        || w.batch_capacity<r.batch_frames || !w.batches || w.temporary_capacity<r.temporary || (r.temporary && !w.temporary)) return NoiseChunkResult::workspace_full;
    size_t samples=0,batches=0,temporary=0,values=0;
    for (size_t i=0;i<=caches_;++i) {
        auto& f=i==caches_?root_field_:arena_.caches[i].field;
        f.frames=w.samples+samples;f.frame_capacity=graph_.required_frames(f.root);samples+=f.frame_capacity;
        f.batch_frames=w.batches+batches;f.batch_capacity=f.frame_capacity;batches+=f.batch_capacity;
        f.temporary_capacity=graph_.required_temporary_arrays(f.root)*largest_array_;
        f.temporary=f.temporary_capacity?w.temporary+temporary:nullptr;temporary+=f.temporary_capacity;
    }
    for (size_t i=0;i<caches_;++i) {
        auto& cache=arena_.caches[i];NoiseCache* wrapped=nullptr;
        const auto result=chunk_.wrap(cache.kind,cache.field.function(),cache.values?w.values+values:nullptr,cache.values,wrapped);
        if (result!=NoiseChunkResult::ok) { failed_=true;return result; }
        cache.input.source=wrapped->function();values+=cache.values;
    }
    active_=true;return NoiseChunkResult::ok;
}
}
