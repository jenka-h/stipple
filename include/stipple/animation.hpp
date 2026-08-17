#pragma once

#include <string>

#include "stipple/point.hpp"

namespace stipple {

class StippleAnimationWriter {
public:
    StippleAnimationWriter(const std::string& output_path, int width, int height, double fps);
    ~StippleAnimationWriter();

    StippleAnimationWriter(const StippleAnimationWriter&) = delete;
    StippleAnimationWriter& operator=(const StippleAnimationWriter&) = delete;

    StippleAnimationWriter(StippleAnimationWriter&&) noexcept;
    StippleAnimationWriter& operator=(StippleAnimationWriter&&) noexcept;

    void write_frame(const Points& points);
    void close();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace stipple
