#include "stipple/logic/serial.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "stipple/logic/simd.hpp"

namespace stipple {

LloydResult run_lloyd_serial(const Image& image, const LloydConfig& config, IterationCallback callback) {
    Points points = initialize_points_uniform(image.width, image.height, config.point_count, config.seed);
    const bool use_simd = config.backend == Backend::SerialSIMD;

    LloydResult result;
    result.completed_iterations = 0;

    std::vector<float> sum_x(config.point_count);
    std::vector<float> sum_y(config.point_count);
    std::vector<float> sum_w(config.point_count);

    for (int iteration = 0; iteration < config.iterations; ++iteration) {
        std::fill(sum_x.begin(), sum_x.end(), 0.0f);
        std::fill(sum_y.begin(), sum_y.end(), 0.0f);
        std::fill(sum_w.begin(), sum_w.end(), 0.0f);

        for (int py = 0; py < image.height; ++py) {
            for (int px = 0; px < image.width; ++px) {
                const int nearest = use_simd
                    ? find_nearest_point_simd(static_cast<float>(px), static_cast<float>(py), points)
                    : find_nearest_point_scalar(static_cast<float>(px), static_cast<float>(py), points);

                const std::size_t pixel_index = static_cast<std::size_t>(py) * image.width + px;
                const float weight = image.density[pixel_index];
                sum_x[nearest] += static_cast<float>(px) * weight;
                sum_y[nearest] += static_cast<float>(py) * weight;
                sum_w[nearest] += weight;
            }
        }

        float total_movement = 0.0f;
        for (std::size_t i = 0; i < config.point_count; ++i) {
            if (sum_w[i] <= 0.0f) {
                continue;
            }

            const float new_x = std::clamp(sum_x[i] / sum_w[i], 0.0f, static_cast<float>(image.width - 1));
            const float new_y = std::clamp(sum_y[i] / sum_w[i], 0.0f, static_cast<float>(image.height - 1));
            total_movement += std::sqrt(squared_distance(points.x[i], points.y[i], new_x, new_y));
            points.x[i] = new_x;
            points.y[i] = new_y;
        }

        result.completed_iterations = iteration + 1;
        result.final_average_movement = total_movement / static_cast<float>(config.point_count);

        if (callback) {
            callback(iteration, points);
        }

        if (result.final_average_movement < config.epsilon) {
            break;
        }
    }

    result.points = std::move(points);
    return result;
}

} // namespace stipple
