#pragma once
#include "mcps2/noise_chunk_graph.hpp"
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>

// Host-only storage/test driver. The runtime binder allocates nothing and the
// PS2 boot probe uses fixed arenas instead of these vectors.
struct NoiseGraphStorage {
    std::vector<mcps2::DensityNode> nodes;
    std::vector<mcps2::DensityInput> inputs;
    std::vector<mcps2::NoiseGraphCache> caches;
    std::vector<mcps2::DensityId> memo;
    std::vector<mcps2::NoiseGraphVisit> visits;
    std::vector<mcps2::NoiseGraphKey> keys;
    std::vector<uint32_t> slots;
    std::vector<mcps2::DensitySpline> splines;
    std::vector<mcps2::DensitySplinePoint> points;
    explicit NoiseGraphStorage(const mcps2::DensityGraph& source):nodes(source.size()),inputs(source.size()+source.input_count()),
        caches(source.size()),memo(source.size()),visits(source.size()),keys(source.size()),splines(source.spline_count()) {
        size_t n=1;while (n<2*source.size()) n*=2;slots.resize(n);
        n=0;for (size_t i=0;i<source.spline_count();++i) n+=source.splines()[i].count;points.resize(n);
    }
    mcps2::NoiseGraphArena arena() noexcept {
        return {nodes.data(),nodes.size(),inputs.data(),inputs.size(),caches.data(),caches.size(),memo.data(),memo.size(),
            visits.data(),visits.size(),keys.data(),keys.size(),slots.data(),slots.size(),splines.data(),splines.size(),points.data(),points.size()};
    }
};
inline uint64_t graph_bits(double value) {
    if (value!=value) return 0x7ff8000000000000ULL;
    uint64_t result;std::memcpy(&result,&value,8);return result;
}
inline std::string graph_hex(uint64_t value) {
    std::ostringstream s;s<<std::hex<<std::setw(16)<<std::setfill('0')<<value;return s.str();
}
template<class Emit> void observe_noise_graph(const mcps2::DensityGraph& source,mcps2::DensityId root,unsigned layout,uint64_t& trace,Emit emit) {
    using namespace mcps2;
    const int32_t xs[]={-32,12800,-30000000},zs[]={64,-9280,29999968};
    NoiseChunkSettings settings{-64,16,4,8,1,xs[layout%3],zs[layout%3]};
    std::vector<NoiseCache> caches(source.size());NoiseChunk chunk(settings,caches.data(),caches.size());
    NoiseGraphStorage storage(source);NoiseChunkGraph binder(source,chunk,storage.arena());
    const size_t length=4*4*8;
    if (binder.prepare(root,length+1)!=DensityResult::ok) std::abort();
    const auto& r=binder.requirements();
    std::vector<double> values(r.values),temporary(r.temporary);
    std::vector<DensityFrame> samples(r.sample_frames);std::vector<DensityBatchFrame> batches(r.batch_frames);
    if (binder.activate({values.data(),values.size(),samples.data(),samples.size(),batches.data(),batches.size(),temporary.data(),temporary.size()})!=NoiseChunkResult::ok) std::abort();
    const auto f=binder.function();
    auto state=[&](const std::string& label) {
        const auto& s=chunk.state();std::string line="chunk-state "+label;
        for (int32_t i:{s.start_x,s.start_y,s.start_z,s.in_x,s.in_y,s.in_z,s.array_index}) line+=" "+std::to_string(i);
        emit(line+" "+graph_hex(s.sample_counter)+" "+graph_hex(s.array_counter)+" "+std::to_string(s.interpolating)+" "+std::to_string(s.filling_cell)+" "+graph_hex(trace)+" 0 0");
    };
    auto value=[&](const std::string& label,NoiseChunkContext context) {
        double v=0;const auto result=f.sample(f.state,context,v);
        if (result!=NoiseChunkResult::ok && result!=NoiseChunkResult::inactive && result!=NoiseChunkResult::invalid_index) std::abort();
        emit("graph-value "+label+" "+std::to_string(uint8_t(result))+":"+graph_hex(result==NoiseChunkResult::ok?graph_bits(v):0)+" "+graph_hex(trace));
    };
    auto array=[&](const std::string& label,size_t size) {
        std::vector<double> out(size);auto& p=chunk.cell_provider();const auto result=f.fill(f.state,out.data(),size,p);
        if (result!=NoiseChunkResult::ok && result!=NoiseChunkResult::inactive && result!=NoiseChunkResult::invalid_index) std::abort();
        uint64_t hash=0;for (double v:out) hash=(hash*0x100000001b3ULL)^graph_bits(v);
        emit("graph-array "+label+" "+std::to_string(uint8_t(result))+" "+graph_hex(hash)+" "+graph_hex(trace));
        state(label);value(label+"-owner",chunk.context());
    };
    auto operation=[&](const std::string& label,NoiseChunkResult result) { emit("chunk-operation "+label+" "+std::to_string(uint8_t(result))); };
    std::string counts="graph-caches";
    for (unsigned k=0;k<5;++k) { size_t n=0;for (size_t i=0;i<chunk.cache_count();++i) if (uint8_t(caches[i].kind)==k) ++n;counts+=" "+std::to_string(n); }
    emit(counts);emit("graph-bounds "+graph_hex(graph_bits(f.minimum()))+" "+graph_hex(graph_bits(f.maximum())));
    state("construct");value("inactive",chunk.context());
    const int32_t positions[][3]={{settings.first_block_x,settings.min_y,settings.first_block_z},
        {settings.first_block_x+4,0,settings.first_block_z+4},{settings.first_block_x-1,127,settings.first_block_z-1},
        {1875066,9,1875066},{INT32_MIN,-1,INT32_MAX}};
    for (const auto& p:positions) value("external",{{p[0],p[1],p[2]},nullptr});
    array("before",length);operation("initialize",chunk.initialize_first_x());state("initialize");
    operation("advance",chunk.advance_x(0));state("advance");
    for (int cy=1;cy>=0;--cy) {
        operation("select",chunk.select_yz(cy,0));state("select-"+std::to_string(cy));
        const auto s=chunk.state();
        for (int y=7;y>=0;--y) {
            chunk.update_y(s.start_y+y,y/8.0);
            for (int x=0;x<4;++x) {
                chunk.update_x(s.start_x+x,x/4.0);
                for (int z=0;z<4;++z) {
                    chunk.update_z(s.start_z+z,z/4.0);
                    value("point-"+std::to_string(cy)+"-"+std::to_string(x)+"-"+std::to_string(y)+"-"+std::to_string(z),chunk.context());value("repeat",chunk.context());
                }
            }
        }
        array("cell",length);array("long",length+1);array("long-repeat",length+1);
    }
    chunk.swap_slices();operation("swap",NoiseChunkResult::ok);state("swap");
    operation("stop",chunk.stop());state("stop");value("stopped",chunk.context());
    operation("restart",chunk.initialize_first_x());state("restart");operation("restop",chunk.stop());
}
