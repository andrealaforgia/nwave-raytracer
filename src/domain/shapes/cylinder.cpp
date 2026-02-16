#include "domain/shapes/cylinder.h"
#include <cmath>

namespace nwave {

Cylinder::Cylinder(Point3 center, double radius, double height, const Material* material)
    : center_(center), radius_(radius), height_(height), material_(material) {}

bool Cylinder::hit_body(const Ray& ray, double t_min, double t_max,
                         double& t_out, HitRecord& rec) const {
    // Cylinder body: x^2 + z^2 = r^2 (relative to center)
    double ox = ray.origin().x() - center_.x();
    double oz = ray.origin().z() - center_.z();
    double dx = ray.direction().x();
    double dz = ray.direction().z();

    double a = dx * dx + dz * dz;
    double b = 2.0 * (ox * dx + oz * dz);
    double c = ox * ox + oz * oz - radius_ * radius_;

    double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0) return false;

    double sqrt_disc = std::sqrt(discriminant);
    double inv_2a = 1.0 / (2.0 * a);

    for (int i = 0; i < 2; ++i) {
        double t = (-b + (i == 0 ? -sqrt_disc : sqrt_disc)) * inv_2a;
        if (t < t_min || t > t_max) continue;

        double y = ray.origin().y() + t * ray.direction().y();
        double y_min = center_.y();
        double y_max = center_.y() + height_;

        if (y >= y_min && y <= y_max) {
            t_out = t;
            rec.t = t;
            rec.point = ray.at(t);
            rec.material = material_;
            // Outward normal on cylinder body (no y component)
            Vec3 outward_normal = Vec3(rec.point.x() - center_.x(), 0,
                                        rec.point.z() - center_.z()) / radius_;
            rec.set_face_normal(ray, outward_normal);
            return true;
        }
    }
    return false;
}

bool Cylinder::hit_cap(const Ray& ray, double t_min, double t_max,
                        double y, double& t_out, HitRecord& rec) const {
    double dy = ray.direction().y();
    if (std::fabs(dy) < 1e-8) return false;

    double t = (y - ray.origin().y()) / dy;
    if (t < t_min || t > t_max) return false;

    Point3 p = ray.at(t);
    double dx = p.x() - center_.x();
    double dz = p.z() - center_.z();
    if (dx * dx + dz * dz > radius_ * radius_) return false;

    t_out = t;
    rec.t = t;
    rec.point = p;
    rec.material = material_;
    Vec3 outward_normal = (y > center_.y()) ? Vec3(0, 1, 0) : Vec3(0, -1, 0);
    rec.set_face_normal(ray, outward_normal);
    return true;
}

bool Cylinder::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    bool hit_anything = false;
    double closest = t_max;
    HitRecord temp_rec;
    double t;

    // Check body
    if (hit_body(ray, t_min, closest, t, temp_rec)) {
        hit_anything = true;
        closest = t;
        rec = temp_rec;
    }

    // Check bottom cap
    if (hit_cap(ray, t_min, closest, center_.y(), t, temp_rec)) {
        hit_anything = true;
        closest = t;
        rec = temp_rec;
    }

    // Check top cap
    if (hit_cap(ray, t_min, closest, center_.y() + height_, t, temp_rec)) {
        hit_anything = true;
        rec = temp_rec;
    }

    return hit_anything;
}

} // namespace nwave
