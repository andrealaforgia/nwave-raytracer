#include "domain/shapes/transformed_shape.h"

namespace nwave {

TransformedShape::TransformedShape(std::shared_ptr<Shape> inner, const Matrix4x4& transform)
    : inner_(std::move(inner)), transform_(transform), inverse_(transform.inverse()) {}

bool TransformedShape::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    // Transform ray from world space to local space using inverse
    Point3 local_origin = inverse_.transform_point(ray.origin());
    Vec3 local_direction = inverse_.transform_vector(ray.direction());
    Ray local_ray(local_origin, local_direction);

    // Delegate to inner shape in local space
    if (!inner_->hit(local_ray, t_min, t_max, rec)) {
        return false;
    }

    // Transform hit result back to world space
    rec.point = transform_.transform_point(rec.point);
    Vec3 world_normal = transform_.transform_normal(rec.normal);
    rec.set_face_normal(ray, normalize(world_normal));

    return true;
}

void TransformedShape::set_transform(const Matrix4x4& transform) {
    transform_ = transform;
    inverse_ = transform.inverse();
}

} // namespace nwave
