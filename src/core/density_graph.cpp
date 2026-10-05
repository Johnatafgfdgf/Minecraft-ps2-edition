#include "mcps2/density_graph.hpp"
#include "mcps2/octave_noise.hpp"
#include "mcps2/blended_noise.hpp"
#include "mcps2/simplex_noise.hpp"
#include "mcps2/density_spline.hpp"
#include "mcps2/java_float.hpp"
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
bool normal_noise(DensityOp op) noexcept { return (op >= DensityOp::noise && op <= DensityOp::shifted_noise) || op == DensityOp::weird_scaled_sampler; }
unsigned children(DensityOp op) noexcept {
    if (op >= DensityOp::add && op <= DensityOp::maximum) return 2;
    if (op == DensityOp::range_choice || op == DensityOp::shifted_noise) return 3;
    return op == DensityOp::clamp || is_map(op) || forwards(op) || op == DensityOp::weird_scaled_sampler || op == DensityOp::spline ? 1 : 0;
}
double gradient(const DensitySpec& s, int32_t y) noexcept {
    const double amount = (double(y) - double(s.from_y)) / (double(s.to_y) - double(s.from_y));
    if (amount < 0) return s.p0;
    if (amount > 1) return s.p1;
    return s.p0 + amount * (s.p1 - s.p0);
}
double weird(const DensitySpec& s, const NormalNoise& field, DensityContext context, double value) noexcept {
    const double scale=s.to_y==0 ? (value < -0.5 ? 0.75 : value < 0 ? 1.0 : value < 0.5 ? 1.5 : 2.0)
        : (value < -0.75 ? 0.5 : value < -0.5 ? 0.75 : value < 0.5 ? 1.0 : value < 0.75 ? 2.0 : 3.0);
    return scale*std::fabs(field.sample(double(context.x)/scale,double(context.y)/scale,double(context.z)/scale));
}
struct BatchPoint {
    const DensityGraph* graph;
    DensityId root;
    DensityFrame* frames;
    size_t capacity;
    static DensityResult compute(const void* state, DensityContext context, double& value) noexcept {
        const auto& p=*static_cast<const BatchPoint*>(state);
        return p.graph->sample(p.root,context,p.frames,p.capacity,value);
    }
};
}

DensityGraph::DensityGraph(DensityNode* nodes, size_t capacity, const DensityInput* inputs, size_t input_count,
                           const DensityNoiseBinding* noises, size_t noise_count, const DensitySpline* splines, size_t spline_count) noexcept
    : nodes_(nodes), capacity_(nodes ? capacity : 0), inputs_(inputs), input_count_(inputs ? input_count : 0),
      noises_(noises), noise_count_(noises ? noise_count : 0), splines_(splines), spline_count_(splines ? spline_count : 0) {}

DensityResult DensityGraph::append(const DensitySpec& spec, DensityId& id) noexcept {
    if (count_ >= capacity_ || count_ >= std::numeric_limits<uint32_t>::max()) return DensityResult::full;
    // Specialized opcodes are created here after the original factory's bound calculation.
    if (spec.op > DensityOp::spline_constant || spec.op == DensityOp::add_constant || spec.op == DensityOp::multiply_constant
        || (spec.op == DensityOp::weird_scaled_sampler && spec.to_y != 0 && spec.to_y != 1))
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
    if (spec.op == DensityOp::input && (spec.a >= input_count_ || (!inputs_[spec.a].sample && !inputs_[spec.a].checked_sample)))
        return DensityResult::invalid_reference;
    if (normal_noise(spec.op) || spec.op == DensityOp::blended_noise || spec.op == DensityOp::end_islands) {
        if (spec.from_y < 0 || size_t(spec.from_y) >= noise_count_) return DensityResult::invalid_reference;
        const auto& binding = noises_[spec.from_y];
        if (normal_noise(spec.op) ? (!binding.normal || !binding.normal->ready()) : spec.op == DensityOp::end_islands ? (!binding.end || !binding.end->ready()) : (!binding.blended || !binding.blended->ready()))
            return DensityResult::invalid_reference;
    }
    if (spec.op == DensityOp::spline) {
        if (spec.from_y < 0 || size_t(spec.from_y) >= spline_count_) return DensityResult::invalid_reference;
        const auto& spline = splines_[spec.from_y];
        if (!spline.points || !spline.count) return DensityResult::invalid_reference;
        for (size_t i = 0; i < spline.count; ++i) {
            if (spline.points[i].value >= count_) return DensityResult::invalid_reference;
            const uint32_t d = nodes_[spline.points[i].value].depth;
            if (d == std::numeric_limits<uint32_t>::max()) return DensityResult::full;
            if (d + 1 > depth) depth = d + 1;
        }
    }
    DensityNode node{spec, 0, 0, depth};
    if (spec.op == DensityOp::constant) node.minimum = node.maximum = spec.p0;
    else if (spec.op == DensityOp::spline_constant) node.minimum = node.maximum = node.spec.p0 = double(java_float::round(spec.p0));
    else if (spec.op == DensityOp::y_gradient) {
        node.minimum = java_min(spec.p0, spec.p1); node.maximum = java_max(spec.p0, spec.p1);
    } else if (spec.op == DensityOp::input) {
        node.minimum = inputs_[spec.a].minimum; node.maximum = inputs_[spec.a].maximum;
    } else if (normal_noise(spec.op)) {
        node.maximum = noises_[spec.from_y].normal->max_value();
        if (spec.op >= DensityOp::shift && spec.op <= DensityOp::shift_b) node.maximum *= 4;
        if (spec.op == DensityOp::weird_scaled_sampler) node.maximum = (spec.to_y == 0 ? 2.0 : 3.0) * node.maximum;
        node.minimum = spec.op == DensityOp::weird_scaled_sampler ? 0 : -node.maximum;
    } else if (spec.op == DensityOp::blended_noise) {
        node.minimum = noises_[spec.from_y].blended->min_value(); node.maximum = noises_[spec.from_y].blended->max_value();
    } else if (spec.op == DensityOp::end_islands) {
        node.minimum = EndIslandDensity::minimum; node.maximum = EndIslandDensity::maximum;
    } else if (spec.op == DensityOp::spline) {
        float low,high;
        spline_bounds(splines_[spec.from_y],nodes_,java_float::round(nodes_[spec.a].minimum),java_float::round(nodes_[spec.a].maximum),low,high);
        node.minimum = double(low); node.maximum = double(high);
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
    const auto& s=node.spec;
    if (s.op==DensityOp::add) {
        const uint32_t b=nodes_[s.b].temporary_arrays;
        if (b==UINT32_MAX) return DensityResult::full;
        node.temporary_arrays=nodes_[s.a].temporary_arrays>b+1?nodes_[s.a].temporary_arrays:b+1;
    } else if (s.op==DensityOp::multiply || s.op==DensityOp::minimum || s.op==DensityOp::maximum ||
               s.op==DensityOp::range_choice || s.op==DensityOp::clamp || is_map(s.op) || forwards(s.op) ||
               s.op==DensityOp::weird_scaled_sampler || s.op==DensityOp::add_constant || s.op==DensityOp::multiply_constant) {
        node.temporary_arrays=nodes_[s.a].temporary_arrays;
    }
    id = uint32_t(count_); nodes_[count_++] = node;
    return DensityResult::ok;
}

DensityResult DensityGraph::append_transformed(const DensitySpec& spec, DensityId& id) noexcept {
    if (spec.op != DensityOp::add_constant && spec.op != DensityOp::multiply_constant) return append(spec,id);
    if (count_ >= capacity_ || count_ >= UINT32_MAX) return DensityResult::full;
    if (spec.a >= count_) return DensityResult::invalid_reference;
    const auto& child=nodes_[spec.a];
    if (child.depth==UINT32_MAX) return DensityResult::full;
    DensityNode node{spec,0,0,child.depth+1,child.temporary_arrays};
    if (spec.op==DensityOp::add_constant) {
        node.minimum=child.minimum+spec.p0; node.maximum=child.maximum+spec.p0;
    } else if (spec.p0>=0) {
        node.minimum=child.minimum*spec.p0; node.maximum=child.maximum*spec.p0;
    } else {
        node.minimum=child.maximum*spec.p0; node.maximum=child.minimum*spec.p0;
    }
    id=uint32_t(count_);nodes_[count_++]=node;return DensityResult::ok;
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
            if (s.op == DensityOp::spline_constant) { value = double(java_float::round(s.p0)); --top; continue; }
            if (s.op == DensityOp::y_gradient) { value = gradient(s, context.y); --top; continue; }
            if (s.op == DensityOp::input) {
                const auto& input=inputs_[s.a];
                if (input.checked_sample) {
                    const auto result=input.checked_sample(input.state,context,value);
                    if (result!=DensityResult::ok) return result;
                } else value=input.sample(input.state,context);
                --top; continue;
            }
            if (s.op == DensityOp::blend_alpha) { value = 1; --top; continue; }
            if (s.op == DensityOp::blend_offset || s.op == DensityOp::beardifier_marker) { value = 0; --top; continue; }
            if (s.op == DensityOp::blended_noise) { value = noises_[s.from_y].blended->sample(context.x, context.y, context.z); --top; continue; }
            if (s.op == DensityOp::end_islands) { value = noises_[s.from_y].end->sample(context.x,context.z); --top; continue; }
            if (normal_noise(s.op) && s.op != DensityOp::shifted_noise && s.op != DensityOp::weird_scaled_sampler) {
                const auto& field = *noises_[s.from_y].normal;
                if (s.op == DensityOp::noise) value = field.sample(double(context.x) * s.p0, double(context.y) * s.p1, double(context.z) * s.p0);
                else if (s.op == DensityOp::shift_a) value = field.sample(double(context.x) * 0.25, 0, double(context.z) * 0.25) * 4;
                else if (s.op == DensityOp::shift_b) value = field.sample(double(context.z) * 0.25, double(context.x) * 0.25, 0) * 4;
                else value = field.sample(double(context.x) * 0.25, double(context.y) * 0.25, double(context.z) * 0.25) * 4;
                --top; continue;
            }
            frame.stage = 1; stack[top++] = {0, 0, s.a, 0}; continue;
        }
        if (s.op == DensityOp::spline) {
            const auto& spline = splines_[s.from_y];
            if (frame.stage == 1) {
                frame.first = double(java_float::round(value));
                const size_t upper = spline_upper(spline,java_float::round(frame.first));
                const size_t point = upper == 0 ? 0 : upper - 1;
                frame.stage = 2; stack[top++] = {0,0,spline.points[point].value,0}; continue;
            }
            const size_t upper = spline_upper(spline,java_float::round(frame.first));
            if (frame.stage == 2) {
                if (upper == 0 || upper == spline.count) {
                    value = double(spline_extrapolate(java_float::round(frame.first),spline.points[upper == 0 ? 0 : upper - 1],java_float::round(value)));
                    --top; continue;
                }
                frame.second = double(java_float::round(value)); frame.stage = 3; stack[top++] = {0,0,spline.points[upper].value,0}; continue;
            }
            value = double(spline_segment(java_float::round(frame.first),spline.points[upper - 1],spline.points[upper],java_float::round(frame.second),java_float::round(value)));
            --top; continue;
        }
        if (s.op == DensityOp::shifted_noise) {
            if (frame.stage == 1) { frame.first = double(context.x) * s.p0 + value; frame.stage = 2; stack[top++] = {0, 0, s.b, 0}; continue; }
            if (frame.stage == 2) { frame.second = double(context.y) * s.p1 + value; frame.stage = 3; stack[top++] = {0, 0, s.c, 0}; continue; }
            value = noises_[s.from_y].normal->sample(frame.first, frame.second, double(context.z) * s.p0 + value);
            --top; continue;
        }
        if (frame.stage == 1) {
            if (s.op == DensityOp::weird_scaled_sampler) {
                value=weird(s,*noises_[s.from_y].normal,context,value);
                --top; continue;
            }
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

DensityResult DensityGraph::fill(DensityId root, double* output, size_t count, DensityBatchProvider& provider,
                                 DensityFrame* sample_frames, size_t sample_capacity,
                                 DensityBatchFrame* batch_frames, size_t batch_capacity,
                                 double* temporary, size_t temporary_capacity) const noexcept {
    if (root>=count_) return DensityResult::invalid_reference;
    if (count>size_t(INT32_MAX)) return DensityResult::invalid_operation;
    if (count>std::numeric_limits<size_t>::max()/sizeof(double)) return DensityResult::workspace_full;
    if ((count && (!output || !sample_frames || sample_capacity<nodes_[root].depth)) ||
        !batch_frames || batch_capacity<nodes_[root].depth) return DensityResult::workspace_full;
    const size_t arrays=nodes_[root].temporary_arrays;
    if (count && arrays>std::numeric_limits<size_t>::max()/count) return DensityResult::workspace_full;
    const size_t need=count*arrays;
    if (need>std::numeric_limits<size_t>::max()/sizeof(double) || temporary_capacity<need || (need && !temporary))
        return DensityResult::workspace_full;
    if (need) {
        const uintptr_t a=reinterpret_cast<uintptr_t>(output),b=reinterpret_cast<uintptr_t>(temporary);
        if ((a<=b && b-a<count*sizeof(double)) || (b<a && a-b<need*sizeof(double))) return DensityResult::invalid_operation;
    }
    size_t top=1,used=0; batch_frames[0]={output,0,root,0};
    while (top) {
        auto& f=batch_frames[top-1]; const auto& s=nodes_[f.node].spec; double* out=f.output;
        if (f.stage==0) {
            if (s.op==DensityOp::constant || s.op==DensityOp::beardifier_marker ||
                s.op==DensityOp::blend_alpha || s.op==DensityOp::blend_offset) {
                const double value=s.op==DensityOp::constant?s.p0:s.op==DensityOp::blend_alpha?1:0;
                for (size_t i=0;i<count;++i) out[i]=value;
                --top; continue;
            }
            if (s.op==DensityOp::input && inputs_[s.a].fill) {
                const auto& input=inputs_[s.a]; const auto r=input.fill(input.state,out,count,provider);
                if (r!=DensityResult::ok) return r;
                --top; continue;
            }
            const bool child_fill=s.op==DensityOp::add || s.op==DensityOp::multiply || s.op==DensityOp::minimum ||
                s.op==DensityOp::maximum || s.op==DensityOp::range_choice || s.op==DensityOp::clamp || is_map(s.op) ||
                forwards(s.op) || s.op==DensityOp::weird_scaled_sampler || s.op==DensityOp::add_constant || s.op==DensityOp::multiply_constant;
            if (!child_fill) {
                if (!provider.direct) return DensityResult::invalid_operation;
                const BatchPoint point{this,f.node,sample_frames,sample_capacity};
                const auto r=provider.direct(provider.state,out,count,{&point,BatchPoint::compute});
                if (r!=DensityResult::ok) return r;
                --top; continue;
            }
            f.stage=1; batch_frames[top++]={out,used,s.a,0}; continue;
        }
        if (s.op==DensityOp::add && f.stage==1) {
            f.temporary_mark=used; f.stage=2;
            double* second=count?temporary+used:nullptr; used+=count;
            // The original allocates a zero-initialized second array. A
            // provider may fill only its cell extent, leaving a tail intact.
            for (size_t i=0;i<count;++i) second[i]=0;
            batch_frames[top++]={second,used,s.b,0}; continue;
        }
        if (s.op==DensityOp::add) {
            for (size_t i=0;i<count;++i) out[i]+=temporary[f.temporary_mark+i];
            used=f.temporary_mark; --top; continue;
        }
        if (forwards(s.op) && s.op!=DensityOp::blend_density) { --top; continue; }
        if (is_map(s.op) || s.op==DensityOp::clamp || s.op==DensityOp::add_constant || s.op==DensityOp::multiply_constant) {
            for (size_t i=0;i<count;++i) {
                if (is_map(s.op)) out[i]=mapped(s.op,out[i]);
                else if (s.op==DensityOp::clamp) out[i]=clamp(out[i],s.p0,s.p1);
                else if (s.op==DensityOp::add_constant) out[i]+=s.p0;
                else out[i]*=s.p0;
            }
            --top; continue;
        }
        for (size_t i=0;i<count;++i) {
            const double first=out[i];
            if (s.op==DensityOp::multiply && first==0) { out[i]=0; continue; }
            if (s.op==DensityOp::minimum && first<nodes_[s.b].minimum) continue;
            if (s.op==DensityOp::maximum && first>nodes_[s.b].maximum) continue;
            if (!provider.at) return DensityResult::invalid_operation;
            DensityContext context{0,0,0}; auto r=provider.at(provider.state,int32_t(i),context);
            if (r!=DensityResult::ok) return r;
            if (s.op==DensityOp::blend_density) continue; // Empty Blender domain; still call forIndex.
            if (s.op==DensityOp::weird_scaled_sampler) { out[i]=weird(s,*noises_[s.from_y].normal,context,first); continue; }
            const DensityId child=s.op==DensityOp::range_choice?(first>=s.p0 && first<s.p1?s.b:s.c):s.b;
            double second=0; r=sample(child,context,sample_frames,sample_capacity,second);
            if (r!=DensityResult::ok) return r;
            if (s.op==DensityOp::multiply) out[i]=first*second;
            else if (s.op==DensityOp::minimum) out[i]=java_min(first,second);
            else if (s.op==DensityOp::maximum) out[i]=java_max(first,second);
            else out[i]=second;
        }
        --top;
    }
    return DensityResult::ok;
}
}
