#include "domain/lights/point_light.h"

namespace nwave {

PointLight::PointLight(Point3 position, Color3 color, double intensity)
    : position_(position), color_(color), intensity_(intensity) {}

LightSample PointLight::sample(const Point3& hit_point) const {
    Vec3 to_light = position_ - hit_point;
    double dist = to_light.length();
    Vec3 direction = normalize(to_light);
    Color3 light_intensity = color_ * intensity_;

    return LightSample{direction, dist, light_intensity};
}

} // namespace nwave
