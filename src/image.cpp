#include "stipple/image.hpp"

#include <algorithm>
#include <stdexcept>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace stipple {

Image load_density_image(const std::string& path) {
    cv::Mat bgr = cv::imread(path, cv::IMREAD_COLOR);
    if (bgr.empty()) {
        throw std::runtime_error("Failed to read image: " + path);
    }

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    Image image;
    image.width = gray.cols;
    image.height = gray.rows;
    image.density.resize(static_cast<std::size_t>(image.width) * image.height);

    for (int y = 0; y < image.height; ++y) {
        const unsigned char* row = gray.ptr<unsigned char>(y);
        for (int x = 0; x < image.width; ++x) {
            const float grayscale = static_cast<float>(row[x]) / 255.0f;
            image.density[static_cast<std::size_t>(y) * image.width + x] = std::max(1.0f - grayscale, 0.001f);
        }
    }

    return image;
}


} // namespace stipple
