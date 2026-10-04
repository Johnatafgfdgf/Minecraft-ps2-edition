#pragma once
#include <cstddef>
#include <cstdint>

namespace mcps2 {
class NormalNoise;
class BlendedNoise;
using DensityId = uint32_t;
enum class DensityOp : uint8_t {
    constant, y_gradient, add, multiply, minimum, maximum, clamp, range_choice,
    absolute, square, cube, half_negative, quarter_negative, squeeze, input,
    add_constant, multiply_constant, noise, shift, shift_a, shift_b, shifted_noise, blended_noise,
    interpolated, flat_cache, cache_2d, cache_once, cache_all_in_cell,
    blend_alpha, blend_offset, blend_density, reference, beardifier_marker
};
enum class DensityResult : uint8_t { ok, full, invalid_reference, invalid_operation, workspace_full };
struct DensityContext { int32_t x, y, z; };
struct DensityInput {
    const void* state = nullptr;
    double (*sample)(const void*, DensityContext) noexcept = nullptr;
    double minimum = 0, maximum = 0;
};
struct DensityNoiseBinding { const NormalNoise* normal = nullptr; const BlendedNoise* blended = nullptr; };
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
};
struct DensityFrame { double first = 0, second = 0; DensityId node = 0; uint8_t stage = 0; };

// Node/input arenas must outlive this graph and all active samples. Input callbacks
// must produce values consistent with their declared bounds. No allocation occurs.
class DensityGraph {
public:
    DensityGraph(DensityNode* nodes, size_t capacity,
                 const DensityInput* inputs = nullptr, size_t input_count = 0,
                 const DensityNoiseBinding* noises = nullptr, size_t noise_count = 0) noexcept;
    DensityResult append(const DensitySpec& spec, DensityId& id) noexcept;
    DensityResult sample(DensityId root, DensityContext context,
                         DensityFrame* workspace, size_t capacity, double& value) const noexcept;
    const DensityNode* node(DensityId id) const noexcept { return id < count_ ? nodes_ + id : nullptr; }
    size_t size() const noexcept { return count_; }
    size_t required_frames(DensityId root) const noexcept { return node(root) ? nodes_[root].depth : 0; }
private:
    DensityNode* nodes_;
    size_t capacity_, count_ = 0;
    const DensityInput* inputs_;
    size_t input_count_;
    const DensityNoiseBinding* noises_;
    size_t noise_count_;
};
static_assert(sizeof(DensityNode) <= 64, "Density arena node budget");
static_assert(sizeof(DensityFrame) <= 24, "Density evaluation frame budget");
}
