#ifndef NWAVE_DOMAIN_MATERIALS_EMISSIVE_H
#define NWAVE_DOMAIN_MATERIALS_EMISSIVE_H

#include "domain/materials/material.h"

namespace nwave {

class Emissive : public Material {
public:
    explicit Emissive(const Color3& color, double intensity = 1.0);

    bool scatter(const Ray& ray_in, const HitRecord& rec,
                Color3& attenuation, Ray& scattered) const override;

    Color3 emitted() const override;

    const Color3& color() const { return color_; }
    double intensity() const { return intensity_; }

private:
    Color3 color_;
    double intensity_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_MATERIALS_EMISSIVE_H
