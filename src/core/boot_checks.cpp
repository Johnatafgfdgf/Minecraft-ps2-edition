#include "mcps2/boot_checks.hpp"
#include "mcps2/random.hpp"
#include "mcps2/improved_noise.hpp"
#include "mcps2/octave_noise.hpp"
#include "mcps2/blended_noise.hpp"
#include "mcps2/density_graph.hpp"
#include "mcps2/worldgen_noise.hpp"
#include "mcps2/simplex_noise.hpp"
#include "mcps2/java_float.hpp"
#include <cstring>

namespace mcps2 {
namespace {
uint32_t bits(float value) { uint32_t v; std::memcpy(&v, &value, sizeof(v)); return v; }
uint64_t bits(double value) { uint64_t v; std::memcpy(&v, &value, sizeof(v)); return v; }
template<class R> bool random_probe(uint32_t integer, uint64_t wide, uint32_t single, uint64_t real, bool boolean) {
    R source(0);
    const uint32_t a = uint32_t(source.next_int());
    const uint64_t b = source.next_long();
    int32_t bounded = -1;
    const bool accepted = source.next_int(1, bounded);
    const uint32_t c = bits(source.next_float());
    const uint64_t d = bits(source.next_double());
    const bool e = source.next_boolean();
    return a == integer && b == wide && accepted && bounded == 0 && c == single && d == real && e == boolean;
}
}
uint32_t run_boot_checks() {
    uint32_t mask = 0;
    if (random_probe<LegacyRandom>(0xbb20b460u, 0xd4d951383d93cb7aULL, 0x3f232dc9u, 0x3fd3c77c08ce970aULL, false)) mask |= 1;
    if (random_probe<XoroshiroRandom>(0xf66f517eu, 0xccbc22d72e97c372ULL, 0x3defdf28u, 0x3fb986c1cae1d8f0ULL, true)) mask |= 2;
    if (random_probe<WorldgenRandom<LegacyRandom>>(0xbb20b460u, 0xd4d951383d93cb7aULL, 0x3f232dc9u, 0x3fd3c77c08ce970aULL, false)) mask |= 4;
    if (random_probe<WorldgenRandom<XoroshiroRandom>>(0x2a2ca488u, 0xccbc22d7404e64b8ULL, 0x3dcc3608u, 0x3fe836b86e2dba9aULL, true)) mask |= 8;
    LegacyRandom source(0);
    ImprovedNoise noise(source);
    const uint64_t after = source.next_long();
    const double x = signed32(8u * 0x9e3779b9u + 0x11221122u) / 32.0;
    const double y = (signed32(8u * 1664525u + 54321u) % 1024) / 8.0;
    const double z = signed32(8u * 0x7f4a7c15u + 0x13579bdfu) / 64.0;
    if (bits(noise.offset(0)) == 0x406764168ea6ca89ULL
        && bits(noise.offset(1)) == 0x404ec9e5b3672e14ULL
        && bits(noise.offset(2)) == 0x406465b93a78ef81ULL
        && after == 0xa15aa3684d0173abULL
        && bits(noise.sample(x, y, z)) == 0x3fa714623e0891ccULL
        && bits(noise.sample(x, y, z, 0.0625, 0.375)) == 0x3fb5e7ba609a9f82ULL) mask |= 16;
    const Seed128 hash = seed_from_java_string(u"minecraft:temperature");
    XoroshiroRandom parent(0);
    auto factory = parent.fork_positional();
    auto child = factory.from_hash(u"minecraft:temperature");
    const uint32_t integer = uint32_t(child.next_int());
    const uint64_t wide = child.next_long(), real = bits(child.next_double());
    int32_t bounded = 0;
    if (hash.low == 0x5c7e6b29735f0d7fULL && hash.high == 0xf7d86f1bbc734988ULL
        && integer == 0x5327bf6fu && wide == 0x7c1aade8e17af0c2ULL && real == 0x3fc382990206f5e0ULL
        && child.next_int(1073741825, bounded) && bounded == 375613209) mask |= 32;
    NoiseOctave first_storage[7], second_storage[7];
    const double amplitudes[7] = {1,1,1,1,1,1,1};
    LegacyRandom perlin_source(0); PerlinNoise perlin;
    if (perlin.initialize(perlin_source, -6, amplitudes, 7, first_storage, 7)
        && perlin_source.next_long() == 0x3d93cb799b3970beULL && bits(perlin.max_value()) == 0x4000000000000000ULL
        && bits(perlin.sample(-33554432.0,70.125,33554432.0)) == 0xbf9e9c02ea74c19eULL) mask |= 64;
    XoroshiroRandom normal_source(0); NormalNoise normal;
    if (normal.initialize(normal_source, -6, amplitudes, 7, first_storage, second_storage, 7)
        && normal_source.next_long() == 0x1986c1cae1d8f2c5ULL && bits(normal.max_value()) == 0x4017555555555555ULL
        && bits(normal.sample(-33554432.0,70.125,33554432.0)) == 0x3fe47725c748740bULL) mask |= 128;
    NoiseOctave blended_storage[BlendedNoise::required_octaves];
    const BlendedNoiseParameters parameters{0.25,0.125,80,160,8};
    LegacyRandom blended_source(0); BlendedNoise blended;
    if (blended.initialize(blended_source, parameters, blended_storage, BlendedNoise::required_octaves)
        && blended_source.next_long() == 0x4a03dc73d63057fbULL
        && bits(blended.min_value()) == 0xc055e34bc6a7ef9fULL && bits(blended.max_value()) == 0x4055e34bc6a7ef9fULL
        && bits(blended.sample(-30000000,0,30000000)) == 0x3fb597a58c0435eaULL) mask |= 256;
    DensityNode density_nodes[5]; DensityFrame density_frames[5]; DensityGraph graph(density_nodes, 5);
    const DensitySpec definitions[] = {
        {DensityOp::y_gradient,0,0,0,-64,320,-1,1}, {DensityOp::square},
        {DensityOp::constant,0,0,0,0,0,0.64}, {DensityOp::multiply,2,1}, {DensityOp::squeeze,3}
    };
    bool accepted = true; DensityId id = 0;
    for (const auto& definition : definitions) if (graph.append(definition, id) != DensityResult::ok) accepted = false;
    double density = 0;
    if (accepted && graph.sample(id, {0,24,0}, density_frames, 5, density) == DensityResult::ok
        && bits(density) == 0x3fb7f705a895a129ULL && bits(graph.node(id)->minimum) == 0
        && bits(graph.node(id)->maximum) == 0x3fd3c7ec4ba67feaULL) mask |= 512;
    WorldgenNoiseFactory legacy_world(0,true); NoiseOctave climate_storage[4]; NormalNoise climate;
    const int32_t climate_x = signed32(7u * 0x9e3779b9u + 0x11221122u) % 30000001;
    const int32_t climate_z = signed32(7u * 0x7f4a7c15u + 0x13579bdfu) % 30000001;
    if (legacy_world.normal(climate,"minecraft:temperature",0,nullptr,0,climate_storage,4)
        && bits(climate.max_value()) == 0x4011c71c71c71c71ULL
        && bits(climate.sample(double(climate_x)*0.25,0,double(climate_z)*0.25)) == 0x3f97868416c77b26ULL) mask |= 1024;
    LegacyRandom simplex_source(0); SimplexNoise simplex; simplex.initialize(simplex_source);
    if (simplex_source.next_long() == 0xa15aa3684d0173abULL
        && bits(simplex.sample(-0.5,0.5)) == 0x3fdfeeb5a7a3e693ULL
        && bits(simplex.sample(-0.5,-0.5)) == 0x3fd3a873cb3210beULL) mask |= 2048;
    EndIslandDensity islands; islands.initialize(0);
    if (bits(islands.height(-970,-486)) == 0x3fce7700u
        && bits(islands.sample(-970,-486)) == 0x3fd752aae0000000ULL) mask |= 4096;
    const DensitySplinePoint spline_points[] = {{-1,2,1},{1,-2,2}};
    const DensitySpline spline_binding{spline_points,2};
    DensityNode spline_nodes[4]; DensityFrame spline_frames[2];
    DensityGraph spline_graph(spline_nodes,4,nullptr,0,nullptr,0,&spline_binding,1);
    const DensitySpec spline_definitions[] = {
        {DensityOp::constant,0,0,0,0,0,0.5}, {DensityOp::constant,0,0,0,0,0,-0.0},
        {DensityOp::constant,0,0,0,0,0,0.25}, {DensityOp::spline,0}
    };
    accepted=true;
    for (const auto& definition:spline_definitions) if (spline_graph.append(definition,id)!=DensityResult::ok) accepted=false;
    if (accepted && spline_graph.sample(id,{0,0,0},spline_frames,2,density)==DensityResult::ok
        && bits(density)==0x3feec00000000000ULL && bits(spline_graph.node(id)->minimum)==0x8000000000000000ULL
        && bits(spline_graph.node(id)->maximum)==0x3ff5000000000000ULL) mask |= 8192;
    const float smallest=java_float::round(0x1p-149);
    if (bits(java_float::round(16777219.0))==0x4b800002u
        && bits(java_float::add(1.0f,0x1.8p-23f))==0x3f800002u
        && bits(java_float::divide(1.0f,10.0f))==0x3dcccccdu
        && bits(java_float::multiply(smallest,1.0f))==1u) mask |= 16384;
    return mask;
}
}
