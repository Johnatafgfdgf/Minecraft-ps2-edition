#pragma once
#include <cstddef>
#include <cstdint>

namespace mcps2 {
class NormalNoise;
class BlendedNoise;
class EndIslandDensity;
class NoiseChunk;
class DensityGraph;
using DensityId = uint32_t;
enum class DensityOp : uint8_t {
    constant, y_gradient, add, multiply, minimum, maximum, clamp, range_choice,
    absolute, square, cube, half_negative, quarter_negative, squeeze, input,
    add_constant, multiply_constant, noise, shift, shift_a, shift_b, shifted_noise, blended_noise,
    interpolated, flat_cache, cache_2d, cache_once, cache_all_in_cell,
    blend_alpha, blend_offset, blend_density, reference, beardifier_marker,
    weird_scaled_sampler, end_islands, spline, spline_constant
};
enum class DensityResult : uint8_t { ok, full, invalid_reference, invalid_operation, workspace_full, inactive, invalid_index };
struct DensityContext { int32_t x, y, z; const NoiseChunk* owner = nullptr; };
struct DensitySampleFunction {
    const void* state = nullptr;
    DensityResult (*sample)(const void*, DensityContext, double&) noexcept = nullptr;
};
struct DensityBatchProvider {
    void* state = nullptr;
    DensityResult (*at)(void*, int32_t, DensityContext&) noexcept = nullptr;
    DensityResult (*direct)(void*, double*, size_t, DensitySampleFunction) noexcept = nullptr;
};
struct DensityInput {
    const void* state = nullptr;
    double (*sample)(const void*, DensityContext) noexcept = nullptr;
    double minimum = 0, maximum = 0;
    DensityResult (*checked_sample)(const void*, DensityContext, double&) noexcept = nullptr;
    DensityResult (*fill)(const void*, double*, size_t, DensityBatchProvider&) noexcept = nullptr;
};
struct DensityNoiseBinding { const NormalNoise* normal = nullptr; const BlendedNoise* blended = nullptr; const EndIslandDensity* end = nullptr; };
struct DensitySplinePoint { float location = 0, derivative = 0; DensityId value = 0; };
struct DensitySpline { const DensitySplinePoint* points = nullptr; size_t count = 0; };
struct DensitySpec {
    DensityOp op = DensityOp::constant;
    DensityId a = 0, b = 0, c = 0;
    int32_t from_y = 0, to_y = 0;
    double p0 = 0, p1 = 0;
};
struct DensityNode {
    DensitySpec spec;
    double minimum = 0, maximum = 0;
    uint32_t depth = 1;
    uint32_t temporary_arrays = 0;
};
struct DensityFrame { double first = 0, second = 0; DensityId node = 0; uint8_t stage = 0; };
struct DensityBatchFrame { double* output = nullptr; size_t temporary_mark = 0; DensityId node = 0; uint8_t stage = 0; };

// Node/input arenas must outlive this graph and all active samples. Input callbacks
// must produce values consistent with their declared bounds. No allocation occurs.
class DensityGraph {
public:
    DensityGraph(DensityNode* nodes, size_t capacity,
                 const DensityInput* inputs = nullptr, size_t input_count = 0,
                 const DensityNoiseBinding* noises = nullptr, size_t noise_count = 0,
                 const DensitySpline* splines = nullptr, size_t spline_count = 0) noexcept;
    DensityResult append(const DensitySpec& spec, DensityId& id) noexcept;
    // mapAll preserves MulOrAdd's class/argument after transforming its child;
    // rerunning the binary factory here could exchange two constant operands.
    DensityResult append_transformed(const DensitySpec& spec, DensityId& id) noexcept;
    DensityResult sample(DensityId root, DensityContext context,
                         DensityFrame* workspace, size_t capacity, double& value) const noexcept;
    // fillArray has a distinct traversal from repeated point sampling. All
    // arenas are external and nonoverlapping; providers must preserve context
    // identity/counters. Capacity is checked before output/provider mutation.
    DensityResult fill(DensityId root, double* output, size_t count, DensityBatchProvider& provider,
                       DensityFrame* sample_frames, size_t sample_capacity,
                       DensityBatchFrame* batch_frames, size_t batch_capacity,
                       double* temporary, size_t temporary_capacity) const noexcept;
    const DensityNode* node(DensityId id) const noexcept { return id < count_ ? nodes_ + id : nullptr; }
    size_t size() const noexcept { return count_; }
    const DensityInput* inputs() const noexcept { return inputs_; }
    size_t input_count() const noexcept { return input_count_; }
    const DensityNoiseBinding* noises() const noexcept { return noises_; }
    size_t noise_count() const noexcept { return noise_count_; }
    const DensitySpline* splines() const noexcept { return splines_; }
    size_t spline_count() const noexcept { return spline_count_; }
    size_t required_frames(DensityId root) const noexcept { return node(root) ? nodes_[root].depth : 0; }
    size_t required_temporary_arrays(DensityId root) const noexcept { return node(root) ? nodes_[root].temporary_arrays : 0; }
private:
    DensityNode* nodes_;
    size_t capacity_, count_ = 0;
    const DensityInput* inputs_;
    size_t input_count_;
    const DensityNoiseBinding* noises_;
    size_t noise_count_;
    const DensitySpline* splines_;
    size_t spline_count_;
};
static_assert(sizeof(DensityNode) <= 64, "Density arena node budget");
static_assert(sizeof(DensityFrame) <= 24, "Density evaluation frame budget");
static_assert(sizeof(DensityBatchFrame) <= 24, "Density batch frame budget");
}
