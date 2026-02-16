#ifndef NWAVE_CORE_AABB_H
#define NWAVE_CORE_AABB_H

#include "vec3.h"
#include "ray.h"
#include <algorithm>

namespace nwave {

class AABB {
public:
    AABB() = default;
    AABB(const Point3& min, const Point3& max);

    bool hit(const Ray& ray, double t_min, double t_max) const;

    static AABB surrounding_box(const AABB& a, const AABB& b);
    int longest_axis() const;

    const Point3& minimum() const { return minimum_; }
    const Point3& maximum() const { return maximum_; }

private:
    Point3 minimum_;
    Point3 maximum_;
};

} // namespace nwave

#endif // NWAVE_CORE_AABB_H
