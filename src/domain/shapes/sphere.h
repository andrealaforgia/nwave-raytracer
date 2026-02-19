#ifndef NWAVE_DOMAIN_SHAPES_SPHERE_H
#define NWAVE_DOMAIN_SHAPES_SPHERE_H

#include "domain/shapes/shape.h"

namespace nwave {

class Sphere : public Shape {
public:
    Sphere(Point3 center, double radius, const Material* mat);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;

    const Point3& center() const { return center_; }
    double radius() const { return radius_; }
    const Material* material() const { return material_; }

private:
    Point3 center_;
    double radius_;
    const Material* material_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_SPHERE_H
