#include "domain/shapes/triangle_mesh.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace nwave {

TriangleMesh::TriangleMesh(std::vector<Point3> vertices,
                           std::vector<Vec3> normals,
                           std::vector<int> vertex_indices,
                           std::vector<int> normal_indices,
                           const Material* mat)
    : vertices_(std::move(vertices))
    , normals_(std::move(normals))
    , vertex_indices_(std::move(vertex_indices))
    , normal_indices_(std::move(normal_indices))
    , material_(mat) {

    // Compute bounding box over all vertices
    if (!vertices_.empty()) {
        Point3 min_pt = vertices_[0];
        Point3 max_pt = vertices_[0];

        for (size_t i = 1; i < vertices_.size(); ++i) {
            for (int c = 0; c < 3; ++c) {
                min_pt[c] = std::min(min_pt[c], vertices_[i][c]);
                max_pt[c] = std::max(max_pt[c], vertices_[i][c]);
            }
        }

        bbox_ = AABB(min_pt, max_pt);
    }
}

bool TriangleMesh::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    bool hit_anything = false;
    double closest_so_far = t_max;

    for (int f = 0; f < face_count(); ++f) {
        HitRecord temp_rec;
        if (hit_face(f, ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

bool TriangleMesh::hit_face(int face_index, const Ray& ray, double t_min, double t_max,
                            HitRecord& rec) const {
    int base = face_index * 3;
    const Point3& v0 = vertices_[vertex_indices_[base]];
    const Point3& v1 = vertices_[vertex_indices_[base + 1]];
    const Point3& v2 = vertices_[vertex_indices_[base + 2]];

    // Moller-Trumbore intersection
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 h = cross(ray.direction(), edge2);
    double a = dot(edge1, h);

    if (std::fabs(a) < epsilon)
        return false;

    double f = 1.0 / a;
    Vec3 s = ray.origin() - v0;
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

    // Compute normal: smooth (interpolated) or flat (face normal)
    Vec3 outward_normal;
    if (has_smooth_normals()) {
        double w = 1.0 - u - v;
        const Vec3& n0 = normals_[normal_indices_[base]];
        const Vec3& n1 = normals_[normal_indices_[base + 1]];
        const Vec3& n2 = normals_[normal_indices_[base + 2]];
        outward_normal = normalize(w * n0 + u * n1 + v * n2);
    } else {
        outward_normal = normalize(cross(edge1, edge2));
    }

    rec.set_face_normal(ray, outward_normal);
    rec.material = material_;

    return true;
}

AABB TriangleMesh::bounding_box() const {
    return bbox_;
}

} // namespace nwave
