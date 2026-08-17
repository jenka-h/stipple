#pragma once

#include <cstddef>
#include <vector>

namespace stipple {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Points {
    std::vector<float> x;
    std::vector<float> y;

    [[nodiscard]] std::size_t size() const noexcept;
    void resize(std::size_t count);
    void clear() noexcept;
};

Points initialize_points_uniform(int width, int height, std::size_t count, unsigned int seed);

} // namespace stipple
