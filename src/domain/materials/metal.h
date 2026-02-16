#ifndef NWAVE_DOMAIN_MATERIALS_METAL_H
#define NWAVE_DOMAIN_MATERIALS_METAL_H

#include "domain/materials/material.h"
#include "domain/hit_record.h"

namespace nwave {

class Metal : public Material {
public:
    Metal(const Color3& albedo, double fuzziness);

    bool scatter(const Ray& ray_in, const HitRecord& rec,
                Color3& attenuation, Ray& scattered) const override;

    const Color3& albedo() const { return albedo_; }
    double fuzziness() const { return fuzziness_; }

private:
    Color3 albedo_;
    double fuzziness_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_MATERIALS_METAL_H
