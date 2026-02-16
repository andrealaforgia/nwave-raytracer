#ifndef NWAVE_DOMAIN_SHAPES_SHAPE_H
#define NWAVE_DOMAIN_SHAPES_SHAPE_H

#include "domain/hit_record.h"

namespace nwave {

class Shape {
public:
    virtual ~Shape() = default;

    virtual bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const = 0;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_SHAPE_H
