#include "stipple/logic/cuda.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

namespace stipple {

namespace {

void check_cuda(cudaError_t status, const char* operation) {
    if (status != cudaSuccess) {
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
    }
}

__global__ void accumulate_centroids_kernel(
    int width,
    int height,
    int point_count,
    const float* density,
    const float* point_x,
    const float* point_y,
    float* sum_x,
    float* sum_y,
    float* sum_w) {
    const int pixel = blockIdx.x * blockDim.x + threadIdx.x;
    const int pixel_count = width * height;
    if (pixel >= pixel_count) {
        return;
    }

    const int px = pixel % width;
    const int py = pixel / width;

    float best_distance = 3.402823466e+38F;
    int best_index = 0;

    for (int i = 0; i < point_count; ++i) {
        const float dx = static_cast<float>(px) - point_x[i];
        const float dy = static_cast<float>(py) - point_y[i];
        const float distance = dx * dx + dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
            best_index = i;
        }
    }

    const float weight = density[pixel];
    atomicAdd(&sum_x[best_index], static_cast<float>(px) * weight);
    atomicAdd(&sum_y[best_index], static_cast<float>(py) * weight);
    atomicAdd(&sum_w[best_index], weight);
}

__global__ void update_points_kernel(
    int width,
    int height,
    int point_count,
    float* point_x,
    float* point_y,
    const float* sum_x,
    const float* sum_y,
    const float* sum_w,
    float* movement) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= point_count) {
        return;
    }

    if (sum_w[i] <= 0.0f) {
        movement[i] = 0.0f;
        return;
    }

    float new_x = sum_x[i] / sum_w[i];
    float new_y = sum_y[i] / sum_w[i];
    new_x = fminf(fmaxf(new_x, 0.0f), static_cast<float>(width - 1));
    new_y = fminf(fmaxf(new_y, 0.0f), static_cast<float>(height - 1));

    const float dx = point_x[i] - new_x;
    const float dy = point_y[i] - new_y;
    movement[i] = sqrtf(dx * dx + dy * dy);
    point_x[i] = new_x;
    point_y[i] = new_y;
}

} // namespace

bool is_cuda_available() {
    int device_count = 0;
    return cudaGetDeviceCount(&device_count) == cudaSuccess && device_count > 0;
}

LloydResult run_lloyd_cuda(const Image& image, const LloydConfig& config, IterationCallback callback) {
    if (!is_cuda_available()) {
        throw std::runtime_error("No CUDA-capable device is available.");
    }

    Points points = initialize_points_uniform(image.width, image.height, config.point_count, config.seed);

    const int point_count = static_cast<int>(config.point_count);
    const int pixel_count = image.width * image.height;
    const std::size_t point_bytes = config.point_count * sizeof(float);
    const std::size_t density_bytes = static_cast<std::size_t>(pixel_count) * sizeof(float);

    float* d_density = nullptr;
    float* d_point_x = nullptr;
    float* d_point_y = nullptr;
    float* d_sum_x = nullptr;
    float* d_sum_y = nullptr;
    float* d_sum_w = nullptr;
    float* d_movement = nullptr;

    check_cuda(cudaMalloc(&d_density, density_bytes), "cudaMalloc density");
    check_cuda(cudaMalloc(&d_point_x, point_bytes), "cudaMalloc point_x");
    check_cuda(cudaMalloc(&d_point_y, point_bytes), "cudaMalloc point_y");
    check_cuda(cudaMalloc(&d_sum_x, point_bytes), "cudaMalloc sum_x");
    check_cuda(cudaMalloc(&d_sum_y, point_bytes), "cudaMalloc sum_y");
    check_cuda(cudaMalloc(&d_sum_w, point_bytes), "cudaMalloc sum_w");
    check_cuda(cudaMalloc(&d_movement, point_bytes), "cudaMalloc movement");

    try {
        check_cuda(cudaMemcpy(d_density, image.density.data(), density_bytes, cudaMemcpyHostToDevice), "cudaMemcpy density");
        check_cuda(cudaMemcpy(d_point_x, points.x.data(), point_bytes, cudaMemcpyHostToDevice), "cudaMemcpy point_x");
        check_cuda(cudaMemcpy(d_point_y, points.y.data(), point_bytes, cudaMemcpyHostToDevice), "cudaMemcpy point_y");

        std::vector<float> movement(config.point_count);
        LloydResult result;

        const int threads = 256;
        const int pixel_blocks = (pixel_count + threads - 1) / threads;
        const int point_blocks = (point_count + threads - 1) / threads;

        for (int iteration = 0; iteration < config.iterations; ++iteration) {
            check_cuda(cudaMemset(d_sum_x, 0, point_bytes), "cudaMemset sum_x");
            check_cuda(cudaMemset(d_sum_y, 0, point_bytes), "cudaMemset sum_y");
            check_cuda(cudaMemset(d_sum_w, 0, point_bytes), "cudaMemset sum_w");

            accumulate_centroids_kernel<<<pixel_blocks, threads>>>(
                image.width,
                image.height,
                point_count,
                d_density,
                d_point_x,
                d_point_y,
                d_sum_x,
                d_sum_y,
                d_sum_w);
            check_cuda(cudaGetLastError(), "accumulate_centroids_kernel launch");

            update_points_kernel<<<point_blocks, threads>>>(
                image.width,
                image.height,
                point_count,
                d_point_x,
                d_point_y,
                d_sum_x,
                d_sum_y,
                d_sum_w,
                d_movement);
            check_cuda(cudaGetLastError(), "update_points_kernel launch");
            check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

            check_cuda(cudaMemcpy(movement.data(), d_movement, point_bytes, cudaMemcpyDeviceToHost), "cudaMemcpy movement");

            float total_movement = 0.0f;
            for (float value : movement) {
                total_movement += value;
            }

            result.completed_iterations = iteration + 1;
            result.final_average_movement = total_movement / static_cast<float>(config.point_count);

            if (callback) {
                check_cuda(cudaMemcpy(points.x.data(), d_point_x, point_bytes, cudaMemcpyDeviceToHost), "cudaMemcpy callback point_x");
                check_cuda(cudaMemcpy(points.y.data(), d_point_y, point_bytes, cudaMemcpyDeviceToHost), "cudaMemcpy callback point_y");
                callback(iteration, points);
            }

            if (result.final_average_movement < config.epsilon) {
                break;
            }
        }

        check_cuda(cudaMemcpy(points.x.data(), d_point_x, point_bytes, cudaMemcpyDeviceToHost), "cudaMemcpy final point_x");
        check_cuda(cudaMemcpy(points.y.data(), d_point_y, point_bytes, cudaMemcpyDeviceToHost), "cudaMemcpy final point_y");
        result.points = std::move(points);

        cudaFree(d_density);
        cudaFree(d_point_x);
        cudaFree(d_point_y);
        cudaFree(d_sum_x);
        cudaFree(d_sum_y);
        cudaFree(d_sum_w);
        cudaFree(d_movement);

        return result;
    } catch (...) {
        cudaFree(d_density);
        cudaFree(d_point_x);
        cudaFree(d_point_y);
        cudaFree(d_sum_x);
        cudaFree(d_sum_y);
        cudaFree(d_sum_w);
        cudaFree(d_movement);
        throw;
    }
}

} // namespace stipple
