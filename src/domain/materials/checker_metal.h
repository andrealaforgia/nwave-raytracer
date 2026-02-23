#ifndef NWAVE_DOMAIN_MATERIALS_CHECKER_METAL_H
#define NWAVE_DOMAIN_MATERIALS_CHECKER_METAL_H

#include "domain/materials/material.h"
#include "domain/hit_record.h"

namespace nwave {

class CheckerMetal : public Material {
public:
    CheckerMetal(const Color3& color_a, const Color3& color_b,
                 double fuzziness, double scale);

    bool scatter(const Ray& ray_in, const HitRecord& rec,
                Color3& attenuation, Ray& scattered) const override;

    const Color3& color_a() const { return color_a_; }
    const Color3& color_b() const { return color_b_; }
    double fuzziness() const { return fuzziness_; }
    double scale() const { return scale_; }

private:
    Color3 color_a_;
    Color3 color_b_;
    double fuzziness_;
    double scale_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_MATERIALS_CHECKER_METAL_H
