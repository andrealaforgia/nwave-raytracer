#include "core/aabb.h"
#include <algorithm>
#include <cmath>

namespace nwave {

AABB::AABB(const Point3& min, const Point3& max) {
    minimum_ = Point3(
        std::fmin(min.x(), max.x()),
        std::fmin(min.y(), max.y()),
        std::fmin(min.z(), max.z())
    );
    maximum_ = Point3(
        std::fmax(min.x(), max.x()),
        std::fmax(min.y(), max.y()),
        std::fmax(min.z(), max.z())
    );
}

bool AABB::hit(const Ray& ray, double t_min, double t_max) const {
    for (int axis = 0; axis < 3; ++axis) {
        double inv_d = 1.0 / ray.direction()[axis];
        double t0 = (minimum_[axis] - ray.origin()[axis]) * inv_d;
        double t1 = (maximum_[axis] - ray.origin()[axis]) * inv_d;
        if (inv_d < 0.0)
            std::swap(t0, t1);
        t_min = t0 > t_min ? t0 : t_min;
        t_max = t1 < t_max ? t1 : t_max;
        if (t_max <= t_min)
            return false;
    }
    return true;
}

AABB AABB::surrounding_box(const AABB& a, const AABB& b) {
    Point3 small(
        std::fmin(a.minimum().x(), b.minimum().x()),
        std::fmin(a.minimum().y(), b.minimum().y()),
        std::fmin(a.minimum().z(), b.minimum().z())
    );
    Point3 big(
        std::fmax(a.maximum().x(), b.maximum().x()),
        std::fmax(a.maximum().y(), b.maximum().y()),
        std::fmax(a.maximum().z(), b.maximum().z())
    );
    return AABB(small, big);
}

int AABB::longest_axis() const {
    double x_extent = maximum_.x() - minimum_.x();
    double y_extent = maximum_.y() - minimum_.y();
    double z_extent = maximum_.z() - minimum_.z();
    if (x_extent >= y_extent && x_extent >= z_extent) return 0;
    if (y_extent >= z_extent) return 1;
    return 2;
}

} // namespace nwave
