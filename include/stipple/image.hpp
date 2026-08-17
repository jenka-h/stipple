#pragma once

#include <string>
#include <vector>

namespace stipple {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<float> density;
};

Image load_density_image(const std::string& path);

} // namespace stipple
