#pragma once

#include "stipple/lloyd.hpp"

namespace stipple {

LloydResult run_lloyd_openmp(const Image& image, const LloydConfig& config, IterationCallback callback = {});

} // namespace stipple
