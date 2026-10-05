#pragma once
#include "mcps2/density_graph.hpp"
#include "mcps2/blended_noise.hpp"
#include <string_view>

namespace mcps2 {
struct DensityResourceView {
    uint32_t kind = 0, amplitude_count = 0;
    int32_t first_octave = 0;
    std::string_view name;
    BlendedNoiseParameters blended{};
};
struct DensitySplineView { uint32_t coordinate = 0, first_point = 0, point_count = 0; };
// Read-only, explicitly little-endian MCDG v2/v3. Binds external bytes.
// v3 preserves function types needed by NoiseChunkGraph; v2 is point-only.
class DensityPack {
public:
    bool bind(const void* bytes, size_t size) noexcept;
    bool spec(uint32_t index, DensitySpec& result) const noexcept;
    bool resource(uint32_t index, DensityResourceView& result) const noexcept;
    bool amplitude(uint32_t resource, uint32_t index, double& result) const noexcept;
    bool spline(uint32_t index,DensitySplineView& result) const noexcept;
    bool point(uint32_t index,DensitySplinePoint& result) const noexcept;
    uint32_t node_count() const noexcept { return nodes_; }
    uint32_t resource_count() const noexcept { return resources_; }
    uint32_t root() const noexcept { return root_; }
    uint32_t spline_count() const noexcept { return splines_; }
    uint32_t point_count() const noexcept { return points_; }
    bool legacy() const noexcept { return legacy_; }
    bool supports_chunk_binding() const noexcept { return version_ == 3; }
private:
    uint32_t u32(size_t offset) const noexcept;
    double real(size_t offset) const noexcept;
    float single(size_t offset) const noexcept;
    const uint8_t* bytes_ = nullptr;
    uint32_t nodes_ = 0, resources_ = 0, root_ = 0, node_offset_ = 0, resource_offset_ = 0;
    bool legacy_ = false;
    uint32_t version_ = 0;
    uint32_t splines_ = 0, points_ = 0, spline_offset_ = 0, point_offset_ = 0;
};
}
