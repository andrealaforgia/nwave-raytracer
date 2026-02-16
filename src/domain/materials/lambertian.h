#ifndef NWAVE_DOMAIN_MATERIALS_LAMBERTIAN_H
#define NWAVE_DOMAIN_MATERIALS_LAMBERTIAN_H

#include "domain/materials/material.h"
#include "domain/hit_record.h"

namespace nwave {

class Lambertian : public Material {
public:
    explicit Lambertian(const Color3& albedo);

    bool scatter(const Ray& ray_in, const HitRecord& rec,
                Color3& attenuation, Ray& scattered) const override;

    const Color3& albedo() const { return albedo_; }

private:
    Color3 albedo_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_MATERIALS_LAMBERTIAN_H
