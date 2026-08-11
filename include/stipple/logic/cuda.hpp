#pragma once

#include "stipple/lloyd.hpp"

namespace stipple {

bool is_cuda_available();
LloydResult run_lloyd_cuda(const Image& image, const LloydConfig& config, IterationCallback callback = {});

} // namespace stipple
