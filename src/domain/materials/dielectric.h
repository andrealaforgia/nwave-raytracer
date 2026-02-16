#ifndef NWAVE_DOMAIN_MATERIALS_DIELECTRIC_H
#define NWAVE_DOMAIN_MATERIALS_DIELECTRIC_H

#include "domain/materials/material.h"
#include "domain/hit_record.h"

namespace nwave {

class Dielectric : public Material {
public:
    explicit Dielectric(double index_of_refraction, const Color3& tint = Color3(1.0, 1.0, 1.0));

    bool scatter(const Ray& ray_in, const HitRecord& rec,
                Color3& attenuation, Ray& scattered) const override;

    double ior() const { return index_of_refraction_; }
    const Color3& tint() const { return tint_; }

private:
    double index_of_refraction_;
    Color3 tint_;

    static double schlick_reflectance(double cosine, double ref_idx);
};

} // namespace nwave

#endif // NWAVE_DOMAIN_MATERIALS_DIELECTRIC_H
