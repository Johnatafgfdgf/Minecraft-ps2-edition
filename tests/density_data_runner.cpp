#include "mcps2/density_pack.hpp"
#include "mcps2/worldgen_noise.hpp"
#include "mcps2/simplex_noise.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>

namespace {
std::string hex(uint64_t value) {
    std::ostringstream s; s << std::hex << std::setfill('0') << std::setw(16) << value; return s.str();
}
uint64_t bits(double value) {
    if (value != value) return 0x7ff8000000000000ULL;
    uint64_t b; std::memcpy(&b,&value,sizeof(b)); return b;
}
unsigned case_index = 0;
void emit(const std::string& value) { std::cout << "R\t" << case_index << '\t' << value << '\n'; }
std::string decode_path(const std::string& text) {
    if (text.size()%2) std::abort();
    std::string result;
    for (size_t i=0;i<text.size();i+=2) result.push_back(char(std::stoul(text.substr(i,2),nullptr,16)));
    return result;
}
}
int main(int argc,char** argv) {
    using namespace mcps2;
    // Format rejection mode exercises the exact runtime reader on authored corrupt fixtures.
    if (argc==2) {
        std::ifstream file(argv[1],std::ios::binary);
        if (!file) return 2;
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}}; DensityPack pack;
        return pack.bind(bytes.data(),bytes.size())?0:3;
    }
    std::string command;
    while (std::cin>>command) {
        std::string setting,seed,path;unsigned field,samples;
        if (command!="density-data") return 2;
        std::cin>>setting>>field>>seed>>samples>>path;
        std::ifstream file(decode_path(path),std::ios::binary);
        if (!file || !std::cin) return 2;
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};DensityPack pack;
        if (!pack.bind(bytes.data(),bytes.size())) return 3;
        const uint64_t world_seed=std::stoull(seed,nullptr,16); WorldgenNoiseFactory factory(world_seed,pack.legacy());
        std::vector<NormalNoise> normals(pack.resource_count());std::vector<BlendedNoise> blended(pack.resource_count());
        std::vector<EndIslandDensity> islands(pack.resource_count());
        std::vector<std::vector<NoiseOctave>> pools(pack.resource_count());std::vector<DensityNoiseBinding> bindings(pack.resource_count());
        for (uint32_t i=0;i<pack.resource_count();++i) {
            DensityResourceView r;if (!pack.resource(i,r)) return 3;
            if (r.kind==2) { islands[i].initialize(world_seed);bindings[i].end=&islands[i];continue; }
            if (r.kind==1) {
                pools[i].resize(BlendedNoise::required_octaves);
                if (!factory.blended(blended[i],r.blended,pools[i].data(),pools[i].size())) return 3;
                bindings[i].blended=&blended[i];continue;
            }
            std::vector<double> amplitudes(r.amplitude_count);
            for (uint32_t j=0;j<r.amplitude_count;++j) if (!pack.amplitude(i,j,amplitudes[j])) return 3;
            size_t count=amplitudes.size();
            if (pack.legacy() && (r.name=="minecraft:temperature" || r.name=="minecraft:vegetation")) count=2;
            if (pack.legacy() && r.name=="minecraft:offset") count=1;
            pools[i].resize(count*2);
            if (!factory.normal(normals[i],r.name,r.first_octave,amplitudes.data(),amplitudes.size(),pools[i].data(),pools[i].size())) return 3;
            bindings[i].normal=&normals[i];
        }
        std::vector<DensitySplinePoint> points(pack.point_count());std::vector<DensitySpline> splines(pack.spline_count());
        for (uint32_t i=0;i<pack.point_count();++i) if (!pack.point(i,points[i])) return 3;
        for (uint32_t i=0;i<pack.spline_count();++i) {
            DensitySplineView s;if (!pack.spline(i,s)) return 3;
            splines[i]={points.data()+s.first_point,s.point_count};
        }
        std::vector<DensityNode> nodes(pack.node_count());DensityGraph graph(nodes.data(),nodes.size(),nullptr,0,bindings.data(),bindings.size(),splines.data(),splines.size());
        for (uint32_t i=0;i<pack.node_count();++i) {
            DensitySpec spec;DensityId id;
            if (!pack.spec(i,spec) || graph.append(spec,id)!=DensityResult::ok || id!=i) return 3;
        }
        const auto* root=graph.node(pack.root());
        emit("density-data-bounds "+hex(bits(root->minimum))+" "+hex(bits(root->maximum)));
        std::vector<DensityFrame> frames(graph.required_frames(pack.root()));
        const int32_t edges[]={-1024,-65,-64,-1,0,1,23,24,103,104,127,128,239,240,256,320};
        for (unsigned i=0;i<samples;++i) {
            const int32_t y=i<16?edges[i]:(signed32(i*1664525u+54321u)%2048)-64;
            const DensityContext context{signed32(i*0x9e3779b9u+0x11221122u)%30000001,y,signed32(i*0x7f4a7c15u+0x13579bdfu)%30000001};
            double value;
            if (graph.sample(pack.root(),context,frames.data(),frames.size(),value)!=DensityResult::ok) return 3;
            emit("density-data "+std::to_string(i)+" "+hex(bits(value)));
        }
        ++case_index;
    }
}
