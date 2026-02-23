#include "domain/materials/checker_metal.h"
#include "core/vec3.h"
#include <cmath>

namespace nwave {

CheckerMetal::CheckerMetal(const Color3& color_a, const Color3& color_b,
                           double fuzziness, double scale)
    : color_a_(color_a), color_b_(color_b),
      fuzziness_(fuzziness), scale_(scale) {}

bool CheckerMetal::scatter(const Ray& ray_in, const HitRecord& rec,
                           Color3& attenuation, Ray& scattered) const {
    // 2D checker pattern (X+Z) to avoid boundary artifacts on axis-aligned surfaces
    int ix = static_cast<int>(std::floor(rec.point.x() * scale_));
    int iz = static_cast<int>(std::floor(rec.point.z() * scale_));
    bool is_even = (ix + iz) % 2 == 0;

    attenuation = is_even ? color_a_ : color_b_;

    Vec3 reflected = reflect(normalize(ray_in.direction()), rec.normal);
    scattered = Ray(rec.point, reflected + fuzziness_ * random_in_unit_sphere());
    return dot(scattered.direction(), rec.normal) > 0;
}

} // namespace nwave
