#include "stipple/renderer.hpp"

#include <algorithm>
#include <stdexcept>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace stipple {

namespace {

cv::Mat render_points_to_mat(const Points& points, int width, int height) {
    cv::Mat canvas(height, width, CV_8UC3, cv::Scalar(255, 255, 255));

    for (std::size_t i = 0; i < points.size(); ++i) {
        const int x = std::clamp(static_cast<int>(points.x[i]), 0, width - 1);
        const int y = std::clamp(static_cast<int>(points.y[i]), 0, height - 1);
        cv::circle(canvas, cv::Point(x, y), 1, cv::Scalar(0, 0, 0), cv::FILLED, cv::LINE_AA);
    }

    return canvas;
}

} // namespace

void render_stipple_image(const Points& points, int width, int height, const std::string& output_path) {
    cv::Mat canvas = render_points_to_mat(points, width, height);
    if (!cv::imwrite(output_path, canvas)) {
        throw std::runtime_error("Failed to write stipple image: " + output_path);
    }
}

void render_stipple_over_image(const Image& image, const Points& points, const std::string& output_path) {
    cv::Mat canvas = render_points_to_mat(points, image.width, image.height);
    if (!cv::imwrite(output_path, canvas)) {
        throw std::runtime_error("Failed to write stipple image: " + output_path);
    }
}

} // namespace stipple
