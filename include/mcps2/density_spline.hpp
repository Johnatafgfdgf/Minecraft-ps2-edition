#pragma once
#include "mcps2/density_graph.hpp"

namespace mcps2 {
size_t spline_upper(const DensitySpline& spline,float coordinate) noexcept;
float spline_extrapolate(float coordinate,const DensitySplinePoint& point,float value) noexcept;
float spline_segment(float coordinate,const DensitySplinePoint& left,const DensitySplinePoint& right,float a,float b) noexcept;
void spline_bounds(const DensitySpline& spline,const DensityNode* nodes,float coordinate_min,float coordinate_max,
                   float& minimum,float& maximum) noexcept;
}
