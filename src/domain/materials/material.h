#ifndef NWAVE_DOMAIN_MATERIALS_MATERIAL_H
#define NWAVE_DOMAIN_MATERIALS_MATERIAL_H

#include "core/ray.h"
#include "core/vec3.h"

namespace nwave {

struct HitRecord;

class Material {
public:
    virtual ~Material() = default;

    virtual bool scatter(const Ray& ray_in, const HitRecord& rec,
                        Color3& attenuation, Ray& scattered) const = 0;

    virtual Color3 emitted() const { return Color3(0, 0, 0); }
};

} // namespace nwave

#endif // NWAVE_DOMAIN_MATERIALS_MATERIAL_H
