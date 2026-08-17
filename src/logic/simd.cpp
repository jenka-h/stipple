#include "stipple/logic/simd.hpp"

#include <cmath>
#include <limits>

namespace stipple {

float squared_distance(float ax, float ay, float bx, float by) noexcept {
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

int find_nearest_point_scalar(float pixel_x, float pixel_y, const Points& points) {
    float best_distance = std::numeric_limits<float>::max();
    int best_index = 0;

    for (std::size_t i = 0; i < points.size(); ++i) {
        const float distance = squared_distance(pixel_x, pixel_y, points.x[i], points.y[i]);
        if (distance < best_distance) {
            best_distance = distance;
            best_index = static_cast<int>(i);
        }
    }

    return best_index;
}

int find_nearest_point_simd(float pixel_x, float pixel_y, const Points& points) {
    float best_distance = std::numeric_limits<float>::max();
    const std::size_t count = points.size();

    // This lets the compiler vectorize the distance minimum reduction.
    // A second scalar pass recovers the index associated with the minimum.
    #pragma omp simd reduction(min: best_distance)
    for (std::size_t i = 0; i < count; ++i) {
        const float dx = pixel_x - points.x[i];
        const float dy = pixel_y - points.y[i];
        const float distance = dx * dx + dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
        }
    }

    int best_index = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const float distance = squared_distance(pixel_x, pixel_y, points.x[i], points.y[i]);
        if (distance == best_distance) {
            best_index = static_cast<int>(i);
            break;
        }
    }

    return best_index;
}

} // namespace stipple
