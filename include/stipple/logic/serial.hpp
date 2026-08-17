#pragma once

#include "stipple/lloyd.hpp"

namespace stipple {

LloydResult run_lloyd_serial(const Image& image, const LloydConfig& config, IterationCallback callback = {});

} // namespace stipple
