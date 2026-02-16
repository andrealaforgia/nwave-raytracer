#ifndef NWAVE_DOMAIN_HIT_RECORD_H
#define NWAVE_DOMAIN_HIT_RECORD_H

#include "core/vec3.h"
#include "core/ray.h"

namespace nwave {

// Forward declaration — Material will be implemented in a later step
class Material;

struct HitRecord {
    Point3 point;
    Vec3 normal;
    double t = 0.0;
    double u = 0.0;
    double v = 0.0;
    bool front_face = false;
    const Material* material = nullptr;

    void set_face_normal(const Ray& ray, const Vec3& outward_normal) {
        front_face = dot(ray.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

} // namespace nwave

#endif // NWAVE_DOMAIN_HIT_RECORD_H
