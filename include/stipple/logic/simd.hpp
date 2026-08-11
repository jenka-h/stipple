#pragma once

#include <cstddef>

#include "stipple/point.hpp"

namespace stipple {

int find_nearest_point_scalar(float pixel_x, float pixel_y, const Points& points);
int find_nearest_point_simd(float pixel_x, float pixel_y, const Points& points);

float squared_distance(float ax, float ay, float bx, float by) noexcept;

} // namespace stipple
