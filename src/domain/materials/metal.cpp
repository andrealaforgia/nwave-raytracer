#include "domain/materials/metal.h"
#include "core/math_utils.h"

namespace nwave {

Metal::Metal(const Color3& albedo, double fuzziness)
    : albedo_(albedo), fuzziness_(clamp(fuzziness, 0.0, 1.0)) {}

bool Metal::scatter(const Ray& ray_in, const HitRecord& rec,
                    Color3& attenuation, Ray& scattered) const {
    Vec3 reflected = reflect(normalize(ray_in.direction()), rec.normal);
    scattered = Ray(rec.point, reflected + fuzziness_ * random_in_unit_sphere());
    attenuation = albedo_;
    return (dot(scattered.direction(), rec.normal) > 0);
}

} // namespace nwave
