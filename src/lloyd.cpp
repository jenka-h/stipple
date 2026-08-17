#include "stipple/lloyd.hpp"

#include <stdexcept>
#include <utility>

#include "stipple/logic/cuda.hpp"
#include "stipple/logic/openmp.hpp"
#include "stipple/logic/serial.hpp"

namespace stipple {

LloydResult run_lloyd(const Image& image, const LloydConfig& config, IterationCallback callback) {
    switch (config.backend) {
        case Backend::Serial:
        case Backend::SerialSIMD:
            return run_lloyd_serial(image, config, std::move(callback));
        case Backend::OpenMP:
        case Backend::OpenMPSIMD:
            return run_lloyd_openmp(image, config, std::move(callback));
        case Backend::CUDA:
            return run_lloyd_cuda(image, config, std::move(callback));
        case Backend::All:
            throw std::runtime_error("Backend::All is handled by the CLI comparison runner, not run_lloyd().");
    }

    throw std::runtime_error("Unsupported Lloyd backend");
}

} // namespace stipple
