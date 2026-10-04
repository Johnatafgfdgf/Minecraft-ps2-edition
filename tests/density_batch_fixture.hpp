#pragma once
#include "mcps2/density_graph.hpp"
#include "mcps2/java_bits.hpp"

// Authored provider used by both synthetic and vanilla bulk tests. Direct
// traversal intentionally differs from forIndex so their call order is visible.
struct DensityBatchFixture {
    uint64_t* trace;
    size_t indexed=0,direct_calls=0;
    void mix(uint64_t v) noexcept { *trace=(*trace*0x100000001b3ULL)^v; }
    static mcps2::DensityContext context(int32_t i) noexcept {
        const int32_t edges[]={-1024,-65,-64,-1,0,1,23,24,103,104,127,128,239,240,256,320};
        const uint32_t n=uint32_t(i);
        const int32_t y=i>=0 && i<16?edges[i]:(mcps2::signed32(n*1664525u+54321u)%2048)-64;
        return {mcps2::signed32(n*0x9e3779b9u+0x11221122u)%30000001,y,
                mcps2::signed32(n*0x7f4a7c15u+0x13579bdfu)%30000001};
    }
    static mcps2::DensityResult at(void* state,int32_t i,mcps2::DensityContext& c) noexcept {
        auto& p=*static_cast<DensityBatchFixture*>(state);p.mix(0xe100000000000000ULL|uint32_t(i));++p.indexed;
        c=context(i);return mcps2::DensityResult::ok;
    }
    static mcps2::DensityResult direct(void* state,double* out,size_t n,mcps2::DensitySampleFunction f) noexcept {
        auto& p=*static_cast<DensityBatchFixture*>(state);p.mix(0xe000000000000000ULL|n);++p.direct_calls;
        for (size_t i=0;i<n;++i) {
            p.mix(0xe200000000000000ULL|i);
            const auto r=f.sample(f.state,context(int32_t(i)),out[i]);if (r!=mcps2::DensityResult::ok) return r;
        }
        return mcps2::DensityResult::ok;
    }
    mcps2::DensityBatchProvider provider() noexcept { return {this,at,direct}; }
};
