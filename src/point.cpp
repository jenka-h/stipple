#include "stipple/point.hpp"

#include <random>

namespace stipple {

std::size_t Points::size() const noexcept {
    return x.size();
}

void Points::resize(std::size_t count) {
    x.resize(count);
    y.resize(count);
}

void Points::clear() noexcept {
    x.clear();
    y.clear();
}

Points initialize_points_uniform(int width, int height, std::size_t count, unsigned int seed) {
    Points points;
    points.resize(count);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist_x(0.0f, static_cast<float>(width - 1));
    std::uniform_real_distribution<float> dist_y(0.0f, static_cast<float>(height - 1));

    for (std::size_t i = 0; i < count; ++i) {
        points.x[i] = dist_x(rng);
        points.y[i] = dist_y(rng);
    }

    return points;
}

} // namespace stipple
