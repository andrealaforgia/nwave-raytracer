#ifndef NWAVE_DOMAIN_SHAPES_TRIANGLE_H
#define NWAVE_DOMAIN_SHAPES_TRIANGLE_H

#include "domain/shapes/shape.h"

namespace nwave {

class Triangle : public Shape {
public:
    Triangle(Point3 v0, Point3 v1, Point3 v2, const Material* mat);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;

    const Point3& v0() const { return v0_; }
    const Point3& v1() const { return v1_; }
    const Point3& v2() const { return v2_; }
    const Material* material() const { return material_; }

private:
    Point3 v0_;
    Point3 v1_;
    Point3 v2_;
    const Material* material_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_TRIANGLE_H
