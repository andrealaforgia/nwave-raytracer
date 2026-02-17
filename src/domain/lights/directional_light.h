#ifndef NWAVE_DOMAIN_LIGHTS_DIRECTIONAL_LIGHT_H
#define NWAVE_DOMAIN_LIGHTS_DIRECTIONAL_LIGHT_H

#include "domain/lights/light.h"

namespace nwave {

class DirectionalLight : public Light {
public:
    DirectionalLight(Vec3 direction, Color3 color, double intensity);

    LightSample sample(const Point3& hit_point) const override;

    const Vec3& direction() const { return direction_; }
    const Color3& color() const { return color_; }
    double intensity() const { return intensity_; }

private:
    Vec3 direction_; // direction the light travels (away from source)
    Color3 color_;
    double intensity_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_LIGHTS_DIRECTIONAL_LIGHT_H
