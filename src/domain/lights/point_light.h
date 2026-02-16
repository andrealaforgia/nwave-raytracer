#ifndef NWAVE_DOMAIN_LIGHTS_POINT_LIGHT_H
#define NWAVE_DOMAIN_LIGHTS_POINT_LIGHT_H

#include "domain/lights/light.h"

namespace nwave {

class PointLight : public Light {
public:
    PointLight(Point3 position, Color3 color, double intensity);

    LightSample sample(const Point3& hit_point) const override;

    const Point3& position() const { return position_; }
    const Color3& color() const { return color_; }
    double intensity() const { return intensity_; }

private:
    Point3 position_;
    Color3 color_;
    double intensity_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_LIGHTS_POINT_LIGHT_H
