#ifndef NWAVE_DOMAIN_SHAPES_TRIANGLE_MESH_H
#define NWAVE_DOMAIN_SHAPES_TRIANGLE_MESH_H

#include "domain/shapes/shape.h"
#include "core/aabb.h"
#include <vector>

namespace nwave {

class TriangleMesh : public Shape {
public:
    TriangleMesh(std::vector<Point3> vertices,
                 std::vector<Vec3> normals,
                 std::vector<int> vertex_indices,
                 std::vector<int> normal_indices,
                 const Material* mat);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;
    AABB bounding_box() const;

private:
    bool hit_face(int face_index, const Ray& ray, double t_min, double t_max,
                  HitRecord& rec) const;

    std::vector<Point3> vertices_;
    std::vector<Vec3> normals_;
    std::vector<int> vertex_indices_;
    std::vector<int> normal_indices_;
    const Material* material_;
    AABB bbox_;

    int face_count() const { return static_cast<int>(vertex_indices_.size()) / 3; }
    bool has_smooth_normals() const { return !normal_indices_.empty(); }
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_TRIANGLE_MESH_H
