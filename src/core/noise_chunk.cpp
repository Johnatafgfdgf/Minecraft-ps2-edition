#include "mcps2/noise_chunk.hpp"
#include "mcps2/java_bits.hpp"
#include "mcps2/position.hpp"
#include <cstring>
#include <limits>

namespace mcps2 {
namespace {
int32_t sum(int32_t a, int32_t b) noexcept { return signed32(uint32_t(a)+uint32_t(b)); }
int32_t difference(int32_t a, int32_t b) noexcept { return signed32(uint32_t(a)-uint32_t(b)); }
int32_t product(int32_t a, int32_t b) noexcept { return signed32(uint32_t(a)*uint32_t(b)); }
int32_t floor_div(int32_t a, int32_t b) noexcept { return a/b-(a%b<0?1:0); }
int32_t floor_mod(int32_t a, int32_t b) noexcept { const int32_t r=a%b; return r<0?r+b:r; }
bool multiply_size(size_t a, size_t b, size_t& out) noexcept {
    if (b && a>std::numeric_limits<size_t>::max()/b) return false;
    out=a*b; return true;
}
double lerp(double t, double a, double b) noexcept { return a+t*(b-a); }
NoiseChunkResult sample(NoiseChunkFunction f, const NoiseChunkContext& c, double& v) noexcept {
    return f.sample?f.sample(f.state,c,v):NoiseChunkResult::invalid_settings;
}
NoiseChunkResult fill(NoiseChunkFunction f, double* v, size_t n, NoiseChunkProvider& p) noexcept {
    if (n && !v) return NoiseChunkResult::workspace_full;
    return f.fill?f.fill(f.state,v,n,p):NoiseChunkResult::invalid_settings;
}
DensityResult density_result(NoiseChunkResult r) noexcept {
    switch (r) {
    case NoiseChunkResult::ok: return DensityResult::ok;
    case NoiseChunkResult::inactive: return DensityResult::inactive;
    case NoiseChunkResult::invalid_index: return DensityResult::invalid_index;
    case NoiseChunkResult::workspace_full: return DensityResult::workspace_full;
    case NoiseChunkResult::full: return DensityResult::full;
    default: return DensityResult::invalid_operation;
    }
}
NoiseChunkResult chunk_result(DensityResult r) noexcept {
    switch (r) {
    case DensityResult::ok: return NoiseChunkResult::ok;
    case DensityResult::inactive: return NoiseChunkResult::inactive;
    case DensityResult::invalid_index: return NoiseChunkResult::invalid_index;
    case DensityResult::workspace_full: return NoiseChunkResult::workspace_full;
    case DensityResult::full: return NoiseChunkResult::full;
    default: return NoiseChunkResult::invalid_settings;
    }
}
struct ChunkSampleBridge {
    NoiseChunkFunction function;
    static DensityResult compute(const void* state, DensityContext context, double& value) noexcept {
        const auto& p=*static_cast<const ChunkSampleBridge*>(state);
        return density_result(sample(p.function,{context,context.owner},value));
    }
};
struct BatchSampleBridge {
    DensitySampleFunction function;
    static NoiseChunkResult compute(void* state, const NoiseChunkContext& context, double& value) noexcept {
        const auto& p=*static_cast<const BatchSampleBridge*>(state);
        return p.function.sample?chunk_result(p.function.sample(p.function.state,context.position(),value)):NoiseChunkResult::invalid_settings;
    }
};
struct BatchProviderBridge {
    NoiseChunkProvider* provider;
    static DensityResult at(void* state, int32_t i, DensityContext& context) noexcept {
        auto& p=*static_cast<BatchProviderBridge*>(state); NoiseChunkContext c;
        const auto r=p.provider->for_index(i,c);
        if (r==NoiseChunkResult::ok) context=c.position();
        return density_result(r);
    }
    static DensityResult direct(void* state, double* out, size_t n, DensitySampleFunction f) noexcept {
        auto& p=*static_cast<BatchProviderBridge*>(state); BatchSampleBridge bridge{f};
        return density_result(p.provider->fill_direct(out,n,{&bridge,BatchSampleBridge::compute,nullptr,nullptr}));
    }
};
struct ChunkProviderBridge {
    DensityBatchProvider* provider;
    static NoiseChunkResult at(void* state, int32_t i, NoiseChunkContext& context) noexcept {
        auto& p=*static_cast<ChunkProviderBridge*>(state); DensityContext c{0,0,0};
        if (!p.provider->at) return NoiseChunkResult::invalid_settings;
        const auto r=p.provider->at(p.provider->state,i,c);
        if (r==DensityResult::ok) context={c,c.owner};
        return chunk_result(r);
    }
    static NoiseChunkResult direct(void* state, double* out, size_t n, NoiseChunkFunction f) noexcept {
        auto& p=*static_cast<ChunkProviderBridge*>(state); ChunkSampleBridge bridge{f};
        if (!p.provider->direct) return NoiseChunkResult::invalid_settings;
        return chunk_result(p.provider->direct(p.provider->state,out,n,{&bridge,ChunkSampleBridge::compute}));
    }
};
}
DensityContext NoiseChunkContext::position() const noexcept { return owner?owner->position():point; }
NoiseChunkResult NoiseChunkProvider::for_index(int32_t i, NoiseChunkContext& c) noexcept {
    return at?at(state,i,c):NoiseChunkResult::invalid_settings;
}
NoiseChunkResult NoiseChunkProvider::fill_direct(double* v, size_t n, NoiseChunkFunction f) noexcept {
    return direct?direct(state,v,n,f):NoiseChunkResult::invalid_settings;
}
NoiseChunkFunction NoiseCache::function() noexcept { return {this,compute,fill_array,child.limits}; }

NoiseChunk::NoiseChunk(NoiseChunkSettings s, NoiseCache* arena, size_t capacity) noexcept
    : settings_(s), caches_(arena), capacity_(capacity),
      cell_provider_{this,cell_at,cell_direct}, slice_provider_{this,slice_at,slice_direct} {
    if (s.cell_width<=0 || s.cell_height<=0 || s.cell_count_xz<0 || s.height<0 || (!arena && capacity)) return;
    count_y_=s.height/s.cell_height;
    min_cell_y_=floor_div(s.min_y,s.cell_height);
    first_cell_x_=floor_div(s.first_block_x,s.cell_width);
    first_cell_z_=floor_div(s.first_block_z,s.cell_width);
    first_quart_x_=floor_div(s.first_block_x,4); first_quart_z_=floor_div(s.first_block_z,4);
    const int64_t horizontal=int64_t(s.cell_count_xz)*s.cell_width;
    if (horizontal>INT32_MAX || count_y_==INT32_MAX || s.cell_count_xz==INT32_MAX) return;
    quart_size_=int32_t(horizontal)/4;
    if (!multiply_size(size_t(s.cell_count_xz)+1,size_t(count_y_)+1,slice_values_) ||
        slice_values_>std::numeric_limits<size_t>::max()/2 ||
        !multiply_size(size_t(quart_size_)+1,size_t(quart_size_)+1,flat_values_) ||
        !multiply_size(size_t(s.cell_width),size_t(s.cell_width),cell_values_) ||
        !multiply_size(cell_values_,size_t(s.cell_height),cell_values_) ||
        cell_values_>size_t(INT32_MAX) || cell_values_>std::numeric_limits<size_t>::max()/sizeof(double) ||
        slice_values_*2>std::numeric_limits<size_t>::max()/sizeof(double) ||
        flat_values_>std::numeric_limits<size_t>::max()/sizeof(double)) return;
    status_=NoiseChunkResult::ok;
}
size_t NoiseChunk::required_values(NoiseCacheKind kind) const noexcept {
    if (status_!=NoiseChunkResult::ok) return 0;
    switch (kind) {
    case NoiseCacheKind::interpolated: return slice_values_*2;
    case NoiseCacheKind::flat: return flat_values_;
    case NoiseCacheKind::cell: return cell_values_;
    case NoiseCacheKind::once: return cell_values_>size_t(count_y_)+1?cell_values_:size_t(count_y_)+1;
    case NoiseCacheKind::column: return 0;
    }
    return 0;
}
NoiseChunkResult NoiseChunk::wrap(NoiseCacheKind kind, NoiseChunkFunction child, double* buffer,
                                 size_t capacity, NoiseCache*& cache, bool initialize_flat) noexcept {
    cache=nullptr;
    if (status_!=NoiseChunkResult::ok) return status_;
    if (uint8_t(kind)>uint8_t(NoiseCacheKind::cell) || !child.sample || !child.fill) return NoiseChunkResult::invalid_settings;
    if (count_==capacity_) return NoiseChunkResult::full;
    const size_t need=required_values(kind);
    if (capacity<need || (need && !buffer)) return NoiseChunkResult::workspace_full;
    NoiseCache& entry=caches_[count_]; entry=NoiseCache{};
    entry.child=child; entry.owner=this; entry.buffer=buffer; entry.capacity=capacity; entry.kind=kind;
    entry.position_key=chunk_pos(1875066,1875066);
    if (need) std::memset(buffer,0,need*sizeof(double));
    if (kind==NoiseCacheKind::interpolated) { entry.slices[0]=buffer; entry.slices[1]=buffer+slice_values_; }
    if (kind==NoiseCacheKind::flat && initialize_flat) {
        const size_t side=size_t(quart_size_)+1;
        for (int32_t x=0;x<=quart_size_;++x) for (int32_t z=0;z<=quart_size_;++z) {
            const NoiseChunkContext c{{product(sum(first_quart_x_,x),4),0,product(sum(first_quart_z_,z),4)},nullptr};
            const auto result=sample(child,c,buffer[size_t(x)*side+size_t(z)]);
            if (result!=NoiseChunkResult::ok) return result;
        }
    }
    ++count_; cache=&entry; return NoiseChunkResult::ok;
}
DensityContext NoiseChunk::position() const noexcept {
    return {sum(state_.start_x,state_.in_x),sum(state_.start_y,state_.in_y),sum(state_.start_z,state_.in_z),this};
}
NoiseChunkResult NoiseCache::compute(void* opaque, const NoiseChunkContext& context, double& value) noexcept {
    auto& c=*static_cast<NoiseCache*>(opaque); auto& chunk=*c.owner; const auto& s=chunk.state_;
    const auto point=context.position();
    if (c.kind==NoiseCacheKind::column) {
        const uint64_t key=chunk_pos(point.x,point.z);
        if (key!=c.position_key) {
            c.position_key=key;
            const auto r=sample(c.child,context,c.values[14]); if (r!=NoiseChunkResult::ok) return r;
        }
        value=c.values[14]; return NoiseChunkResult::ok;
    }
    if (c.kind==NoiseCacheKind::flat) {
        const int32_t x=difference(floor_div(point.x,4),chunk.first_quart_x_);
        const int32_t z=difference(floor_div(point.z,4),chunk.first_quart_z_);
        if (x>=0 && z>=0 && x<=chunk.quart_size_ && z<=chunk.quart_size_) {
            value=c.buffer[size_t(x)*(size_t(chunk.quart_size_)+1)+size_t(z)]; return NoiseChunkResult::ok;
        }
        return sample(c.child,context,value);
    }
    if (context.owner!=&chunk) return sample(c.child,context,value);
    if (c.kind==NoiseCacheKind::once) {
        if (c.array_valid && c.array_counter==s.array_counter) {
            if (s.array_index<0 || size_t(s.array_index)>=c.array_length) return NoiseChunkResult::invalid_index;
            value=c.buffer[size_t(s.array_index)]; return NoiseChunkResult::ok;
        }
        if (c.sample_counter!=s.sample_counter) {
            c.sample_counter=s.sample_counter;
            const auto r=sample(c.child,context,c.values[14]); if (r!=NoiseChunkResult::ok) return r;
        }
        value=c.values[14]; return NoiseChunkResult::ok;
    }
    if (!s.interpolating) return NoiseChunkResult::inactive;
    if (c.kind==NoiseCacheKind::cell) {
        const auto& g=chunk.settings_;
        if (s.in_x>=0 && s.in_x<g.cell_width && s.in_y>=0 && s.in_y<g.cell_height && s.in_z>=0 && s.in_z<g.cell_width) {
            const size_t index=(size_t(g.cell_height-1-s.in_y)*size_t(g.cell_width)+size_t(s.in_x))*size_t(g.cell_width)+size_t(s.in_z);
            value=c.buffer[index]; return NoiseChunkResult::ok;
        }
        return sample(c.child,context,value);
    }
    if (!s.filling_cell) { value=c.values[14]; return NoiseChunkResult::ok; }
    const double x=double(s.in_x)/chunk.settings_.cell_width, y=double(s.in_y)/chunk.settings_.cell_height;
    const double z=double(s.in_z)/chunk.settings_.cell_width;
    // Direct Mth.lerp3 path: X then Y then Z. The block traversal uses Y/X/Z.
    const double low=lerp(y,lerp(x,c.values[0],c.values[2]),lerp(x,c.values[4],c.values[6]));
    const double high=lerp(y,lerp(x,c.values[1],c.values[3]),lerp(x,c.values[5],c.values[7]));
    value=lerp(z,low,high); return NoiseChunkResult::ok;
}
NoiseChunkResult NoiseCache::fill_array(void* opaque, double* out, size_t count, NoiseChunkProvider& provider) noexcept {
    auto& c=*static_cast<NoiseCache*>(opaque);
    if (count && !out) return NoiseChunkResult::workspace_full;
    if (c.kind==NoiseCacheKind::once) {
        if (c.array_valid && c.array_counter==c.owner->state_.array_counter) {
            if (count>c.array_length) return NoiseChunkResult::invalid_index;
            if (count) std::memmove(out,c.buffer,count*sizeof(double));
            return NoiseChunkResult::ok;
        }
        if (count>c.capacity) return NoiseChunkResult::workspace_full;
        const auto r=fill(c.child,out,count,provider); if (r!=NoiseChunkResult::ok) return r;
        if (count) std::memmove(c.buffer,out,count*sizeof(double));
        c.array_length=count; c.array_valid=true; c.array_counter=c.owner->state_.array_counter;
        return NoiseChunkResult::ok;
    }
    if (c.kind==NoiseCacheKind::column || (c.kind==NoiseCacheKind::interpolated && !c.owner->state_.filling_cell))
        return fill(c.child,out,count,provider);
    return provider.fill_direct(out,count,c.function());
}
NoiseChunkResult NoiseChunk::slice_at(void* opaque, int32_t index, NoiseChunkContext& context) noexcept {
    auto& c=*static_cast<NoiseChunk*>(opaque);
    if (c.status_!=NoiseChunkResult::ok) return c.status_;
    c.state_.start_y=product(sum(index,c.min_cell_y_),c.settings_.cell_height);
    ++c.state_.sample_counter; c.state_.in_y=0; c.state_.array_index=index; context=c.context(); return NoiseChunkResult::ok;
}
NoiseChunkResult NoiseChunk::cell_at(void* opaque, int32_t index, NoiseChunkContext& context) noexcept {
    auto& c=*static_cast<NoiseChunk*>(opaque); const int32_t width=c.settings_.cell_width;
    if (c.status_!=NoiseChunkResult::ok) return c.status_;
    const int32_t plane=floor_div(index,width);
    c.state_.in_z=floor_mod(index,width); c.state_.in_x=floor_mod(plane,width);
    c.state_.in_y=difference(c.settings_.cell_height-1,floor_div(plane,width));
    c.state_.array_index=index; context=c.context(); return NoiseChunkResult::ok;
}
NoiseChunkResult NoiseChunk::slice_direct(void* opaque, double* out, size_t size, NoiseChunkFunction f) noexcept {
    auto& c=*static_cast<NoiseChunk*>(opaque);
    if (c.status_!=NoiseChunkResult::ok) return c.status_;
    if (!out || size<size_t(c.count_y_)+1) return NoiseChunkResult::workspace_full;
    for (int32_t i=0;i<=c.count_y_;++i) {
        NoiseChunkContext context; slice_at(&c,i,context);
        const auto r=sample(f,context,out[size_t(i)]); if (r!=NoiseChunkResult::ok) return r;
    }
    return NoiseChunkResult::ok;
}
NoiseChunkResult NoiseChunk::cell_direct(void* opaque, double* out, size_t size, NoiseChunkFunction f) noexcept {
    auto& c=*static_cast<NoiseChunk*>(opaque);
    if (c.status_!=NoiseChunkResult::ok) return c.status_;
    if (!out || size<c.cell_values_) return NoiseChunkResult::workspace_full;
    c.state_.array_index=0;
    for (int32_t y=c.settings_.cell_height-1;y>=0;--y) {
        c.state_.in_y=y;
        for (int32_t x=0;x<c.settings_.cell_width;++x) {
            c.state_.in_x=x;
            for (int32_t z=0;z<c.settings_.cell_width;++z) {
                c.state_.in_z=z;
                // Java's array[index++] evaluates the increment before compute.
                const size_t i=size_t(c.state_.array_index++);
                const auto r=sample(f,c.context(),out[i]); if (r!=NoiseChunkResult::ok) return r;
            }
        }
    }
    return NoiseChunkResult::ok;
}
NoiseChunkResult NoiseChunk::fill_slice(unsigned slot, int32_t cell_x) noexcept {
    state_.start_x=product(cell_x,settings_.cell_width); state_.in_x=0;
    for (int32_t z=0;z<=settings_.cell_count_xz;++z) {
        state_.start_z=product(sum(first_cell_z_,z),settings_.cell_width); state_.in_z=0; ++state_.array_counter;
        for (size_t i=0;i<count_;++i) if (caches_[i].kind==NoiseCacheKind::interpolated) {
            auto& c=caches_[i]; double* row=c.slices[slot]+size_t(z)*(size_t(count_y_)+1);
            const auto r=fill(c.function(),row,size_t(count_y_)+1,slice_provider_); if (r!=NoiseChunkResult::ok) return r;
        }
    }
    ++state_.array_counter; return NoiseChunkResult::ok;
}
NoiseChunkResult NoiseChunk::initialize_first_x() noexcept {
    if (status_!=NoiseChunkResult::ok) return status_;
    if (state_.interpolating) return NoiseChunkResult::inactive;
    state_.interpolating=true; state_.sample_counter=0; return fill_slice(0,first_cell_x_);
}
NoiseChunkResult NoiseChunk::advance_x(int32_t cell_x) noexcept {
    if (status_!=NoiseChunkResult::ok) return status_;
    const auto r=fill_slice(1,sum(sum(first_cell_x_,cell_x),1));
    state_.start_x=product(sum(first_cell_x_,cell_x),settings_.cell_width); return r;
}
NoiseChunkResult NoiseChunk::select_yz(int32_t y, int32_t z) noexcept {
    if (status_!=NoiseChunkResult::ok) return status_;
    if (y<0 || y>=count_y_ || z<0 || z>=settings_.cell_count_xz) return NoiseChunkResult::invalid_index;
    const size_t stride=size_t(count_y_)+1, base=size_t(z)*stride+size_t(y);
    for (size_t i=0;i<count_;++i) if (caches_[i].kind==NoiseCacheKind::interpolated) {
        auto& c=caches_[i];
        for (unsigned x=0;x<2;++x) for (unsigned yy=0;yy<2;++yy) for (unsigned zz=0;zz<2;++zz)
            c.values[x*2+yy*4+zz]=c.slices[x][base+yy+zz*stride];
    }
    state_.filling_cell=true; state_.start_y=product(sum(y,min_cell_y_),settings_.cell_height);
    state_.start_z=product(sum(first_cell_z_,z),settings_.cell_width); ++state_.array_counter;
    for (size_t i=0;i<count_;++i) if (caches_[i].kind==NoiseCacheKind::cell) {
        const auto r=fill(caches_[i].child,caches_[i].buffer,cell_values_,cell_provider_);
        if (r!=NoiseChunkResult::ok) return r;
    }
    ++state_.array_counter; state_.filling_cell=false; return NoiseChunkResult::ok;
}
void NoiseChunk::update_y(int32_t block_y, double fraction) noexcept {
    state_.in_y=difference(block_y,state_.start_y);
    for (size_t i=0;i<count_;++i) if (caches_[i].kind==NoiseCacheKind::interpolated) {
        auto& v=caches_[i].values;
        v[8]=lerp(fraction,v[0],v[4]); v[9]=lerp(fraction,v[2],v[6]);
        v[10]=lerp(fraction,v[1],v[5]); v[11]=lerp(fraction,v[3],v[7]);
    }
}
void NoiseChunk::update_x(int32_t block_x, double fraction) noexcept {
    state_.in_x=difference(block_x,state_.start_x);
    for (size_t i=0;i<count_;++i) if (caches_[i].kind==NoiseCacheKind::interpolated) {
        auto& v=caches_[i].values; v[12]=lerp(fraction,v[8],v[9]); v[13]=lerp(fraction,v[10],v[11]);
    }
}
void NoiseChunk::update_z(int32_t block_z, double fraction) noexcept {
    state_.in_z=difference(block_z,state_.start_z); ++state_.sample_counter;
    for (size_t i=0;i<count_;++i) if (caches_[i].kind==NoiseCacheKind::interpolated) {
        auto& v=caches_[i].values; v[14]=lerp(fraction,v[12],v[13]);
    }
}
NoiseChunkResult NoiseChunk::stop() noexcept {
    if (!state_.interpolating) return NoiseChunkResult::inactive;
    state_.interpolating=false; return NoiseChunkResult::ok;
}
void NoiseChunk::swap_slices() noexcept {
    for (size_t i=0;i<count_;++i) if (caches_[i].kind==NoiseCacheKind::interpolated) {
        auto& c=caches_[i]; double* first=c.slices[0]; c.slices[0]=c.slices[1]; c.slices[1]=first;
    }
}
NoiseChunkFunction NoiseChunkDensityField::function() noexcept {
    const DensityNode* node=graph?graph->node(root):nullptr;
    limits.minimum=node?node->minimum:0; limits.maximum=node?node->maximum:0;
    return {this,compute,fill_array,&limits};
}
NoiseChunkResult NoiseChunkDensityField::compute(void* opaque, const NoiseChunkContext& context, double& value) noexcept {
    auto& field=*static_cast<NoiseChunkDensityField*>(opaque);
    if (!field.graph || !field.graph->node(field.root)) return NoiseChunkResult::invalid_settings;
    if (!field.frames) return NoiseChunkResult::workspace_full;
    const auto r=field.graph->sample(field.root,context.position(),field.frames,field.frame_capacity,value);
    return chunk_result(r);
}
NoiseChunkResult NoiseChunkDensityField::fill_array(void* opaque, double* values, size_t count, NoiseChunkProvider& provider) noexcept {
    auto& f=*static_cast<NoiseChunkDensityField*>(opaque);
    if (!f.graph || !f.graph->node(f.root)) return NoiseChunkResult::invalid_settings;
    BatchProviderBridge bridge{&provider}; DensityBatchProvider p{&bridge,BatchProviderBridge::at,BatchProviderBridge::direct};
    return chunk_result(f.graph->fill(f.root,values,count,p,f.frames,f.frame_capacity,
        f.batch_frames?f.batch_frames:&f.inline_frame,f.batch_frames?f.batch_capacity:1,f.temporary,f.temporary_capacity));
}
void NoiseChunkDensityInput::bind(DensityInput& input) const noexcept {
    input.state=this; input.sample=nullptr; input.minimum=source.minimum(); input.maximum=source.maximum();
    input.checked_sample=compute; input.fill=fill_array;
}
DensityResult NoiseChunkDensityInput::compute(const void* opaque, DensityContext context, double& value) noexcept {
    const auto& input=*static_cast<const NoiseChunkDensityInput*>(opaque);
    return density_result(sample(input.source,{context,context.owner},value));
}
DensityResult NoiseChunkDensityInput::fill_array(const void* opaque, double* values, size_t count, DensityBatchProvider& provider) noexcept {
    const auto& input=*static_cast<const NoiseChunkDensityInput*>(opaque);
    ChunkProviderBridge bridge{&provider}; NoiseChunkProvider p{&bridge,ChunkProviderBridge::at,ChunkProviderBridge::direct};
    return density_result(fill(input.source,values,count,p));
}
}
