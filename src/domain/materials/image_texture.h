#ifndef NWAVE_DOMAIN_MATERIALS_IMAGE_TEXTURE_H
#define NWAVE_DOMAIN_MATERIALS_IMAGE_TEXTURE_H

#include "domain/materials/material.h"
#include "domain/hit_record.h"
#include <cstdint>
#include <string>
#include <vector>

namespace nwave {

class ImageTexture : public Material {
public:
    ImageTexture(std::vector<uint8_t> pixels, int width, int height);

    bool scatter(const Ray& ray_in, const HitRecord& rec,
                Color3& attenuation, Ray& scattered) const override;

    const std::vector<uint8_t>& pixels() const { return pixels_; }
    int width() const { return width_; }
    int height() const { return height_; }
    float texture_scale() const { return texture_scale_; }
    void set_texture_scale(float scale) { texture_scale_ = scale; }

    static std::unique_ptr<ImageTexture> load_from_file(const std::string& path);

private:
    std::vector<uint8_t> pixels_;  // RGBA, 4 bytes per pixel
    int width_;
    int height_;
    float texture_scale_{0.0f};  // 0 = full coverage, >0 = scaled down
};

} // namespace nwave

#endif // NWAVE_DOMAIN_MATERIALS_IMAGE_TEXTURE_H
