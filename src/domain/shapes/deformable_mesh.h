#ifndef NWAVE_DOMAIN_SHAPES_DEFORMABLE_MESH_H
#define NWAVE_DOMAIN_SHAPES_DEFORMABLE_MESH_H

#include "domain/shapes/shape.h"
#include "core/aabb.h"
#include <vector>

namespace nwave {

class DeformableMesh : public Shape {
public:
    DeformableMesh(std::vector<int> face_indices, const Material* mat);

    void update_vertices(std::vector<Point3> vertices);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;
    AABB bounding_box() const;

    const std::vector<int>& face_indices() const { return face_indices_; }
    const std::vector<Point3>& vertices() const { return vertices_; }
    const Material* material() const { return material_; }

private:
    bool hit_face(int face_index, const Ray& ray, double t_min, double t_max,
                  HitRecord& rec) const;
    void compute_smooth_normals();
    void compute_bounding_box();

    std::vector<int> face_indices_;
    std::vector<Point3> vertices_;
    std::vector<Vec3> vertex_normals_;
    const Material* material_;
    AABB bbox_;

    int face_count() const { return static_cast<int>(face_indices_.size()) / 3; }
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_DEFORMABLE_MESH_H
