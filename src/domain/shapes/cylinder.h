#ifndef NWAVE_DOMAIN_SHAPES_CYLINDER_H
#define NWAVE_DOMAIN_SHAPES_CYLINDER_H

#include "domain/shapes/shape.h"

namespace nwave {

// Finite cylinder aligned along the Y axis, with caps
class Cylinder : public Shape {
public:
    Cylinder(Point3 center, double radius, double height, const Material* material);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;

    const Point3& center() const { return center_; }
    double radius() const { return radius_; }
    double height() const { return height_; }

private:
    Point3 center_; // center of the base
    double radius_;
    double height_;
    const Material* material_;

    bool hit_body(const Ray& ray, double t_min, double t_max, double& t_out, HitRecord& rec) const;
    bool hit_cap(const Ray& ray, double t_min, double t_max, double y, double& t_out, HitRecord& rec) const;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_CYLINDER_H
