#include "domain/shapes/sphere.h"
#include <cmath>

namespace nwave {

Sphere::Sphere(Point3 center, double radius, const Material* mat)
    : center_(center), radius_(radius), material_(mat) {}

bool Sphere::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    Vec3 oc = ray.origin() - center_;
    double a = dot(ray.direction(), ray.direction());
    double half_b = dot(oc, ray.direction());
    double c = dot(oc, oc) - radius_ * radius_;

    double discriminant = half_b * half_b - a * c;
    if (discriminant < 0)
        return false;

    double sqrtd = std::sqrt(discriminant);

    // Find the nearest root that lies in the acceptable range
    double root = (-half_b - sqrtd) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || root > t_max)
            return false;
    }

    rec.t = root;
    rec.point = ray.at(root);
    Vec3 outward_normal = (rec.point - center_) / radius_;
    rec.set_face_normal(ray, outward_normal);
    rec.material = material_;

    return true;
}

} // namespace nwave
