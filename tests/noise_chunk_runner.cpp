#include "mcps2/noise_chunk.hpp"
#include "mcps2/improved_noise.hpp"
#include "mcps2/random.hpp"
#include "mcps2/java_bits.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace mcps2;
unsigned case_index=0;
std::string hex(uint64_t value) { std::ostringstream out; out<<std::hex<<std::setfill('0')<<std::setw(16)<<value;return out.str(); }
uint64_t bits(double value) { uint64_t result;std::memcpy(&result,&value,8);return value!=value?0x7ff8000000000000ULL:result; }
double number(uint64_t value) { double result;std::memcpy(&result,&value,8);return result; }
void emit(const std::string& value) { std::cout<<"R\t"<<case_index<<"\t"<<value<<'\n'; }
void require(NoiseChunkResult result) { if (result!=NoiseChunkResult::ok) std::abort(); }
int32_t add(int32_t x,int32_t y) { return signed32(uint32_t(x)+uint32_t(y)); }
struct Test;
struct Leaf {
    Test* test=nullptr;unsigned id=0;LegacyRandom source;ImprovedNoise noise;
    NoiseChunkLimits limits{-1e12,1e12};
    Leaf(Test* t,unsigned i,uint64_t seed) : test(t),id(i),source(seed^(i?0x9e3779b97f4a7c15ULL:0)),noise(source) {}
    static NoiseChunkResult sample(void*,const NoiseChunkContext&,double&) noexcept;
    static NoiseChunkResult fill(void*,double*,size_t,NoiseChunkProvider&) noexcept;
    NoiseChunkFunction function() { return {this,sample,fill,&limits}; }
};
struct Test {
    NoiseChunkSettings settings;
    std::array<NoiseCache,10> arena;
    NoiseChunk chunk;
    std::array<std::vector<double>,10> buffers;
    std::array<NoiseChunkFunction,10> functions;
    Leaf leaves[2];
    DensityNode gradient_node[1];DensityFrame gradient_frame[1];
    DensityGraph gradient{gradient_node,1};NoiseChunkDensityField field{&gradient,0,gradient_frame,1};
    unsigned mode,style,cycles;
    uint64_t trace=0,samples=0,fills=0;
    Test(NoiseChunkSettings s,unsigned m,unsigned f,unsigned c,uint64_t seed)
        : settings(s),chunk(s,arena.data(),arena.size()),leaves{{this,0,seed},{this,1,seed}},mode(m),style(f),cycles(c) {
        require(chunk.status());
        DensityId id; if (gradient.append({DensityOp::y_gradient,0,0,0,-65,104,-1.3,2.7},id)!=DensityResult::ok) std::abort();
        wrap(0,NoiseCacheKind::once,leaves[0].function());wrap(1,NoiseCacheKind::column,leaves[1].function());
        wrap(2,NoiseCacheKind::flat,functions[0]);wrap(3,NoiseCacheKind::interpolated,functions[0]);
        wrap(4,NoiseCacheKind::interpolated,functions[1]);wrap(5,NoiseCacheKind::once,functions[3]);
        wrap(6,NoiseCacheKind::cell,functions[5]);wrap(7,NoiseCacheKind::cell,functions[5]);
        wrap(8,NoiseCacheKind::once,functions[5]);wrap(9,NoiseCacheKind::flat,functions[1],false);
    }
    void mix(uint64_t value) { trace=(trace*0x100000001b3ULL)^value; }
    void wrap(unsigned i,NoiseCacheKind kind,NoiseChunkFunction child,bool initialize=true) {
        buffers[i].resize(chunk.required_values(kind));NoiseCache* cache;
        require(chunk.wrap(kind,child,buffers[i].data(),buffers[i].size(),cache,initialize));functions[i]=cache->function();
    }
    std::string tail() { return " "+hex(trace)+" "+std::to_string(samples)+" "+std::to_string(fills); }
    void state(const std::string& label) {
        const auto& s=chunk.state(); std::string record="chunk-state "+label;
        for (int32_t value:{s.start_x,s.start_y,s.start_z,s.in_x,s.in_y,s.in_z,s.array_index}) record+=" "+std::to_string(value);
        emit(record+" "+hex(s.sample_counter)+" "+hex(s.array_counter)+" "+std::to_string(s.interpolating)+" "+std::to_string(s.filling_cell)+tail());
    }
    std::string sample(NoiseChunkFunction f,NoiseChunkContext context) {
        double value=0;const auto result=f.sample(f.state,context,value);
        if (unsigned(result)>2) std::abort();
        return std::to_string(unsigned(result))+":"+hex(result==NoiseChunkResult::ok?bits(value):0);
    }
    void values(const std::string& label,NoiseChunkContext context) {
        std::string record="chunk-values "+label;
        for (auto f:functions) record+=" "+sample(f,context);
        record+=" "+sample(functions[5],context);record+=" "+sample(functions[8],context);emit(record+tail());
    }
    void operation(const std::string& label,NoiseChunkResult result) { emit("chunk-operation "+label+" "+std::to_string(unsigned(result))); }
    void fill_array(const std::string& label,NoiseChunkFunction f,size_t length) {
        std::vector<double> out(length);const auto result=f.fill(f.state,out.data(),out.size(),chunk.cell_provider());
        if (unsigned(result)>2) std::abort();
        uint64_t hash=0;for (double value:out) hash=(hash*0x100000001b3ULL)^bits(value);
        emit("chunk-array "+label+" "+std::to_string(unsigned(result))+" "+hex(hash)+tail());
    }
    void execute() {
        state("construct");
        for (size_t i=0;i<functions.size();++i) emit("chunk-bounds "+std::to_string(i)+" "+hex(bits(functions[i].minimum()))+" "+hex(bits(functions[i].maximum())));
        const auto sentinel=sample(functions[1],{{1875066,9,1875066},nullptr});emit("chunk-sentinel "+sentinel+tail());
        values("inactive",chunk.context());
        const int32_t extent=settings.cell_count_xz*settings.cell_width;
        const DensityContext edges[]={{settings.first_block_x,settings.min_y,settings.first_block_z},
            {settings.first_block_x,add(settings.min_y,settings.cell_height),settings.first_block_z},
            {add(settings.first_block_x,-1),settings.min_y,add(settings.first_block_z,-1)},
            {add(settings.first_block_x,extent+4),settings.min_y,add(settings.first_block_z,extent+4)},{INT32_MIN,-1,INT32_MAX}};
        for (unsigned i=0;i<5;++i) values("external-"+std::to_string(i),{edges[i],nullptr});
        const int32_t width=settings.cell_width,height=settings.cell_height;
        const int32_t length=width*width*height;
        fill_array("once-first",functions[0],size_t(length));
        const auto owner=sample(functions[0],chunk.context());emit("chunk-array-owner "+owner+tail());
        fill_array("once-short",functions[0],3);fill_array("once-long",functions[0],size_t(length)+1);
        for (int32_t index:{-width*width,-1,0,length-1,length}) {
            NoiseChunkContext context;require(chunk.cell_provider().for_index(index,context));state("index-"+std::to_string(index));
        }
        for (unsigned cycle=0;cycle<cycles;++cycle) {
            operation("initialize",chunk.initialize_first_x());state("initialize-"+std::to_string(cycle));
            operation("initialize-twice",chunk.initialize_first_x());
            for (int32_t cx=0;cx<settings.cell_count_xz;++cx) {
                operation("advance",chunk.advance_x(cx));state("advance-"+std::to_string(cycle)+"-"+std::to_string(cx));
                for (int32_t cz=0;cz<settings.cell_count_xz;++cz) for (int32_t cy=chunk.cell_count_y()-1;cy>=0;--cy) {
                    operation("select",chunk.select_yz(cy,cz));
                    const std::string label=std::to_string(cycle)+"-"+std::to_string(cx)+"-"+std::to_string(cy)+"-"+std::to_string(cz);
                    state("select-"+label);
                    const auto s=chunk.state();
                    for (int32_t y=height-1;y>=0;--y) {
                        chunk.update_y(add(s.start_y,y),double(y)/height);
                        for (int32_t x=0;x<width;++x) {
                            chunk.update_x(add(s.start_x,x),double(x)/width);
                            for (int32_t z=0;z<width;++z) {
                                chunk.update_z(add(s.start_z,z),double(z)/width);
                                values("point-"+label+"-"+std::to_string(x)+"-"+std::to_string(y)+"-"+std::to_string(z),chunk.context());
                            }
                        }
                    }
                    chunk.update_y(add(s.start_y,-1),-0.125);chunk.update_x(add(s.start_x,width),1.125);chunk.update_z(add(s.start_z,width),1.25);
                    values("fallback",chunk.context());
                }
                chunk.swap_slices();operation("swap",NoiseChunkResult::ok);state("swap");
            }
            fill_array("flat-bulk",functions[2],size_t(length));fill_array("cell-bulk",functions[6],size_t(length));
            fill_array("interpolated-bulk",functions[3],size_t(length));fill_array("cell-bulk-repeat",functions[6],size_t(length));
            values("bulk-owner",chunk.context());state("bulk");
            operation("stop",chunk.stop());operation("stop-twice",chunk.stop());values("stopped",chunk.context());
        }
    }
};
NoiseChunkResult Leaf::sample(void* state,const NoiseChunkContext& c,double& value) noexcept {
    auto& leaf=*static_cast<Leaf*>(state);auto& t=*leaf.test;const auto p=c.position();
    t.mix(0x10+leaf.id);t.mix(uint32_t(p.x));t.mix(uint32_t(p.y));t.mix(uint32_t(p.z));
    if (c.owner==&t.chunk) {
        const auto& s=t.chunk.state();t.mix(s.sample_counter);t.mix(s.array_counter);t.mix(uint32_t(s.array_index));t.mix(s.filling_cell?1:0);
    } else t.mix(UINT64_MAX);
    ++t.samples;
    if (t.mode==1) {
        const uint64_t edges[]={0,0x8000000000000000ULL,0x3ff0000000000000ULL,0xbff0000000000000ULL,
            0x3fb999999999999aULL,0x7ff0000000000000ULL,0xfff0000000000000ULL,0x7ff8000000000000ULL};
        value=number(edges[((uint32_t(p.x)>>2)+(uint32_t(p.y)>>2)+(uint32_t(p.z)>>2)+leaf.id)&7]);return NoiseChunkResult::ok;
    }
    if (t.mode==2 && leaf.id==0) { auto f=t.field.function();return f.sample(f.state,c,value); }
    value=leaf.noise.sample(double(p.x)*0.03125,double(p.y)*0.0625,double(p.z)*0.015625)+double(leaf.id)*0.125;
    return NoiseChunkResult::ok;
}
NoiseChunkResult Leaf::fill(void* state,double* out,size_t length,NoiseChunkProvider& provider) noexcept {
    auto& leaf=*static_cast<Leaf*>(state);auto& t=*leaf.test;
    t.mix(0x100+leaf.id);t.mix(length);t.mix(t.chunk.state().array_counter);++t.fills;
    if (t.style==0) return provider.fill_direct(out,length,leaf.function());
    for (size_t i=0;i<length;++i) {
        NoiseChunkContext c;auto result=provider.for_index(int32_t(i),c);
        if (result!=NoiseChunkResult::ok) return result;
        result=sample(&leaf,c,out[i]);if (result!=NoiseChunkResult::ok) return result;
    }
    return NoiseChunkResult::ok;
}
}
int main() {
    std::string line;
    while (std::getline(std::cin,line)) {
        std::istringstream input(line);std::string command,seed;NoiseChunkSettings s;unsigned mode,style,cycles;
        if (!(input>>command>>s.cell_width>>s.cell_height>>s.min_y>>s.height>>s.cell_count_xz>>s.first_block_x>>s.first_block_z>>mode>>style>>cycles>>seed) || command!="chunk") return 2;
        Test test(s,mode,style,cycles,std::stoull(seed,nullptr,16));test.execute();++case_index;
    }
}
