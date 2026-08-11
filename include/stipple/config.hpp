#pragma once

#include <cstddef>
#include <string>

namespace stipple {

enum class Backend {
    Serial,
    SerialSIMD,
    OpenMP,
    OpenMPSIMD,
    CUDA
};

struct LloydConfig {
    std::size_t point_count = 1000;
    int iterations = 20;
    float epsilon = 0.001f;
    unsigned int seed = 42;
    Backend backend = Backend::Serial;
    bool animate = false;
};

Backend parse_backend(const std::string& value);
std::string to_string(Backend backend);

} // namespace stipple
