#include "stipple/logic/openmp.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "stipple/logic/serial.hpp"
#include "stipple/logic/simd.hpp"

#ifdef STIPPLE_WITH_OPENMP
#include <omp.h>
#endif

namespace stipple {

LloydResult run_lloyd_openmp(const Image& image, const LloydConfig& config, IterationCallback callback) {
#ifndef STIPPLE_WITH_OPENMP
    return run_lloyd_serial(image, config, std::move(callback));
#else
    Points points = initialize_points_uniform(image.width, image.height, config.point_count, config.seed);
    const bool use_simd = config.backend == Backend::OpenMPSIMD;
    const int point_count = static_cast<int>(config.point_count);
    const int pixel_count = image.width * image.height;
    const int thread_count = omp_get_max_threads();

    std::vector<float> thread_sum_x(static_cast<std::size_t>(thread_count) * point_count);
    std::vector<float> thread_sum_y(static_cast<std::size_t>(thread_count) * point_count);
    std::vector<float> thread_sum_w(static_cast<std::size_t>(thread_count) * point_count);
    std::vector<float> sum_x(config.point_count);
    std::vector<float> sum_y(config.point_count);
    std::vector<float> sum_w(config.point_count);

    LloydResult result;

    for (int iteration = 0; iteration < config.iterations; ++iteration) {
        std::fill(thread_sum_x.begin(), thread_sum_x.end(), 0.0f);
        std::fill(thread_sum_y.begin(), thread_sum_y.end(), 0.0f);
        std::fill(thread_sum_w.begin(), thread_sum_w.end(), 0.0f);

        #pragma omp parallel
        {
            const int tid = omp_get_thread_num();
            float* local_x = thread_sum_x.data() + static_cast<std::size_t>(tid) * point_count;
            float* local_y = thread_sum_y.data() + static_cast<std::size_t>(tid) * point_count;
            float* local_w = thread_sum_w.data() + static_cast<std::size_t>(tid) * point_count;

            #pragma omp for schedule(static)
            for (int pixel = 0; pixel < pixel_count; ++pixel) {
                const int px = pixel % image.width;
                const int py = pixel / image.width;
                const int nearest = use_simd
                    ? find_nearest_point_simd(static_cast<float>(px), static_cast<float>(py), points)
                    : find_nearest_point_scalar(static_cast<float>(px), static_cast<float>(py), points);

                const float weight = image.density[static_cast<std::size_t>(pixel)];
                local_x[nearest] += static_cast<float>(px) * weight;
                local_y[nearest] += static_cast<float>(py) * weight;
                local_w[nearest] += weight;
            }
        }

        std::fill(sum_x.begin(), sum_x.end(), 0.0f);
        std::fill(sum_y.begin(), sum_y.end(), 0.0f);
        std::fill(sum_w.begin(), sum_w.end(), 0.0f);

        for (int tid = 0; tid < thread_count; ++tid) {
            const std::size_t offset = static_cast<std::size_t>(tid) * point_count;
            for (int i = 0; i < point_count; ++i) {
                sum_x[i] += thread_sum_x[offset + i];
                sum_y[i] += thread_sum_y[offset + i];
                sum_w[i] += thread_sum_w[offset + i];
            }
        }

        float total_movement = 0.0f;
        for (int i = 0; i < point_count; ++i) {
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
#endif
}

} // namespace stipple
