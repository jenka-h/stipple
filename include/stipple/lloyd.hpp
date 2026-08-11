#pragma once

#include <functional>

#include "stipple/config.hpp"
#include "stipple/image.hpp"
#include "stipple/point.hpp"

namespace stipple {

using IterationCallback = std::function<void(int iteration, const Points& points)>;

struct LloydResult {
    Points points;
    int completed_iterations = 0;
    float final_average_movement = 0.0f;
};

LloydResult run_lloyd(const Image& image, const LloydConfig& config, IterationCallback callback = {});

} // namespace stipple
