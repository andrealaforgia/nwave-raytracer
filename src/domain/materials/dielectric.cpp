#include "domain/materials/dielectric.h"
#include "core/math_utils.h"
#include <cmath>

namespace nwave {

Dielectric::Dielectric(double index_of_refraction, const Color3& tint)
    : index_of_refraction_(index_of_refraction), tint_(tint) {}

bool Dielectric::scatter(const Ray& ray_in, const HitRecord& rec,
                         Color3& attenuation, Ray& scattered) const {
    attenuation = tint_;
    double eta = rec.front_face ? (1.0 / index_of_refraction_) : index_of_refraction_;

    Vec3 unit_direction = normalize(ray_in.direction());
    double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
    double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

    bool cannot_refract = eta * sin_theta > 1.0;

    if (cannot_refract || schlick_reflectance(cos_theta, eta) > random_double()) {
        Vec3 reflected = reflect(unit_direction, rec.normal);
        scattered = Ray(rec.point, reflected);
    } else {
        Vec3 refracted = refract(unit_direction, rec.normal, eta);
        scattered = Ray(rec.point, refracted);
    }
    return true;
}

double Dielectric::schlick_reflectance(double cosine, double ref_idx) {
    auto r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * std::pow((1.0 - cosine), 5);
}

} // namespace nwave
