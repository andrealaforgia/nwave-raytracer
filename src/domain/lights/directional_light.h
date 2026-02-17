#ifndef NWAVE_DOMAIN_LIGHTS_DIRECTIONAL_LIGHT_H
#define NWAVE_DOMAIN_LIGHTS_DIRECTIONAL_LIGHT_H

#include "domain/lights/light.h"

namespace nwave {

class DirectionalLight : public Light {
public:
    DirectionalLight(Vec3 direction, Color3 color, double intensity);

    LightSample sample(const Point3& hit_point) const override;

private:
    Vec3 direction_; // direction the light travels (away from source)
    Color3 color_;
    double intensity_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_LIGHTS_DIRECTIONAL_LIGHT_H
