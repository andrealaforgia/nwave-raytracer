#include "infrastructure/ppm_writer.h"
#include "core/math_utils.h"
#include <fstream>
#include <sstream>

namespace nwave {

void write_ppm(const std::string& filename, const std::vector<Color3>& pixels,
               int width, int height) {
    std::ofstream file(filename, std::ios::binary);

    // Write P6 header as text, then binary pixel data
    std::ostringstream header;
    header << "P6\n" << width << " " << height << "\n255\n";
    std::string hdr = header.str();
    file.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));

    // Write RGB bytes directly (3 bytes per pixel)
    int pixel_count = width * height;
    std::vector<uint8_t> rgb(pixel_count * 3);
    for (int i = 0; i < pixel_count; ++i) {
        rgb[i * 3 + 0] = static_cast<uint8_t>(255.999 * clamp(pixels[i].r(), 0.0, 1.0));
        rgb[i * 3 + 1] = static_cast<uint8_t>(255.999 * clamp(pixels[i].g(), 0.0, 1.0));
        rgb[i * 3 + 2] = static_cast<uint8_t>(255.999 * clamp(pixels[i].b(), 0.0, 1.0));
    }
    file.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
}

} // namespace nwave
