#include "stipple/animation.hpp"

#include <algorithm>
#include <stdexcept>

#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>

namespace stipple {

struct StippleAnimationWriter::Impl {
    cv::VideoWriter writer;
    int width = 0;
    int height = 0;
};

StippleAnimationWriter::StippleAnimationWriter(const std::string& output_path, int width, int height, double fps)
    : impl_(new Impl{}) {
    impl_->width = width;
    impl_->height = height;

    const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    impl_->writer.open(output_path, fourcc, fps, cv::Size(width, height), true);
    if (!impl_->writer.isOpened()) {
        delete impl_;
        impl_ = nullptr;
        throw std::runtime_error("Failed to open animation output: " + output_path);
    }
}

StippleAnimationWriter::~StippleAnimationWriter() {
    close();
    delete impl_;
}

StippleAnimationWriter::StippleAnimationWriter(StippleAnimationWriter&& other) noexcept
    : impl_(other.impl_) {
    other.impl_ = nullptr;
}

StippleAnimationWriter& StippleAnimationWriter::operator=(StippleAnimationWriter&& other) noexcept {
    if (this != &other) {
        close();
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

void StippleAnimationWriter::write_frame(const Points& points) {
    if (!impl_ || !impl_->writer.isOpened()) {
        return;
    }

    cv::Mat frame(impl_->height, impl_->width, CV_8UC3, cv::Scalar(255, 255, 255));
    for (std::size_t i = 0; i < points.size(); ++i) {
        const int x = std::clamp(static_cast<int>(points.x[i]), 0, impl_->width - 1);
        const int y = std::clamp(static_cast<int>(points.y[i]), 0, impl_->height - 1);
        cv::circle(frame, cv::Point(x, y), 1, cv::Scalar(0, 0, 0), cv::FILLED, cv::LINE_AA);
    }

    impl_->writer.write(frame);
}

void StippleAnimationWriter::close() {
    if (impl_ && impl_->writer.isOpened()) {
        impl_->writer.release();
    }
}

} // namespace stipple
