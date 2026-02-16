#include "domain/shapes/triangle.h"
#include <cmath>

namespace nwave {

Triangle::Triangle(Point3 v0, Point3 v1, Point3 v2, const Material* mat)
    : v0_(v0), v1_(v1), v2_(v2), material_(mat) {}

bool Triangle::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    Vec3 edge1 = v1_ - v0_;
    Vec3 edge2 = v2_ - v0_;
    Vec3 h = cross(ray.direction(), edge2);
    double a = dot(edge1, h);

    if (std::fabs(a) < epsilon)
        return false; // Ray parallel to triangle

    double f = 1.0 / a;
    Vec3 s = ray.origin() - v0_;
    double u = f * dot(s, h);

    if (u < 0.0 || u > 1.0)
        return false;

    Vec3 q = cross(s, edge1);
    double v = f * dot(ray.direction(), q);

    if (v < 0.0 || u + v > 1.0)
        return false;

    double t = f * dot(edge2, q);

    if (t < t_min || t > t_max)
        return false;

    rec.t = t;
    rec.point = ray.at(t);
    rec.u = u;
    rec.v = v;
    Vec3 outward_normal = normalize(cross(edge1, edge2));
    rec.set_face_normal(ray, outward_normal);
    rec.material = material_;

    return true;
}

} // namespace nwave
