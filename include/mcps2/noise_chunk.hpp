#pragma once
#include "mcps2/density_graph.hpp"
#include <cstddef>
#include <cstdint>

namespace mcps2 {
class NoiseChunk;
struct NoiseCache;
enum class NoiseChunkResult : uint8_t { ok, inactive, invalid_index, invalid_settings, workspace_full, full };
enum class NoiseCacheKind : uint8_t { interpolated, flat, column, once, cell };
struct NoiseChunkSettings {
    int32_t min_y = 0, height = 0, cell_width = 4, cell_height = 8;
    int32_t cell_count_xz = 4, first_block_x = 0, first_block_z = 0;
};
struct NoiseChunkContext {
    DensityContext point{0,0,0};
    const NoiseChunk* owner = nullptr;
    DensityContext position() const noexcept;
};
struct NoiseChunkProvider;
struct NoiseChunkLimits { double minimum = 0, maximum = 0; };
// Functions and their state must outlive the chunk. fill() is deliberately a
// separate entry point: replacing bulk evaluation by repeated sample() changes
// CacheOnce counters and leaf invocation order in the original.
struct NoiseChunkFunction {
    void* state = nullptr;
    NoiseChunkResult (*sample)(void*, const NoiseChunkContext&, double&) noexcept = nullptr;
    NoiseChunkResult (*fill)(void*, double*, size_t, NoiseChunkProvider&) noexcept = nullptr;
    const NoiseChunkLimits* limits = nullptr;
    double minimum() const noexcept { return limits?limits->minimum:0; }
    double maximum() const noexcept { return limits?limits->maximum:0; }
};
struct NoiseChunkProvider {
    void* state = nullptr;
    NoiseChunkResult (*at)(void*, int32_t, NoiseChunkContext&) noexcept = nullptr;
    NoiseChunkResult (*direct)(void*, double*, size_t, NoiseChunkFunction) noexcept = nullptr;
    NoiseChunkResult for_index(int32_t i, NoiseChunkContext& c) noexcept;
    NoiseChunkResult fill_direct(double* values, size_t count, NoiseChunkFunction f) noexcept;
};
struct NoiseChunkState {
    int32_t start_x = 0, start_y = 0, start_z = 0;
    int32_t in_x = 0, in_y = 0, in_z = 0, array_index = 0;
    uint64_t sample_counter = 0, array_counter = 0;
    bool interpolating = false, filling_cell = false;
};
// Caller-owned stable arena entry. No allocation, virtual dispatch or Java
// object layout is required. Interpolation's 15 doubles overlap the other
// wrappers' single cached value; buffer layout is selected by kind.
struct NoiseCache {
    NoiseChunkFunction child;
    NoiseChunk* owner = nullptr;
    double* buffer = nullptr;
    double* slices[2]{nullptr,nullptr};
    size_t capacity = 0, array_length = 0;
    uint64_t position_key = 0, sample_counter = 0, array_counter = 0;
    double values[15]{};
    NoiseCacheKind kind = NoiseCacheKind::column;
    bool array_valid = false;
    NoiseChunkFunction function() noexcept;
private:
    friend class NoiseChunk;
    static NoiseChunkResult compute(void*, const NoiseChunkContext&, double&) noexcept;
    static NoiseChunkResult fill_array(void*, double*, size_t, NoiseChunkProvider&) noexcept;
};
static_assert(sizeof(NoiseCache) <= 256, "NoiseChunk wrapper arena budget");

class NoiseChunk {
public:
    NoiseChunk(NoiseChunkSettings settings, NoiseCache* arena, size_t capacity) noexcept;
    NoiseChunk(const NoiseChunk&) = delete;
    NoiseChunk& operator=(const NoiseChunk&) = delete;
    NoiseChunkResult status() const noexcept { return status_; }
    size_t required_values(NoiseCacheKind kind) const noexcept;
    NoiseChunkResult wrap(NoiseCacheKind kind, NoiseChunkFunction child,
                          double* buffer, size_t capacity, NoiseCache*& cache,
                          bool initialize_flat = true) noexcept;
    NoiseChunkResult initialize_first_x() noexcept;
    NoiseChunkResult advance_x(int32_t cell_x) noexcept;
    NoiseChunkResult select_yz(int32_t cell_y, int32_t cell_z) noexcept;
    void update_y(int32_t block_y, double fraction) noexcept;
    void update_x(int32_t block_x, double fraction) noexcept;
    void update_z(int32_t block_z, double fraction) noexcept;
    NoiseChunkResult stop() noexcept;
    void swap_slices() noexcept;
    DensityContext position() const noexcept;
    NoiseChunkContext context() const noexcept { return {{0,0,0},this}; }
    const NoiseChunkState& state() const noexcept { return state_; }
    const NoiseChunkSettings& settings() const noexcept { return settings_; }
    int32_t cell_count_y() const noexcept { return count_y_; }
    int32_t first_cell_x() const noexcept { return first_cell_x_; }
    int32_t first_cell_z() const noexcept { return first_cell_z_; }
    int32_t cell_min_y() const noexcept { return min_cell_y_; }
    int32_t first_quart_x() const noexcept { return first_quart_x_; }
    int32_t first_quart_z() const noexcept { return first_quart_z_; }
    int32_t quart_size() const noexcept { return quart_size_; }
    NoiseChunkProvider& cell_provider() noexcept { return cell_provider_; }
    NoiseChunkProvider& slice_provider() noexcept { return slice_provider_; }
private:
    friend struct NoiseCache;
    NoiseChunkSettings settings_;
    NoiseChunkState state_;
    NoiseCache* caches_;
    size_t capacity_, count_ = 0, slice_values_ = 0, flat_values_ = 0, cell_values_ = 0;
    int32_t count_y_ = 0, min_cell_y_ = 0, first_cell_x_ = 0, first_cell_z_ = 0;
    int32_t first_quart_x_ = 0, first_quart_z_ = 0, quart_size_ = 0;
    NoiseChunkResult status_ = NoiseChunkResult::invalid_settings;
    NoiseChunkProvider cell_provider_, slice_provider_;
    NoiseChunkResult fill_slice(unsigned slot, int32_t cell_x) noexcept;
    static NoiseChunkResult cell_at(void*, int32_t, NoiseChunkContext&) noexcept;
    static NoiseChunkResult slice_at(void*, int32_t, NoiseChunkContext&) noexcept;
    static NoiseChunkResult cell_direct(void*, double*, size_t, NoiseChunkFunction) noexcept;
    static NoiseChunkResult slice_direct(void*, double*, size_t, NoiseChunkFunction) noexcept;
};

// Bridge for already-bound point DensityGraph fields, e.g. a cache's filler.
// This does not run the NoiseChunk router visitor or rewrite graph markers.
// Frames are external, single-use workspace; keep each active filler separate.
struct NoiseChunkDensityField {
    const DensityGraph* graph = nullptr;
    DensityId root = 0;
    DensityFrame* frames = nullptr;
    size_t frame_capacity = 0;
    NoiseChunkLimits limits{};
    NoiseChunkFunction function() noexcept;
private:
    static NoiseChunkResult compute(void*, const NoiseChunkContext&, double&) noexcept;
    static NoiseChunkResult fill_array(void*, double*, size_t, NoiseChunkProvider&) noexcept;
};
}
