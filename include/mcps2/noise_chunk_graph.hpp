#pragma once
#include "mcps2/noise_chunk.hpp"

namespace mcps2 {
struct NoiseGraphVisit { DensityId node = 0; size_t child = 0; };
struct NoiseGraphKey {
    DensitySpec spec{};
    double minimum = 0, maximum = 0;
    uint32_t origin = 0;
    DensityId result = 0;
};
struct NoiseGraphCache {
    NoiseChunkDensityField field{};
    NoiseChunkDensityInput input{};
    NoiseCacheKind kind = NoiseCacheKind::column;
    size_t values = 0;
};
// Planning storage, owned by the caller. Capacity N suffices for nodes, keys,
// memo and DFS frames for a source with N nodes. Slots need a power of two >=2N;
// inputs need source.input_count()+N, caches need N, splines/points mirror the
// source. Buffers must not overlap; every address stays stable after prepare.
struct NoiseGraphArena {
    DensityNode* nodes = nullptr; size_t node_capacity = 0;
    DensityInput* inputs = nullptr; size_t input_capacity = 0;
    NoiseGraphCache* caches = nullptr; size_t cache_capacity = 0;
    DensityId* memo = nullptr; size_t memo_capacity = 0;
    NoiseGraphVisit* visits = nullptr; size_t visit_capacity = 0;
    NoiseGraphKey* keys = nullptr; size_t key_capacity = 0;
    uint32_t* slots = nullptr; size_t slot_capacity = 0;
    DensitySpline* splines = nullptr; size_t spline_capacity = 0;
    DensitySplinePoint* points = nullptr; size_t point_capacity = 0;
};
struct NoiseGraphRequirements {
    size_t caches = 0, values = 0, sample_frames = 0, batch_frames = 0, temporary = 0;
};
struct NoiseGraphWorkspace {
    double* values = nullptr; size_t value_capacity = 0;
    DensityFrame* samples = nullptr; size_t sample_capacity = 0;
    DensityBatchFrame* batches = nullptr; size_t batch_capacity = 0;
    double* temporary = nullptr; size_t temporary_capacity = 0;
};

// Native equivalent of DensityFunction.mapAll(NoiseChunk.wrap). The current
// domain is EmptyBlender and an empty Beardifier, matching the density core.
// Source graphs must retain types/identities (MCDG v3, not ambiguous v2 packs).
// prepare performs no evaluation or chunk mutation; activate preflights all
// runtime storage before constructing FlatCache in original DFS order. Each
// nested cache has separate evaluation stacks; no heap allocation occurs.
class NoiseChunkGraph {
public:
    NoiseChunkGraph(const DensityGraph& source, NoiseChunk& chunk, NoiseGraphArena arena) noexcept;
    NoiseChunkGraph(const NoiseChunkGraph&) = delete;
    NoiseChunkGraph& operator=(const NoiseChunkGraph&) = delete;
    DensityResult prepare(DensityId root, size_t largest_array) noexcept;
    NoiseChunkResult activate(NoiseGraphWorkspace workspace) noexcept;
    const NoiseGraphRequirements& requirements() const noexcept { return requirements_; }
    const DensityGraph& graph() const noexcept { return graph_; }
    DensityId root() const noexcept { return root_field_.root; }
    DensityId mapped(DensityId source) const noexcept;
    NoiseChunkFunction function() noexcept { return active_ ? root_field_.function() : NoiseChunkFunction{}; }
private:
    const DensityGraph& source_;
    NoiseChunk& chunk_;
    NoiseGraphArena arena_;
    DensityGraph graph_;
    NoiseChunkDensityField root_field_{};
    NoiseGraphRequirements requirements_{};
    size_t keys_ = 0, caches_ = 0, largest_array_ = 0;
    bool attempted_ = false, prepared_ = false, active_ = false, failed_ = false;
    size_t child_count(DensityId node) const noexcept;
    DensityId child_at(DensityId node, size_t child) const noexcept;
    size_t slot(const NoiseGraphKey& key) const noexcept;
    DensityResult transform(DensityId source, DensityId& result) noexcept;
    bool add_field_requirements(const NoiseChunkDensityField& field) noexcept;
};
static_assert(sizeof(NoiseGraphKey)<=64,"NoiseChunk visitor key budget");
}
