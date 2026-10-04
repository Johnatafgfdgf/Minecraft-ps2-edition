#include "mcps2/density_graph.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
using namespace mcps2;
struct Probe {
    unsigned indexed=0,direct=0,samples=0;
    size_t extent=SIZE_MAX;
    bool fail=false;
    static DensityContext point(int32_t i) noexcept { return {i,i-2,-i}; }
    static DensityResult at(void* state,int32_t i,DensityContext& context) noexcept {
        auto& p=*static_cast<Probe*>(state);++p.indexed;context=point(i);return DensityResult::ok;
    }
    static DensityResult fill(void* state,double* out,size_t count,DensitySampleFunction f) noexcept {
        auto& p=*static_cast<Probe*>(state);++p.direct;
        for (size_t i=0;i<count && i<p.extent;++i) {
            const auto r=f.sample(f.state,point(int32_t(i)),out[i]);if (r!=DensityResult::ok) return r;
        }
        return DensityResult::ok;
    }
    static DensityResult sample(const void* state,DensityContext context,double& value) noexcept {
        auto& p=*static_cast<Probe*>(const_cast<void*>(state));++p.samples;
        if (p.fail) return DensityResult::inactive;
        value=context.y;return DensityResult::ok;
    }
    DensityBatchProvider provider() noexcept { return {this,at,fill}; }
    void reset() noexcept { indexed=direct=samples=0; }
};
uint64_t bits(double value) { uint64_t v;std::memcpy(&v,&value,8);return v; }
}
int main() {
    using namespace mcps2;
    Probe probe;auto provider=probe.provider();
    DensityInput input;input.state=&probe;input.minimum=-16;input.maximum=16;input.checked_sample=Probe::sample;
    DensityNode nodes[8];DensityGraph graph(nodes,8,&input,1);DensityId leaf,pair,right,left,constant,alpha,offset;
    assert(graph.append({DensityOp::input,0},leaf)==DensityResult::ok);
    assert(graph.append({DensityOp::add,leaf,leaf},pair)==DensityResult::ok);
    assert(graph.append({DensityOp::add,leaf,pair},right)==DensityResult::ok);
    assert(graph.append({DensityOp::add,pair,leaf},left)==DensityResult::ok);
    assert(graph.append({DensityOp::constant,0,0,0,0,0,-0.0},constant)==DensityResult::ok);
    assert(graph.append({DensityOp::blend_alpha},alpha)==DensityResult::ok);
    assert(graph.append({DensityOp::blend_offset},offset)==DensityResult::ok);
    assert(graph.required_temporary_arrays(right)==2 && graph.required_temporary_arrays(left)==1);
    DensityFrame frames[4];DensityBatchFrame batch[4];double out[10],temporary[18];
    for (double& v:out) v=91;
    for (double& v:temporary) v=92;
    auto run=[&](DensityId root,size_t count,size_t points,size_t bulk,size_t scratch,double* tmp= nullptr) {
        return graph.fill(root,out+1,count,provider,frames,points,batch,bulk,tmp?tmp:temporary+1,scratch);
    };
    assert(run(right,8,4,4,15)==DensityResult::workspace_full);
    assert(run(right,8,2,4,16)==DensityResult::workspace_full);
    assert(run(right,8,4,2,16)==DensityResult::workspace_full);
    assert(run(right,8,4,4,16,out+1)==DensityResult::invalid_operation);
    assert(probe.samples==0 && probe.direct==0 && probe.indexed==0);
    for (double v:out) assert(v==91);
    for (double v:temporary) assert(v==92);
    assert(run(right,8,4,4,16)==DensityResult::ok);
    assert(probe.samples==24 && probe.direct==3 && probe.indexed==0);
    for (int i=0;i<8;++i) assert(out[i+1]==3.0*(i-2));

    // A caller's existing tail survives first-child filling. The scratch tail
    // is +0, matching allocation of a Java double[] for the second child.
    probe.extent=3;
    for (double& v:out) v=91;
    for (double& v:temporary) v=92;
    assert(run(pair,8,4,4,8)==DensityResult::ok);
    for (int i=0;i<8;++i) assert(out[i+1]==(i<3?2.0*(i-2):91));
    probe.extent=SIZE_MAX;
    assert(out[0]==91 && out[9]==91 && temporary[0]==92 && temporary[17]==92);
    probe.reset();assert(run(left,8,4,4,8)==DensityResult::ok);
    assert(probe.samples==24 && probe.direct==3 && probe.indexed==0);
    for (int i=0;i<8;++i) assert(out[i+1]==3.0*(i-2));

    // Constant/empty-Blender paths do not dereference a provider. An empty
    // array still reaches each direct leaf, just as original fillArray does.
    DensityBatchProvider unused;
    for (DensityId root:{constant,alpha,offset}) {
        assert(graph.fill(root,out+1,8,unused,frames,4,batch,4,nullptr,0)==DensityResult::ok);
        for (int i=0;i<8;++i) assert(bits(out[i+1])==bits(root==constant?-0.0:root==alpha?1.0:0.0));
    }
    probe.reset();assert(graph.fill(right,nullptr,0,provider,nullptr,0,batch,4,nullptr,0)==DensityResult::ok);
    assert(probe.direct==3 && probe.samples==0 && probe.indexed==0);
    assert(graph.fill(99,out,1,provider,frames,4,batch,4,nullptr,0)==DensityResult::invalid_reference);
    assert(graph.fill(leaf,out,size_t(INT32_MAX)+1,provider,frames,4,batch,4,nullptr,0)==DensityResult::invalid_operation);
    double value=91;probe.fail=true;
    assert(graph.sample(leaf,{0,0,0},frames,4,value)==DensityResult::inactive && value==91);
    out[1]=91;assert(run(leaf,8,4,4,0)==DensityResult::inactive && out[1]==91);probe.fail=false;

    // A graph deeper than the native call stack budget evaluates in external
    // frames. Refusing a smaller arena leaves output and the provider intact.
    std::vector<DensityNode> deep_nodes(2049);DensityGraph deep(deep_nodes.data(),deep_nodes.size(),&input,1);
    DensityId root;assert(deep.append({DensityOp::input,0},root)==DensityResult::ok);
    for (size_t i=1;i<deep_nodes.size();++i) assert(deep.append({DensityOp::absolute,root},root)==DensityResult::ok);
    std::vector<DensityFrame> points(deep.required_frames(root));std::vector<DensityBatchFrame> bulk(points.size());
    probe.reset();out[1]=91;
    assert(deep.fill(root,out+1,8,provider,points.data(),points.size(),bulk.data(),bulk.size()-1,nullptr,0)==DensityResult::workspace_full);
    assert(out[1]==91 && probe.direct==0 && probe.samples==0);
    assert(deep.fill(root,out+1,8,provider,points.data(),points.size(),bulk.data(),bulk.size(),nullptr,0)==DensityResult::ok);
    for (int i=0;i<8;++i) assert(out[i+1]==(i<2?2-i:i-2));
    std::puts("Density batch resource tests passed: bounded scratch, guards, checked errors, empty arrays and iterative depth.");
}
