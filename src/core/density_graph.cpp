#include "mcps2/density_graph.hpp"
#include "mcps2/octave_noise.hpp"
#include "mcps2/blended_noise.hpp"
#include <cmath>
#include <limits>

namespace mcps2 {
namespace {
double java_min(double a, double b) noexcept {
    if (a != a) return a;
    if (a == 0 && b == 0 && std::signbit(b)) return b;
    return a <= b ? a : b;
}
double java_max(double a, double b) noexcept {
    if (a != a) return a;
    if (a == 0 && b == 0 && std::signbit(a)) return b;
    return a >= b ? a : b;
}
double clamp(double value, double low, double high) noexcept {
    return value < low ? low : java_min(value, high);
}
double mapped(DensityOp op, double value) noexcept {
    switch (op) {
        case DensityOp::absolute: return std::fabs(value);
        case DensityOp::square: return value * value;
        case DensityOp::cube: return (value * value) * value;
        case DensityOp::half_negative: return value > 0 ? value : value * 0.5;
        case DensityOp::quarter_negative: return value > 0 ? value : value * 0.25;
        case DensityOp::squeeze: {
            const double v = clamp(value, -1, 1);
            return v / 2 - ((v * v) * v) / 24;
        }
        default: return value;
    }
}
bool is_map(DensityOp op) noexcept { return op >= DensityOp::absolute && op <= DensityOp::squeeze; }
bool forwards(DensityOp op) noexcept {
    return (op >= DensityOp::interpolated && op <= DensityOp::cache_all_in_cell)
        || op == DensityOp::blend_density || op == DensityOp::reference;
}
bool normal_noise(DensityOp op) noexcept { return op >= DensityOp::noise && op <= DensityOp::shifted_noise; }
unsigned children(DensityOp op) noexcept {
    if (op >= DensityOp::add && op <= DensityOp::maximum) return 2;
    if (op == DensityOp::range_choice || op == DensityOp::shifted_noise) return 3;
    return op == DensityOp::clamp || is_map(op) || forwards(op) ? 1 : 0;
}
double gradient(const DensitySpec& s, int32_t y) noexcept {
    const double amount = (double(y) - double(s.from_y)) / (double(s.to_y) - double(s.from_y));
    if (amount < 0) return s.p0;
    if (amount > 1) return s.p1;
    return s.p0 + amount * (s.p1 - s.p0);
}
}

DensityGraph::DensityGraph(DensityNode* nodes, size_t capacity, const DensityInput* inputs, size_t input_count,
                           const DensityNoiseBinding* noises, size_t noise_count) noexcept
    : nodes_(nodes), capacity_(nodes ? capacity : 0), inputs_(inputs), input_count_(inputs ? input_count : 0),
      noises_(noises), noise_count_(noises ? noise_count : 0) {}

DensityResult DensityGraph::append(const DensitySpec& spec, DensityId& id) noexcept {
    if (count_ >= capacity_ || count_ >= std::numeric_limits<uint32_t>::max()) return DensityResult::full;
    // Specialized opcodes are created here after the original factory's bound calculation.
    if (spec.op > DensityOp::beardifier_marker || spec.op == DensityOp::add_constant || spec.op == DensityOp::multiply_constant)
        return DensityResult::invalid_operation;
    const unsigned arity = children(spec.op);
    const DensityId references[] = {spec.a, spec.b, spec.c};
    uint32_t depth = 1;
    for (unsigned i = 0; i < arity; ++i) {
        if (references[i] >= count_) return DensityResult::invalid_reference;
        const uint32_t d = nodes_[references[i]].depth;
        if (d == std::numeric_limits<uint32_t>::max()) return DensityResult::full;
        if (d + 1 > depth) depth = d + 1;
    }
    if (spec.op == DensityOp::input && (spec.a >= input_count_ || !inputs_[spec.a].sample))
        return DensityResult::invalid_reference;
    if (normal_noise(spec.op) || spec.op == DensityOp::blended_noise) {
        if (spec.from_y < 0 || size_t(spec.from_y) >= noise_count_) return DensityResult::invalid_reference;
        const auto& binding = noises_[spec.from_y];
        if (normal_noise(spec.op) ? (!binding.normal || !binding.normal->ready()) : (!binding.blended || !binding.blended->ready()))
            return DensityResult::invalid_reference;
    }
    DensityNode node{spec, 0, 0, depth};
    if (spec.op == DensityOp::constant) node.minimum = node.maximum = spec.p0;
    else if (spec.op == DensityOp::y_gradient) {
        node.minimum = java_min(spec.p0, spec.p1); node.maximum = java_max(spec.p0, spec.p1);
    } else if (spec.op == DensityOp::input) {
        node.minimum = inputs_[spec.a].minimum; node.maximum = inputs_[spec.a].maximum;
    } else if (normal_noise(spec.op)) {
        node.maximum = noises_[spec.from_y].normal->max_value();
        if (spec.op >= DensityOp::shift && spec.op <= DensityOp::shift_b) node.maximum *= 4;
        node.minimum = -node.maximum;
    } else if (spec.op == DensityOp::blended_noise) {
        node.minimum = noises_[spec.from_y].blended->min_value(); node.maximum = noises_[spec.from_y].blended->max_value();
    } else if (spec.op == DensityOp::blend_alpha) {
        node.minimum = node.maximum = 1;
    } else if (spec.op == DensityOp::blend_offset || spec.op == DensityOp::beardifier_marker) {
        node.minimum = node.maximum = 0;
    } else if (forwards(spec.op)) {
        node.minimum = spec.op == DensityOp::blend_density ? -std::numeric_limits<double>::infinity() : nodes_[spec.a].minimum;
        node.maximum = spec.op == DensityOp::blend_density ? std::numeric_limits<double>::infinity() : nodes_[spec.a].maximum;
    } else if (spec.op == DensityOp::clamp) {
        node.minimum = spec.p0; node.maximum = spec.p1;
    } else if (is_map(spec.op)) {
        const auto& child = nodes_[spec.a];
        const double low = mapped(spec.op, child.minimum), high = mapped(spec.op, child.maximum);
        if (spec.op == DensityOp::absolute || spec.op == DensityOp::square) {
            node.minimum = java_max(0, child.minimum); node.maximum = java_max(low, high);
        } else { node.minimum = low; node.maximum = high; }
    } else if (spec.op == DensityOp::range_choice) {
        node.minimum = java_min(nodes_[spec.b].minimum, nodes_[spec.c].minimum);
        node.maximum = java_max(nodes_[spec.b].maximum, nodes_[spec.c].maximum);
    } else {
        const auto& a = nodes_[spec.a]; const auto& b = nodes_[spec.b];
        switch (spec.op) {
            case DensityOp::add:
                node.minimum = a.minimum + b.minimum; node.maximum = a.maximum + b.maximum; break;
            case DensityOp::minimum:
                node.minimum = java_min(a.minimum, b.minimum); node.maximum = java_min(a.maximum, b.maximum); break;
            case DensityOp::maximum:
                node.minimum = java_max(a.minimum, b.minimum); node.maximum = java_max(a.maximum, b.maximum); break;
            case DensityOp::multiply:
                if (a.minimum > 0 && b.minimum > 0) {
                    node.minimum = a.minimum * b.minimum; node.maximum = a.maximum * b.maximum;
                } else if (a.maximum < 0 && b.maximum < 0) {
                    node.minimum = a.maximum * b.maximum; node.maximum = a.minimum * b.minimum;
                } else {
                    node.minimum = java_min(a.minimum * b.maximum, a.maximum * b.minimum);
                    node.maximum = java_max(a.minimum * b.minimum, a.maximum * b.maximum);
                }
                break;
            default: return DensityResult::invalid_operation;
        }
        if (spec.op == DensityOp::add || spec.op == DensityOp::multiply) {
            const DensityNode* constant = a.spec.op == DensityOp::constant ? &a : b.spec.op == DensityOp::constant ? &b : nullptr;
            if (constant) {
                node.spec.op = spec.op == DensityOp::add ? DensityOp::add_constant : DensityOp::multiply_constant;
                node.spec.a = constant == &a ? spec.b : spec.a;
                node.spec.p0 = constant->spec.p0;
                node.depth = nodes_[node.spec.a].depth + 1;
            }
        }
    }
    id = uint32_t(count_); nodes_[count_++] = node;
    return DensityResult::ok;
}

DensityResult DensityGraph::sample(DensityId root, DensityContext context, DensityFrame* stack,
                                   size_t capacity, double& output) const noexcept {
    if (root >= count_) return DensityResult::invalid_reference;
    if (!stack || capacity < nodes_[root].depth) return DensityResult::workspace_full;
    size_t top = 1; stack[0] = {0, 0, root, 0}; double value = 0;
    while (top) {
        auto& frame = stack[top - 1]; const auto& node = nodes_[frame.node]; const auto& s = node.spec;
        if (frame.stage == 0) {
            if (s.op == DensityOp::constant) { value = s.p0; --top; continue; }
            if (s.op == DensityOp::y_gradient) { value = gradient(s, context.y); --top; continue; }
            if (s.op == DensityOp::input) { value = inputs_[s.a].sample(inputs_[s.a].state, context); --top; continue; }
            if (s.op == DensityOp::blend_alpha) { value = 1; --top; continue; }
            if (s.op == DensityOp::blend_offset || s.op == DensityOp::beardifier_marker) { value = 0; --top; continue; }
            if (s.op == DensityOp::blended_noise) { value = noises_[s.from_y].blended->sample(context.x, context.y, context.z); --top; continue; }
            if (normal_noise(s.op) && s.op != DensityOp::shifted_noise) {
                const auto& field = *noises_[s.from_y].normal;
                if (s.op == DensityOp::noise) value = field.sample(double(context.x) * s.p0, double(context.y) * s.p1, double(context.z) * s.p0);
                else if (s.op == DensityOp::shift_a) value = field.sample(double(context.x) * 0.25, 0, double(context.z) * 0.25) * 4;
                else if (s.op == DensityOp::shift_b) value = field.sample(double(context.z) * 0.25, double(context.x) * 0.25, 0) * 4;
                else value = field.sample(double(context.x) * 0.25, double(context.y) * 0.25, double(context.z) * 0.25) * 4;
                --top; continue;
            }
            frame.stage = 1; stack[top++] = {0, 0, s.a, 0}; continue;
        }
        if (s.op == DensityOp::shifted_noise) {
            if (frame.stage == 1) { frame.first = double(context.x) * s.p0 + value; frame.stage = 2; stack[top++] = {0, 0, s.b, 0}; continue; }
            if (frame.stage == 2) { frame.second = double(context.y) * s.p1 + value; frame.stage = 3; stack[top++] = {0, 0, s.c, 0}; continue; }
            value = noises_[s.from_y].normal->sample(frame.first, frame.second, double(context.z) * s.p0 + value);
            --top; continue;
        }
        if (frame.stage == 1) {
            if (forwards(s.op)) { --top; continue; } // SinglePointContext; chunk wrappers/blending are a separate runtime.
            if (is_map(s.op)) { value = mapped(s.op, value); --top; continue; }
            if (s.op == DensityOp::clamp) { value = clamp(value, s.p0, s.p1); --top; continue; }
            if (s.op == DensityOp::add_constant) { value += s.p0; --top; continue; }
            if (s.op == DensityOp::multiply_constant) { value *= s.p0; --top; continue; }
            frame.first = value;
            if (s.op == DensityOp::multiply && value == 0) { value = 0; --top; continue; }
            if (s.op == DensityOp::minimum && value < nodes_[s.b].minimum) { --top; continue; }
            if (s.op == DensityOp::maximum && value > nodes_[s.b].maximum) { --top; continue; }
            const DensityId next = s.op == DensityOp::range_choice ? (value >= s.p0 && value < s.p1 ? s.b : s.c) : s.b;
            frame.stage = 2; stack[top++] = {0, 0, next, 0}; continue;
        }
        switch (s.op) {
            case DensityOp::add: value = frame.first + value; break;
            case DensityOp::multiply: value = frame.first * value; break;
            case DensityOp::minimum: value = java_min(frame.first, value); break;
            case DensityOp::maximum: value = java_max(frame.first, value); break;
            default: break; // range_choice returns the selected branch unchanged.
        }
        --top;
    }
    output = value; return DensityResult::ok;
}
}
