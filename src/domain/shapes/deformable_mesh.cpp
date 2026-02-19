#include "domain/shapes/deformable_mesh.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace nwave {

DeformableMesh::DeformableMesh(std::vector<int> face_indices, const Material* mat)
    : face_indices_(std::move(face_indices))
    , material_(mat) {
}

void DeformableMesh::update_vertices(std::vector<Point3> vertices) {
    vertices_ = std::move(vertices);
    compute_smooth_normals();
    compute_bounding_box();
}

void DeformableMesh::compute_smooth_normals() {
    vertex_normals_.assign(vertices_.size(), Vec3(0, 0, 0));

    int num_faces = face_count();
    for (int f = 0; f < num_faces; ++f) {
        int base = f * 3;
        int i0 = face_indices_[base];
        int i1 = face_indices_[base + 1];
        int i2 = face_indices_[base + 2];

        Vec3 edge1 = vertices_[i1] - vertices_[i0];
        Vec3 edge2 = vertices_[i2] - vertices_[i0];

        // UN-normalized cross product: magnitude equals twice the triangle area
        Vec3 area_weighted_normal = cross(edge1, edge2);

        // Accumulate into each vertex of this face
        vertex_normals_[i0] += area_weighted_normal;
        vertex_normals_[i1] += area_weighted_normal;
        vertex_normals_[i2] += area_weighted_normal;
    }

    for (auto& n : vertex_normals_) {
        double len = n.length();
        if (len > 0.0) {
            n = n / len;
        }
    }
}

void DeformableMesh::compute_bounding_box() {
    if (vertices_.empty()) return;

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

bool DeformableMesh::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
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

bool DeformableMesh::hit_face(int face_index, const Ray& ray, double t_min, double t_max,
                              HitRecord& rec) const {
    int base = face_index * 3;
    int i0 = face_indices_[base];
    int i1 = face_indices_[base + 1];
    int i2 = face_indices_[base + 2];

    const Point3& v0 = vertices_[i0];
    const Point3& v1 = vertices_[i1];
    const Point3& v2 = vertices_[i2];

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

    // Barycentric interpolation of per-vertex smooth normals
    double w = 1.0 - u - v;
    const Vec3& n0 = vertex_normals_[i0];
    const Vec3& n1 = vertex_normals_[i1];
    const Vec3& n2 = vertex_normals_[i2];
    Vec3 outward_normal = normalize(w * n0 + u * n1 + v * n2);

    rec.set_face_normal(ray, outward_normal);
    rec.material = material_;

    return true;
}

AABB DeformableMesh::bounding_box() const {
    return bbox_;
}

} // namespace nwave
