#include "domain/shapes/plane.h"
#include <cmath>

namespace nwave {

Plane::Plane(Point3 point, Vec3 normal, const Material* mat)
    : point_(point), normal_(normalize(normal)), material_(mat) {}

bool Plane::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    double denom = dot(normal_, ray.direction());

    if (std::fabs(denom) < 1e-8)
        return false;

    double t = dot(normal_, point_ - ray.origin()) / denom;

    if (t < t_min || t > t_max)
        return false;

    rec.t = t;
    rec.point = ray.at(t);
    rec.set_face_normal(ray, normal_);
    rec.material = material_;

    return true;
}

} // namespace nwave
