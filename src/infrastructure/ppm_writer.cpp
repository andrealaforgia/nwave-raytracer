#include "infrastructure/ppm_writer.h"
#include "core/math_utils.h"
#include <fstream>
#include <algorithm>

namespace nwave {

void write_ppm(const std::string& filename, const std::vector<Color3>& pixels,
               int width, int height) {
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int i = 0; i < width * height; ++i) {
        int r = static_cast<int>(255.999 * clamp(pixels[i].r(), 0.0, 1.0));
        int g = static_cast<int>(255.999 * clamp(pixels[i].g(), 0.0, 1.0));
        int b = static_cast<int>(255.999 * clamp(pixels[i].b(), 0.0, 1.0));
        file << r << " " << g << " " << b << "\n";
    }
}

} // namespace nwave
