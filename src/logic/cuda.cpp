#include "stipple/logic/cuda.hpp"

#include <stdexcept>

namespace stipple {

bool is_cuda_available() {
    return false;
}

LloydResult run_lloyd_cuda(const Image&, const LloydConfig&, IterationCallback) {
    throw std::runtime_error("CUDA backend was not built. Reconfigure with -DSTIPPLE_ENABLE_CUDA=ON and a CUDA compiler.");
}

} // namespace stipple
