#pragma once

#include <string>

#include "stipple/image.hpp"
#include "stipple/point.hpp"

namespace stipple {

void render_stipple_image(const Points& points, int width, int height, const std::string& output_path);
void render_stipple_over_image(const Image& image, const Points& points, const std::string& output_path);

} // namespace stipple
