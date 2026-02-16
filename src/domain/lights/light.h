#ifndef NWAVE_DOMAIN_LIGHTS_LIGHT_H
#define NWAVE_DOMAIN_LIGHTS_LIGHT_H

#include "core/vec3.h"

namespace nwave {

struct LightSample {
    Vec3 direction;    // Direction FROM hit point TOWARD the light
    double distance;   // Distance to the light (for shadow ray t_max)
    Color3 intensity;  // Light color * intensity
};

class Light {
public:
    virtual ~Light() = default;

    virtual LightSample sample(const Point3& hit_point) const = 0;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_LIGHTS_LIGHT_H
