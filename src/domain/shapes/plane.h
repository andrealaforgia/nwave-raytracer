#ifndef NWAVE_DOMAIN_SHAPES_PLANE_H
#define NWAVE_DOMAIN_SHAPES_PLANE_H

#include "domain/shapes/shape.h"

namespace nwave {

class Plane : public Shape {
public:
    Plane(Point3 point, Vec3 normal, const Material* mat);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;

    const Point3& point() const { return point_; }
    const Vec3& normal() const { return normal_; }

private:
    Point3 point_;
    Vec3 normal_;
    const Material* material_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_PLANE_H
