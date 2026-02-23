#include "domain/materials/image_texture.h"
#include "domain/hit_record.h"
#include <cmath>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace nwave {

ImageTexture::ImageTexture(std::vector<uint8_t> pixels, int width, int height)
    : pixels_(std::move(pixels)), width_(width), height_(height) {}

bool ImageTexture::scatter(const Ray& /*ray_in*/, const HitRecord& rec,
                           Color3& attenuation, Ray& scattered) const {
    // Sample texture at UV coordinates
    double u = rec.u;
    double v = rec.v;

    // Clamp UV to [0,1]
    u = u - std::floor(u);
    v = v - std::floor(v);

    int px = static_cast<int>(u * width_) % width_;
    int py = static_cast<int>(v * height_) % height_;
    if (px < 0) px += width_;
    if (py < 0) py += height_;

    int idx = (py * width_ + px) * 4;
    double r = pixels_[idx] / 255.0;
    double g = pixels_[idx + 1] / 255.0;
    double b = pixels_[idx + 2] / 255.0;

    attenuation = Color3(r, g, b);

    // Lambertian scatter
    Vec3 scatter_dir = rec.normal + random_unit_vector();
    if (scatter_dir.near_zero()) {
        scatter_dir = rec.normal;
    }
    scattered = Ray(rec.point, scatter_dir);
    return true;
}

std::unique_ptr<ImageTexture> ImageTexture::load_from_file(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4); // force RGBA
    if (!data) {
        throw std::runtime_error("Failed to load texture image: " + path);
    }

    std::vector<uint8_t> pixels(data, data + width * height * 4);
    stbi_image_free(data);

    return std::make_unique<ImageTexture>(std::move(pixels), width, height);
}

} // namespace nwave
