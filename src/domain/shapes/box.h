#ifndef NWAVE_DOMAIN_SHAPES_BOX_H
#define NWAVE_DOMAIN_SHAPES_BOX_H

#include "domain/shapes/shape.h"

namespace nwave {

class Box : public Shape {
public:
    Box(Point3 min, Point3 max, const Material* mat);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;

    const Point3& box_min() const { return box_min_; }
    const Point3& box_max() const { return box_max_; }

private:
    Point3 box_min_;
    Point3 box_max_;
    const Material* material_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_BOX_H
