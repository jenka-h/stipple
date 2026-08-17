#include "stipple/config.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace stipple {

Backend parse_backend(const std::string& value) {
    std::string mode = value;
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (mode == "serial") return Backend::Serial;
    if (mode == "serial-simd" || mode == "simd") return Backend::SerialSIMD;
    if (mode == "openmp" || mode == "omp") return Backend::OpenMP;
    if (mode == "openmp-simd" || mode == "omp-simd") return Backend::OpenMPSIMD;
    if (mode == "cuda" || mode == "gpu") return Backend::CUDA;
    if (mode == "all") return Backend::All;

    throw std::invalid_argument("Unknown backend: " + value);
}

std::string to_string(Backend backend) {
    switch (backend) {
        case Backend::Serial: return "serial";
        case Backend::SerialSIMD: return "serial-simd";
        case Backend::OpenMP: return "openmp";
        case Backend::OpenMPSIMD: return "openmp-simd";
        case Backend::CUDA: return "cuda";
        case Backend::All: return "all";
    }
    return "unknown";
}

} // namespace stipple
