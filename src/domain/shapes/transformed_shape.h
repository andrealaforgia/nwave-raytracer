#ifndef NWAVE_DOMAIN_SHAPES_TRANSFORMED_SHAPE_H
#define NWAVE_DOMAIN_SHAPES_TRANSFORMED_SHAPE_H

#include "domain/shapes/shape.h"
#include "core/matrix4x4.h"
#include <memory>

namespace nwave {

class TransformedShape : public Shape {
public:
    TransformedShape(std::shared_ptr<Shape> inner, const Matrix4x4& transform);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;

    void set_transform(const Matrix4x4& transform);

    const std::shared_ptr<Shape>& inner() const { return inner_; }
    const Matrix4x4& transform() const { return transform_; }
    const Matrix4x4& inverse_matrix() const { return inverse_; }

private:
    std::shared_ptr<Shape> inner_;
    Matrix4x4 transform_;
    Matrix4x4 inverse_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_TRANSFORMED_SHAPE_H
