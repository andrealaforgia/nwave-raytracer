#include "domain/lights/directional_light.h"
#include "core/math_utils.h"

namespace nwave {

DirectionalLight::DirectionalLight(Vec3 direction, Color3 color, double intensity)
    : direction_(normalize(direction)), color_(color), intensity_(intensity) {}

LightSample DirectionalLight::sample(const Point3& /*hit_point*/) const {
    LightSample ls;
    ls.direction = -direction_;          // toward the light (opposite of light travel)
    ls.distance = infinity;              // infinitely far away
    ls.intensity = intensity_ * color_;
    return ls;
}

} // namespace nwave
