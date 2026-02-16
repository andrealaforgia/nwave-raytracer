#include "domain/materials/lambertian.h"

namespace nwave {

Lambertian::Lambertian(const Color3& albedo)
    : albedo_(albedo) {}

bool Lambertian::scatter(const Ray& /*ray_in*/, const HitRecord& rec,
                        Color3& attenuation, Ray& scattered) const {
    Vec3 scatter_direction = rec.normal + random_unit_vector();

    if (scatter_direction.near_zero())
        scatter_direction = rec.normal;

    scattered = Ray(rec.point, scatter_direction);
    attenuation = albedo_;
    return true;
}

} // namespace nwave
