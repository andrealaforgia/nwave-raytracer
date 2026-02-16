#include "domain/shapes/box.h"
#include <algorithm>
#include <cmath>

namespace nwave {

Box::Box(Point3 min, Point3 max, const Material* mat)
    : box_min_(
          std::fmin(min.x(), max.x()),
          std::fmin(min.y(), max.y()),
          std::fmin(min.z(), max.z())
      ),
      box_max_(
          std::fmax(min.x(), max.x()),
          std::fmax(min.y(), max.y()),
          std::fmax(min.z(), max.z())
      ),
      material_(mat) {}

bool Box::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    double t_enter = t_min;
    double t_exit = t_max;
    int enter_axis = -1;
    int enter_sign = 0;

    for (int axis = 0; axis < 3; ++axis) {
        double inv_d = 1.0 / ray.direction()[axis];
        double t0 = (box_min_[axis] - ray.origin()[axis]) * inv_d;
        double t1 = (box_max_[axis] - ray.origin()[axis]) * inv_d;

        if (inv_d < 0.0)
            std::swap(t0, t1);

        if (t0 > t_enter) {
            t_enter = t0;
            enter_axis = axis;
            enter_sign = (inv_d >= 0.0) ? -1 : 1;
        }
        if (t1 < t_exit) {
            t_exit = t1;
        }
        if (t_exit <= t_enter)
            return false;
    }

    // Determine which t to use and compute the normal
    double hit_t;
    Vec3 outward_normal;

    if (enter_axis >= 0 && t_enter >= t_min && t_enter <= t_max) {
        // Ray hits from outside: use entry point
        hit_t = t_enter;
        outward_normal = Vec3(0, 0, 0);
        outward_normal[enter_axis] = static_cast<double>(enter_sign);
    } else if (t_exit >= t_min && t_exit <= t_max) {
        // Ray origin inside box: use exit point
        hit_t = t_exit;
        // For exit, determine which axis/face
        // Recompute to find exit axis
        int exit_axis = -1;
        int exit_sign = 0;
        double closest_exit = t_max;
        for (int axis = 0; axis < 3; ++axis) {
            double inv_d = 1.0 / ray.direction()[axis];
            double t0 = (box_min_[axis] - ray.origin()[axis]) * inv_d;
            double t1 = (box_max_[axis] - ray.origin()[axis]) * inv_d;
            if (inv_d < 0.0)
                std::swap(t0, t1);
            if (t1 <= closest_exit) {
                closest_exit = t1;
                exit_axis = axis;
                exit_sign = (inv_d >= 0.0) ? 1 : -1;
            }
        }
        outward_normal = Vec3(0, 0, 0);
        outward_normal[exit_axis] = static_cast<double>(exit_sign);
    } else {
        return false;
    }

    rec.t = hit_t;
    rec.point = ray.at(hit_t);
    rec.material = material_;
    rec.set_face_normal(ray, outward_normal);

    return true;
}

} // namespace nwave
