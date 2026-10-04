#include "mcps2/random.hpp"
#include "mcps2/position.hpp"
#include "mcps2/bit_storage.hpp"
#include "mcps2/tick_queue.hpp"
#include "mcps2/improved_noise.hpp"
#include "mcps2/octave_noise.hpp"
#include "mcps2/blended_noise.hpp"
#include "mcps2/density_graph.hpp"
#include "mcps2/simplex_noise.hpp"
#include "mcps2/java_float.hpp"
#include "mcps2/java_math.hpp"
#include "density_batch_fixture.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

namespace {
unsigned case_index = 0;
uint64_t parse_hex(const std::string& value) { return std::stoull(value, nullptr, 16); }
std::string hex(uint64_t value, unsigned width = 16) {
    std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(width) << value; return out.str();
}
void emit(const std::string& value) { std::cout << "R\t" << case_index << '\t' << value << '\n'; }
template<class R> void rng(uint64_t seed, unsigned count) {
    R source(seed); int32_t result = 0;
    emit(std::string("reject ") + (!source.next_int(0, result) ? '1' : '0'));
    emit(std::string("reject ") + (!source.next_int(-1, result) ? '1' : '0'));
    const int32_t bounds[] = {1,2,3,16,17,1000,1073741825,2147483647};
    for (unsigned i = 0; i < count; ++i) {
        if (i == 127) source.set_seed(seed ^ 0xfedcba9876543210ULL);
        std::string output;
        switch (i % 6) {
            case 0: output = "I " + hex(uint32_t(source.next_int()), 8); break;
            case 1: output = "L " + hex(source.next_long()); break;
            case 2:
                if (!source.next_int(bounds[(i / 6) % 8], result)) std::abort();
                output = "B " + std::to_string(result); break;
            case 3: {
                const float value = source.next_float(); uint32_t bits;
                std::memcpy(&bits, &value, sizeof(bits)); output = "F " + hex(bits, 8); break;
            }
            case 4: {
                const double value = source.next_double(); uint64_t bits;
                std::memcpy(&bits, &value, sizeof(bits)); output = "D " + hex(bits); break;
            }
            default: output = std::string("O ") + (source.next_boolean() ? '1' : '0');
        }
        emit("rng " + std::to_string(i) + " " + output);
    }
}
template<class R> void fork_rng(uint64_t seed, unsigned count) {
    R source(seed);
    for (unsigned i = 0; i < count; ++i) {
        auto child = source.fork();
        const uint64_t child_value = child.next_long(), parent_value = source.next_long();
        emit("fork " + hex(child_value) + " " + hex(parent_value));
    }
}
template<class R> void worldgen(uint64_t seed, int32_t x, int32_t z, int32_t index, int32_t step, int32_t salt) {
    mcps2::WorldgenRandom<R> random(seed);
    const uint64_t derived = random.decoration_seed(seed, x, z);
    emit("decoration " + hex(derived) + " " + hex(random.next_long()));
    random.feature_seed(seed, index, step); emit("feature " + hex(random.next_long()));
    random.large_feature_seed(seed, x, z); emit("large " + hex(random.next_long()));
    random.large_feature_with_salt(seed, x, z, salt);
    const uint64_t value = random.next_long();
    emit("salt " + hex(value) + " " + std::to_string(random.count()));
}
uint64_t double_bits(double value) { uint64_t bits; std::memcpy(&bits,&value,sizeof(bits)); return bits; }
double from_double_bits(const std::string& token) { const uint64_t bits=parse_hex(token); double value; std::memcpy(&value,&bits,sizeof(value)); return value; }
float from_float_bits(const std::string& token) { const uint32_t bits=uint32_t(parse_hex(token)); float value; std::memcpy(&value,&bits,sizeof(value)); return value; }
uint32_t float_bits(float value) { uint32_t bits; std::memcpy(&bits,&value,sizeof(bits)); return value!=value?0x7fc00000u:bits; }
float float_from_bits(uint32_t bits) { float value;std::memcpy(&value,&bits,sizeof(value));return value; }
void java_float_cases(uint32_t seed,unsigned samples) {
    const uint32_t edges[]={0,0x80000000u,1,0x80000001u,0x007fffffu,0x00800000u,0x3f000000u,0x3f800000u,
        0x3f800001u,0x40000000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00000u,0x7fa00001u};
    auto next=[&seed]() { seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed; };
    for (unsigned i=0;i<samples;++i) {
        const float a=float_from_bits(i<256?edges[i/16]:next()),b=float_from_bits(i<256?edges[i%16]:next());
        const uint64_t hi=next(),lo=next(),raw=hi<<32|lo;double d;std::memcpy(&d,&raw,sizeof(d));
        const int32_t integer=mcps2::signed32(next());
        const float values[]={mcps2::java_float::add(a,b),mcps2::java_float::subtract(a,b),mcps2::java_float::multiply(a,b),
            mcps2::java_float::divide(a,b),mcps2::java_float::remainder(a,b),mcps2::java_float::square_root(a),
            mcps2::java_float::round(d),mcps2::java_float::round(double(integer)),mcps2::java_minimum(a,b),mcps2::java_maximum(a,b)};
        std::string record="java-float "+std::to_string(i);
        for (float value:values) record+=" "+hex(float_bits(value),8);
        record+=" "+std::to_string(mcps2::java_float::equal(a,b))+" "+std::to_string(mcps2::java_float::less(a,b))+" "+std::to_string(mcps2::java_float::less_equal(a,b));
        emit(record);
    }
}
uint64_t density_bits(double value) { return value != value ? 0x7ff8000000000000ULL : double_bits(value); }
struct DensityProbe { uint64_t* trace; unsigned id; int32_t threshold; double low,high; };
double density_probe(const void* state,mcps2::DensityContext context) noexcept {
    const auto& p=*static_cast<const DensityProbe*>(state);
    *p.trace=(*p.trace*0x100000001b3ULL)^(p.id+1);
    return context.y<p.threshold?p.low:p.high;
}
void density(bool batch=false) {
    using namespace mcps2;
    unsigned count,root,samples;std::cin>>count>>root>>samples;
    std::vector<DensityNode> nodes(count);std::vector<DensityInput> inputs(count);
    std::vector<DensityProbe> probes(count);uint64_t trace=0;
    std::vector<DensitySpec> specs(count);unsigned spline_count=0;
    for (auto& s:specs) {
        unsigned op;std::string p0,p1;
        std::cin>>op>>s.a>>s.b>>s.c>>s.from_y>>s.to_y>>p0>>p1;
        s.op=DensityOp(op);s.p0=from_double_bits(p0);s.p1=from_double_bits(p1);
        if (s.op==DensityOp::spline) spline_count=std::max(spline_count,unsigned(s.from_y)+1);
    }
    std::vector<std::vector<DensitySplinePoint>> points(spline_count);std::vector<DensitySpline> splines(spline_count);
    if (spline_count) {
        unsigned encoded;std::cin>>encoded;if (encoded!=spline_count) std::abort();
        for (unsigned i=0;i<spline_count;++i) {
            unsigned size;std::cin>>size;points[i].resize(size);
            for (auto& p:points[i]) {
                std::string location,derivative;std::cin>>location>>derivative>>p.value;
                p.location=from_float_bits(location);p.derivative=from_float_bits(derivative);
            }
            splines[i]={points[i].data(),points[i].size()};
        }
    }
    // Bindings are installed before their corresponding node is appended; the arena never moves.
    DensityGraph graph(nodes.data(),count,inputs.data(),count,nullptr,0,splines.data(),splines.size());
    for (unsigned i=0;i<count;++i) {
        DensitySpec s=specs[i];
        if (s.op==DensityOp::input) {
            probes[i]={&trace,i,s.from_y,s.p0,s.p1};inputs[i]={&probes[i],density_probe,s.p0,s.p1};s.a=i;
        }
        DensityId id;
        if (graph.append(s,id)!=DensityResult::ok || id!=i) std::abort();
        const auto* n=graph.node(id);
        emit("density-bounds "+std::to_string(i)+" "+hex(density_bits(n->minimum))+" "+hex(density_bits(n->maximum)));
    }
    std::vector<DensityFrame> frames(graph.required_frames(root));
    if (batch) {
        std::vector<DensityBatchFrame> bulk(frames.size());
        std::vector<double> temporary(graph.required_temporary_arrays(root)*samples),out(samples);
        trace=0;DensityBatchFixture fixture{&trace};auto provider=fixture.provider();
        if (graph.fill(root,out.data(),samples,provider,frames.data(),frames.size(),bulk.data(),bulk.size(),temporary.data(),temporary.size())!=DensityResult::ok) std::abort();
        for (unsigned i=0;i<samples;++i) emit("density-batch "+std::to_string(i)+" "+hex(density_bits(out[i])));
        emit("density-batch-trace "+hex(trace)+" "+std::to_string(fixture.indexed)+" "+std::to_string(fixture.direct_calls));return;
    }
    const int32_t edges[]={-1024,-65,-64,-1,0,1,23,24,103,104,127,128,239,240,256,320};
    for (unsigned i=0;i<samples;++i) {
        const int32_t y=i<16?edges[i]:(mcps2::signed32(i*1664525u+54321u)%2048)-64;
        const DensityContext context{mcps2::signed32(i*0x9e3779b9u+0x11221122u)%30000001,y,mcps2::signed32(i*0x7f4a7c15u+0x13579bdfu)%30000001};
        double value;trace=0;
        if (graph.sample(root,context,frames.data(),frames.size(),value)!=DensityResult::ok) std::abort();
        emit("density "+std::to_string(i)+" "+hex(density_bits(value))+" "+hex(trace));
    }
}
template<class R> void simplex(uint64_t seed,unsigned samples) {
    R source(seed);mcps2::SimplexNoise field;field.initialize(source);
    emit("simplex-offsets "+hex(double_bits(field.offset(0)))+" "+hex(double_bits(field.offset(1)))+" "+hex(double_bits(field.offset(2))));
    emit("simplex-parent "+hex(source.next_long()));
    if constexpr (!std::is_same_v<R,mcps2::LegacyRandom> && !std::is_same_v<R,mcps2::XoroshiroRandom>)
        emit("simplex-count "+std::to_string(source.count()));
    const double edges[]={-2147483648.25,-33554432,-256,-1,-0.5,-0.0,0,0.5,1,256,33554432,2147483648.25};
    for (unsigned i=0;i<samples;++i) {
        const double x=i<12?edges[i]:i<40?(double(i)-32)/16:mcps2::signed32(i*0x9e3779b9u+0x11221122u)/64.0;
        const double y=i<12?edges[11-i]:i<40?0:mcps2::signed32(i*0x7f4a7c15u+0x13579bdfu)/128.0;
        emit("simplex2 "+std::to_string(i)+" "+hex(density_bits(field.sample(x,y)))+" "+hex(density_bits(field.sample(x,x))));
    }
}
void end_islands(uint64_t seed,unsigned samples) {
    mcps2::EndIslandDensity field;field.initialize(seed);
    emit("end-bounds "+hex(double_bits(field.minimum))+" "+hex(double_bits(field.maximum)));
    for (unsigned i=0;i<samples;++i) {
        const int32_t x=i<256?(int32_t(i)-128)*8+int32_t(i%8):mcps2::signed32(i*0x9e3779b9u+0x11221122u)%(i<512?32769:30000001);
        const int32_t z=i<256?(int32_t((i*97)%257)-128)*8-int32_t(i%8):mcps2::signed32(i*0x7f4a7c15u+0x13579bdfu)%(i<512?32769:30000001);
        emit("end "+std::to_string(i)+" "+hex(float_bits(field.height(x,z)),8)+" "+hex(density_bits(field.sample(x,z))));
    }
}
template<class R> void blended(uint64_t seed,unsigned samples,const mcps2::BlendedNoiseParameters& parameters) {
    mcps2::NoiseOctave storage[mcps2::BlendedNoise::required_octaves];
    R source(seed);mcps2::BlendedNoise field;
    if (!field.initialize(source,parameters,storage,mcps2::BlendedNoise::required_octaves)) std::abort();
    emit("blended-parent "+hex(source.next_long()));
    if constexpr (!std::is_same_v<R,mcps2::LegacyRandom> && !std::is_same_v<R,mcps2::XoroshiroRandom>)
        emit("blended-count "+std::to_string(source.count()));
    emit("blended-bounds "+hex(double_bits(field.min_value()))+" "+hex(double_bits(field.max_value())));
    const int32_t edges[]={-30000000,-64,-1,0,1,319,4096,30000000};
    for (unsigned i=0;i<samples;++i) {
        if (i==128) {
            R next(seed^0xfedcba9876543210ULL);
            if (!field.reseed(next,storage,mcps2::BlendedNoise::required_octaves)) std::abort();
            emit("blended-reseed "+hex(next.next_long()));
        }
        const int32_t x=i<8?edges[i]:mcps2::signed32(i*0x9e3779b9u+0x11221122u)%30000001;
        const int32_t y=i<8?edges[(i+3)%8]:(mcps2::signed32(i*1664525u+54321u)%1024)-64;
        const int32_t z=i<8?-x:mcps2::signed32(i*0x7f4a7c15u+0x13579bdfu)%30000001;
        emit("blended "+std::to_string(i)+" "+hex(double_bits(field.sample(x,y,z))));
    }
}
template<class R> void octaves(uint64_t seed,const std::string& mode,int32_t first,const std::vector<double>& amplitudes,unsigned samples) {
    const size_t count=amplitudes.size();
    std::vector<mcps2::NoiseOctave> first_storage(count),second_storage(count);
    R source(seed); mcps2::PerlinNoise perlin; mcps2::NormalNoise normal;
    const bool is_normal=mode=="normal"||mode=="normal-legacy",modern=mode=="normal"||mode=="modern";
    const bool accepted=is_normal ? normal.initialize(source,first,amplitudes.data(),count,first_storage.data(),second_storage.data(),count,modern)
        : perlin.initialize(source,first,amplitudes.data(),count,first_storage.data(),count,modern);
    emit(std::string("octaves-accepted ")+(accepted?'1':'0'));
    emit("octaves-parent "+hex(source.next_long()));
    if constexpr (!std::is_same_v<R,mcps2::LegacyRandom> && !std::is_same_v<R,mcps2::XoroshiroRandom>)
        emit("octaves-count "+std::to_string(source.count()));
    if (!accepted) return;
    emit("octaves-max "+hex(double_bits(is_normal?normal.max_value():perlin.max_value())));
    if (!is_normal) {
        emit("octaves-broken "+hex(double_bits(perlin.max_broken_value(0.375))));
        for (size_t i=0;i<count;++i) {
            const auto* field=perlin.octave(i);
            if (!field) { emit("octave "+std::to_string(i)+" null"); continue; }
            std::string line="octave "+std::to_string(i);
            for (unsigned axis=0;axis<3;++axis) line+=" "+hex(double_bits(field->offset(axis)));
            emit(line);
        }
    }
    const double edges[]={-33554432,-16777216,-1,-0.5,0,0.5,16777216,33554432};
    for (unsigned i=0;i<samples;++i) {
        const double x=i<8?edges[i]:mcps2::signed32(i*0x9e3779b9u+0x11221122u)/64.0;
        const double y=(mcps2::signed32(i*1664525u+54321u)%2048)/8.0-64.0;
        const double z=i<8?-x:mcps2::signed32(i*0x7f4a7c15u+0x13579bdfu)/64.0;
        emit("octaves3 "+std::to_string(i)+" "+hex(double_bits(is_normal?normal.sample(x,y,z):perlin.sample(x,y,z))));
        if (!is_normal) emit("octaves6 "+std::to_string(i)+" "+hex(double_bits(perlin.sample(x,y,z,(i%7)/16.0,i%3?(i%5)/8.0:-1.0,i%2!=0))));
    }
}
template<class R> void factory_draw(const std::string& label, R& child) {
    const uint32_t a=uint32_t(child.next_int());
    const uint64_t b=child.next_long(), c=double_bits(child.next_double());
    int32_t d=0; if (!child.next_int(1073741825,d)) std::abort();
    emit(label+" "+hex(a,8)+" "+hex(b)+" "+hex(c)+" "+std::to_string(d));
}
template<class R> void factory(uint64_t seed, const std::string& token, bool text_case) {
    R source(seed); const auto factory=source.fork_positional();
    emit("factory-parent "+hex(source.next_long()));
    if constexpr (!std::is_same_v<R,mcps2::LegacyRandom> && !std::is_same_v<R,mcps2::XoroshiroRandom>)
        emit("factory-count "+std::to_string(source.count()));
    if (text_case) {
        std::u16string text;
        if (token!="-") for (size_t i=0;i<token.size();i+=4) text.push_back(char16_t(std::stoul(token.substr(i,4),nullptr,16)));
        const auto hash=mcps2::seed_from_java_string(text);
        emit("hash "+hex(hash.low)+" "+hex(hash.high));
        auto child=factory.from_hash(text); factory_draw("factory-hash",child); return;
    }
    const unsigned count=unsigned(std::stoul(token));
    for (unsigned i=0;i<count;++i) {
        int32_t x=mcps2::signed32(i*0x9e3779b9u+0x11221122u), y=mcps2::signed32(i*1664525u+54321u), z=mcps2::signed32(i*0x7f4a7c15u+0x13579bdfu);
        if (i<4) { x=mcps2::signed32(0x80000000u+i); y=-64+int32_t(i); z=2147483647-int32_t(i); }
        emit("position-seed "+hex(mcps2::positional_seed(x,y,z)));
        auto at=factory.at(x,y,z); factory_draw("factory-at",at);
        auto from_seed=factory.from_seed(seed^mcps2::sign_extend32(i*0x9e3779b9u)); factory_draw("factory-seed",from_seed);
    }
}
template<class R> void noise(uint64_t seed, unsigned count) {
    R source(seed); mcps2::ImprovedNoise field(source);
    emit("offsets " + hex(double_bits(field.offset(0))) + " " + hex(double_bits(field.offset(1))) + " " + hex(double_bits(field.offset(2))));
    emit("noise-rng " + hex(source.next_long()));
    for (unsigned i=0;i<count;++i) {
        double x,y,z;
        if (i<8) { x=-field.offset(0)+(int(i)-4)/8.0; y=-field.offset(1)+(i%3)/4.0; z=-field.offset(2)+(i%5)/8.0; }
        else {
            x=mcps2::signed32(i*0x9e3779b9u+0x11221122u)/32.0;
            y=(mcps2::signed32(i*1664525u+54321u)%1024)/8.0;
            z=mcps2::signed32(i*0x7f4a7c15u+0x13579bdfu)/64.0;
        }
        emit("noise3 " + std::to_string(i) + " " + hex(double_bits(field.sample(x,y,z))));
        const double scale=(i%7)/16.0, limit=i%3 ? (i%5)/8.0 : -1.0;
        emit("noise5 " + std::to_string(i) + " " + hex(double_bits(field.sample(x,y,z,scale,limit))));
    }
}
}

int main() {
    using namespace mcps2;
    std::string command;
    while (std::cin >> command) {
        if (command == "rng" || command == "fork") {
            std::string variant, seed; unsigned count; std::cin >> variant >> seed >> count;
            const uint64_t value = parse_hex(seed);
            if (command == "fork") {
                if (variant == "legacy") fork_rng<LegacyRandom>(value, count);
                else fork_rng<XoroshiroRandom>(value, count);
            } else if (variant == "legacy") rng<LegacyRandom>(value, count);
            else if (variant == "xoroshiro") rng<XoroshiroRandom>(value, count);
            else if (variant == "worldgen-legacy") rng<WorldgenRandom<LegacyRandom>>(value, count);
            else if (variant == "worldgen-xoroshiro") rng<WorldgenRandom<XoroshiroRandom>>(value, count);
            else return 2;
        } else if (command=="java-float") {
            std::string seed;unsigned count;std::cin>>seed>>count;java_float_cases(uint32_t(parse_hex(seed)),count);
        } else if (command=="simplex") {
            std::string variant,seed;unsigned count;std::cin>>variant>>seed>>count;
            if (variant=="legacy") simplex<LegacyRandom>(parse_hex(seed),count);
            else if (variant=="xoroshiro") simplex<XoroshiroRandom>(parse_hex(seed),count);
            else if (variant=="worldgen-legacy") simplex<WorldgenRandom<LegacyRandom>>(parse_hex(seed),count);
            else if (variant=="worldgen-xoroshiro") simplex<WorldgenRandom<XoroshiroRandom>>(parse_hex(seed),count);
            else return 2;
        } else if (command=="end") {
            std::string seed;unsigned count;std::cin>>seed>>count;end_islands(parse_hex(seed),count);
        } else if (command=="density") {
            density();
        } else if (command=="density-batch") {
            density(true);
        } else if (command=="blended") {
            std::string variant,seed;unsigned samples;std::cin>>variant>>seed>>samples;
            double values[5];for (double& value:values) { std::string token;std::cin>>token;value=from_double_bits(token); }
            const BlendedNoiseParameters parameters{values[0],values[1],values[2],values[3],values[4]};
            if (variant=="legacy") blended<LegacyRandom>(parse_hex(seed),samples,parameters);
            else if (variant=="xoroshiro") blended<XoroshiroRandom>(parse_hex(seed),samples,parameters);
            else if (variant=="worldgen-legacy") blended<WorldgenRandom<LegacyRandom>>(parse_hex(seed),samples,parameters);
            else if (variant=="worldgen-xoroshiro") blended<WorldgenRandom<XoroshiroRandom>>(parse_hex(seed),samples,parameters);
            else return 2;
        } else if (command=="octaves") {
            std::string variant,seed,mode; int32_t first; size_t count; unsigned samples;
            std::cin>>variant>>seed>>mode>>first>>count>>samples;
            std::vector<double> amplitudes(count);
            for (auto& a:amplitudes) { std::string token;std::cin>>token;a=from_double_bits(token); }
            if (variant=="legacy") octaves<LegacyRandom>(parse_hex(seed),mode,first,amplitudes,samples);
            else if (variant=="xoroshiro") octaves<XoroshiroRandom>(parse_hex(seed),mode,first,amplitudes,samples);
            else if (variant=="worldgen-legacy") octaves<WorldgenRandom<LegacyRandom>>(parse_hex(seed),mode,first,amplitudes,samples);
            else if (variant=="worldgen-xoroshiro") octaves<WorldgenRandom<XoroshiroRandom>>(parse_hex(seed),mode,first,amplitudes,samples);
            else return 2;
        } else if (command=="wrap") {
            std::string token;std::cin>>token;emit("wrap "+hex(double_bits(PerlinNoise::wrap(from_double_bits(token)))));
        } else if (command=="factory" || command=="factory-hash") {
            std::string variant,seed,token; std::cin>>variant>>seed>>token;
            if (variant=="legacy") factory<LegacyRandom>(parse_hex(seed),token,command=="factory-hash");
            else if (variant=="xoroshiro") factory<XoroshiroRandom>(parse_hex(seed),token,command=="factory-hash");
            else if (variant=="worldgen-legacy") factory<WorldgenRandom<LegacyRandom>>(parse_hex(seed),token,command=="factory-hash");
            else if (variant=="worldgen-xoroshiro") factory<WorldgenRandom<XoroshiroRandom>>(parse_hex(seed),token,command=="factory-hash");
            else return 2;
        } else if (command == "zero") {
            unsigned count; std::cin >> count; XoroshiroRandom source(0, 0);
            for (unsigned i=0;i<count;++i) emit("zero " + hex(source.next_long()));
        } else if (command == "noise") {
            std::string variant,seed;unsigned count;std::cin>>variant>>seed>>count;
            if (variant=="legacy") noise<LegacyRandom>(parse_hex(seed),count);
            else if (variant=="xoroshiro") noise<XoroshiroRandom>(parse_hex(seed),count);
            else if (variant=="worldgen-legacy") noise<WorldgenRandom<LegacyRandom>>(parse_hex(seed),count);
            else if (variant=="worldgen-xoroshiro") noise<WorldgenRandom<XoroshiroRandom>>(parse_hex(seed),count);
            else return 2;
        } else if (command == "mix") {
            std::string seed; std::cin >> seed; emit("mix " + hex(mix_stafford13(parse_hex(seed))));
        } else if (command == "pos") {
            int32_t x,y,z; std::cin >> x >> y >> z;
            const uint64_t bp=BlockPos{x,y,z}.pack(), sp=SectionPos{x,y,z}.pack();
            const auto block=BlockPos::unpack(bp); const auto section=SectionPos::unpack(sp);
            emit("block " + hex(bp) + " " + std::to_string(block.x) + " " + std::to_string(block.y) + " " + std::to_string(block.z));
            emit("section " + hex(sp) + " " + std::to_string(section.x) + " " + std::to_string(section.y) + " " + std::to_string(section.z));
            emit("local " + std::to_string(block_to_section(x)) + " " + std::to_string(section_relative({x,y,z})));
            emit("chunk " + hex(chunk_pos(x,z)));
        } else if (command == "worldgen") {
            std::string variant, seed; int32_t x,z,index,step,salt;
            std::cin >> variant >> seed >> x >> z >> index >> step >> salt;
            if (variant == "legacy") worldgen<LegacyRandom>(parse_hex(seed),x,z,index,step,salt);
            else worldgen<XoroshiroRandom>(parse_hex(seed),x,z,index,step,salt);
        } else if (command == "slime") {
            std::string seed,salt; int32_t x,z; std::cin >> seed >> x >> z >> salt;
            auto random=slime_chunk_random(x,z,parse_hex(seed),parse_hex(salt)); int32_t value;
            if (!random.next_int(10,value)) return 3;
            emit("slime " + std::to_string(value) + " " + hex(random.next_long()));
        } else if (command == "storage") {
            unsigned bits; uint32_t entries; std::string seed_text; std::cin >> bits >> entries >> seed_text;
            const uint32_t seed=uint32_t(parse_hex(seed_text));
            const uint32_t mask=bits==32 ? 0x7fffffffu : uint32_t((uint64_t(1)<<bits)-1);
            std::vector<uint64_t> words(BitStorage::required_words(bits,entries)); BitStorage data;
            if (!data.bind(bits,entries,words.data(),words.size())) return 3;
            for (uint32_t i=0;i<entries;++i) if (!data.set(i,(i*0x9e3779b9u+seed)&mask)) return 3;
            uint64_t checksum=0;
            for (int64_t i=int64_t(entries)-1;i>=0;i-=3) {
                uint32_t previous=0;
                if (!data.get_and_set(uint32_t(i),((uint32_t(i)*1664525u+seed)^0x5a5a5a5au)&mask,previous)) return 3;
                checksum ^= uint64_t(previous)*(uint64_t(i)+1);
            }
            emit("previous " + hex(checksum)); checksum=0;
            for (uint32_t i=0;i<entries;++i) {
                uint32_t value=0; if (!data.get(i,value)) return 3;
                checksum=checksum*0x100000001b3ULL ^ value;
            }
            emit("read " + hex(checksum));
            for (size_t i=0;i<words.size();++i) emit("word " + std::to_string(i) + " " + hex(words[i]));
        } else if (command == "ticks") {
            std::string seed_text; unsigned count; std::cin >> seed_text >> count;
            uint32_t seed=uint32_t(parse_hex(seed_text)); std::vector<ScheduledTick> memory(count);
            ChunkTickQueue queue(memory.data(),count); unsigned inserted=0;
            for (unsigned i=0;i<count;++i) {
                seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5;
                const int32_t logical=int32_t(i%97);
                ScheduledTick tick{BlockPos{logical,-64,logical/7}.pack(),int64_t(seed%80)-20,int64_t(i),i%3,int8_t(int(seed%7)-3)};
                const auto result=queue.schedule(tick);
                if (result==ScheduleResult::inserted) ++inserted;
                else if (result!=ScheduleResult::duplicate) return 3;
            }
            emit("inserted " + std::to_string(inserted)); ScheduledTick tick;
            while (queue.poll(tick)) emit("tick " + hex(tick.position) + " " + std::to_string(tick.trigger_tick)
                + " " + std::to_string(tick.priority) + " " + std::to_string(tick.sub_tick_order) + " " + std::to_string(tick.type));
        } else return 2;
        if (!std::cin) return 2;
        ++case_index;
    }
}
