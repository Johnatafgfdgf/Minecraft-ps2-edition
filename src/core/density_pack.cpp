#include "mcps2/density_pack.hpp"
#include "mcps2/java_bits.hpp"
#include <cmath>
#include <cstring>
#include <limits>

namespace mcps2 {
namespace {
uint32_t crc32(const uint8_t* data, size_t size) noexcept {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
    }
    return ~crc;
}
bool resource_name(std::string_view name) noexcept {
    bool separator = false; size_t colon = 0;
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        if (c == ':') { if (separator || i == 0) return false; separator = true; colon = i; continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || (separator && c == '/')))
            return false;
    }
    return separator && colon + 1 < name.size();
}
}
uint32_t DensityPack::u32(size_t offset) const noexcept {
    const auto* p = bytes_ + offset;
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
double DensityPack::real(size_t offset) const noexcept {
    uint64_t bits = 0;
    for (unsigned i = 0; i < 8; ++i) bits |= uint64_t(bytes_[offset + i]) << (8 * i);
    double value; std::memcpy(&value, &bits, sizeof(value)); return value;
}
float DensityPack::single(size_t offset) const noexcept {
    const uint32_t bits=u32(offset);float value;std::memcpy(&value,&bits,sizeof(value));return value;
}
bool DensityPack::spline(uint32_t index,DensitySplineView& result) const noexcept {
    if (!bytes_ || index>=splines_) return false;
    const size_t offset=spline_offset_+size_t(index)*16;
    result={u32(offset),u32(offset+4),u32(offset+8)};return true;
}
bool DensityPack::point(uint32_t index,DensitySplinePoint& result) const noexcept {
    if (!bytes_ || index>=points_) return false;
    const size_t offset=point_offset_+size_t(index)*16;
    result={single(offset),single(offset+4),u32(offset+8)};return true;
}
bool DensityPack::spec(uint32_t index, DensitySpec& result) const noexcept {
    if (!bytes_ || index >= nodes_) return false;
    const size_t offset = node_offset_ + size_t(index) * 40;
    result = {DensityOp(bytes_[offset]),u32(offset + 4),u32(offset + 8),u32(offset + 12),
              signed32(u32(offset + 16)),signed32(u32(offset + 20)),real(offset + 24),real(offset + 32)};
    return true;
}
bool DensityPack::resource(uint32_t index, DensityResourceView& result) const noexcept {
    if (!bytes_ || index >= resources_) return false;
    const size_t offset = resource_offset_ + size_t(index) * 64;
    result.kind = u32(offset); result.first_octave = signed32(u32(offset + 12)); result.amplitude_count = u32(offset + 20);
    result.name = std::string_view(reinterpret_cast<const char*>(bytes_ + u32(offset + 4)), u32(offset + 8));
    result.blended = {real(offset + 24),real(offset + 32),real(offset + 40),real(offset + 48),real(offset + 56)};
    return true;
}
bool DensityPack::amplitude(uint32_t resource_index, uint32_t index, double& result) const noexcept {
    if (!bytes_ || resource_index >= resources_) return false;
    const size_t offset = resource_offset_ + size_t(resource_index) * 64;
    if (index >= u32(offset + 20)) return false;
    result = real(u32(offset + 16) + size_t(index) * 8); return true;
}
bool DensityPack::bind(const void* data, size_t size) noexcept {
    bytes_ = nullptr; nodes_ = resources_ = root_ = node_offset_ = resource_offset_ = 0; legacy_ = false;
    splines_=points_=spline_offset_=point_offset_=0;
    if (!data || size < 56 || size > std::numeric_limits<uint32_t>::max() || std::memcmp(data,"MCDG",4) != 0) return false;
    bytes_ = static_cast<const uint8_t*>(data);
    auto reject = [this]() noexcept { bytes_ = nullptr; nodes_ = resources_ = splines_ = points_ = 0; return false; };
    if (u32(4) != 2 || (u32(8) != 2 && u32(8) != 3) || u32(32) != size || crc32(bytes_ + 56, size - 56) != u32(36)) return reject();
    nodes_ = u32(12); root_ = u32(16); resources_ = u32(20); node_offset_ = u32(24); resource_offset_ = u32(28); legacy_ = u32(8) == 3;
    splines_=u32(40);points_=u32(44);spline_offset_=u32(48);point_offset_=u32(52);
    if (!nodes_ || root_ >= nodes_ || node_offset_ != 56 || nodes_ > (size - 56) / 40) return reject();
    const size_t node_end = 56 + size_t(nodes_) * 40;
    if (resource_offset_ != node_end || resources_ > (size - node_end) / 64) return reject();
    const size_t resource_end = node_end + size_t(resources_) * 64;
    if (spline_offset_!=resource_end || splines_>(size-resource_end)/16) return reject();
    const size_t spline_end=resource_end+size_t(splines_)*16;
    if (point_offset_!=spline_end || points_>(size-spline_end)/16) return reject();
    const size_t table_end = spline_end+size_t(points_)*16;
    auto span = [size,table_end](uint32_t offset, uint32_t count, unsigned width) noexcept {
        return offset >= table_end && offset <= size && count <= (size - offset) / width;
    };
    for (uint32_t i = 0; i < resources_; ++i) {
        const size_t offset = resource_offset_ + size_t(i) * 64;
        if (u32(offset) > 2 || !span(u32(offset + 4),u32(offset + 8),1) || !span(u32(offset + 16),u32(offset + 20),8)) return reject();
        DensityResourceView r; resource(i,r);
        if (!resource_name(r.name)) return reject();
        if (r.kind==2 && r.amplitude_count) return reject();
        if (r.kind == 1) {
            if (r.amplitude_count || r.blended.xz_scale < 0.001 || r.blended.xz_scale > 1000
                || r.blended.y_scale < 0.001 || r.blended.y_scale > 1000 || r.blended.xz_factor < 0.001 || r.blended.xz_factor > 1000
                || r.blended.y_factor < 0.001 || r.blended.y_factor > 1000 || r.blended.smear_scale < 1 || r.blended.smear_scale > 8) return reject();
        }
        for (unsigned n = 0; n < 5; ++n) if (!std::isfinite(real(offset + 24 + n * 8))) return reject();
        for (uint32_t n = 0; n < r.amplitude_count; ++n) {
            double value; amplitude(i,n,value); if (!std::isfinite(value)) return reject();
        }
    }
    for (uint32_t i=0;i<points_;++i) {
        DensitySplinePoint p;point(i,p);
        if (p.value>=nodes_ || !std::isfinite(double(p.location)) || !std::isfinite(double(p.derivative)) || u32(point_offset_+size_t(i)*16+12)) return reject();
    }
    for (uint32_t i=0;i<splines_;++i) {
        DensitySplineView s;spline(i,s);
        if (!s.point_count || s.coordinate>=nodes_ || s.first_point>points_ || s.point_count>points_-s.first_point
            || u32(spline_offset_+size_t(i)*16+12)) return reject();
    }
    for (uint32_t i = 0; i < nodes_; ++i) {
        DensitySpec s; spec(i,s); const unsigned op = unsigned(s.op);
        const size_t offset = node_offset_ + size_t(i) * 40;
        if (bytes_[offset + 1] || bytes_[offset + 2] || bytes_[offset + 3] || op > 35 || (op >= 14 && op <= 16)
            || !std::isfinite(s.p0) || !std::isfinite(s.p1)) return reject();
        unsigned arity = op >= 2 && op <= 5 ? 2 : op == 7 || op == 21 ? 3 : op == 6 || (op >= 8 && op <= 13) || (op >= 23 && op <= 27) || op == 30 || op == 31 || op == 33 || op == 35 ? 1 : 0;
        const uint32_t references[] = {s.a,s.b,s.c};
        for (unsigned j = 0; j < arity; ++j) if (references[j] >= i) return reject();
        if (op==33 && s.to_y!=0 && s.to_y!=1) return reject();
        if ((op >= 17 && op <= 22) || op==33 || op==34) {
            if (s.from_y < 0 || uint32_t(s.from_y) >= resources_) return reject();
            DensityResourceView r; resource(uint32_t(s.from_y),r);
            if (r.kind != (op == 22 ? 1u : op==34 ? 2u : 0u)) return reject();
        }
        if (op==35) {
            if (s.from_y<0 || uint32_t(s.from_y)>=splines_) return reject();
            DensitySplineView view;spline(uint32_t(s.from_y),view);
            if (s.a!=view.coordinate) return reject();
            for (uint32_t j=0;j<view.point_count;++j) {
                DensitySplinePoint p;point(view.first_point+j,p);if (p.value>=i) return reject();
            }
        }
    }
    return true;
}
}
