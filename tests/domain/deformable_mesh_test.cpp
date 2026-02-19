#include <gtest/gtest.h>
#include "domain/shapes/deformable_mesh.h"
#include "domain/hit_record.h"
#include "core/ray.h"
#include "core/vec3.h"
#include "core/aabb.h"
#include <cmath>
#include <vector>

using namespace nwave;

// Test Budget: 6 behaviors x 2 = 12 max unit tests
//
// Behaviors:
// 1. Ray hits triangle face - correct t, point, and barycentric-interpolated smooth normal
// 2. Ray missing all faces returns false
// 3. Bounding box tight around current vertices
// 4. update_vertices replaces positions and recomputes normals and AABB
// 5. Two-triangle quad can be constructed and intersected
// 6. Material pointer propagated

// Helper: single triangle in XY plane (4 vertices, but only 3 used in 1 face)
static std::vector<Point3> xy_triangle_vertices() {
    return {Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0)};
}

static std::vector<int> single_face_indices() {
    return {0, 1, 2};
}

// Helper: 2-triangle quad in XY plane
//   v2(0,1,0) --- v3(1,1,0)
//   |           /  |
//   |  face0  / face1 |
//   |       /      |
//   v0(0,0,0) --- v1(1,0,0)
static std::vector<Point3> quad_vertices() {
    return {
        Point3(0, 0, 0),  // v0
        Point3(1, 0, 0),  // v1
        Point3(0, 1, 0),  // v2
        Point3(1, 1, 0),  // v3
    };
}

static std::vector<int> quad_face_indices() {
    return {
        0, 1, 2,  // face 0: v0, v1, v2
        1, 3, 2,  // face 1: v1, v3, v2
    };
}

// --- Behavior 1: Ray hits triangle face returns correct t, point, and smooth normal ---

TEST(DeformableMeshTest, RayHitsTriangleFaceReturnsCorrectTAndPoint) {
    auto face_indices = single_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);
    mesh.update_vertices(xy_triangle_vertices());

    // Ray from z=2, aimed at centroid (1/3, 1/3, 0)
    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 2.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 2.0, 1e-6);
    EXPECT_NEAR(rec.point.x(), centroid.x(), 1e-6);
    EXPECT_NEAR(rec.point.y(), centroid.y(), 1e-6);
    EXPECT_NEAR(rec.point.z(), 0.0, 1e-6);
}

TEST(DeformableMeshTest, RayHitReturnsBarycentricSmoothNormal) {
    // A single triangle in XY plane. All three vertex normals point in +Z,
    // so the smooth normal at any point must be (0, 0, 1).
    auto face_indices = single_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);
    mesh.update_vertices(xy_triangle_vertices());

    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    // For a flat triangle in XY plane, all vertex normals are (0,0,1),
    // so interpolated smooth normal should be (0,0,1).
    EXPECT_NEAR(rec.normal.x(), 0.0, 1e-6);
    EXPECT_NEAR(rec.normal.y(), 0.0, 1e-6);
    EXPECT_NEAR(rec.normal.z(), 1.0, 1e-6);
    EXPECT_NEAR(rec.normal.length(), 1.0, 1e-6);
    EXPECT_TRUE(rec.front_face);
}

// --- Behavior 2: Ray missing all faces returns false ---

TEST(DeformableMeshTest, RayMissingAllFacesReturnsFalse) {
    auto face_indices = single_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);
    mesh.update_vertices(xy_triangle_vertices());

    // Point (2, 2, 0) is well outside the triangle
    Ray ray(Point3(2, 2, 1), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}

// --- Behavior 3: Bounding box tight around vertices ---

TEST(DeformableMeshTest, BoundingBoxTightAroundVertices) {
    auto face_indices = quad_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);
    mesh.update_vertices(quad_vertices());

    AABB box = mesh.bounding_box();

    // Quad spans [0,1] x [0,1] x [0,0]
    EXPECT_NEAR(box.minimum().x(), 0.0, 1e-10);
    EXPECT_NEAR(box.minimum().y(), 0.0, 1e-10);
    EXPECT_NEAR(box.minimum().z(), 0.0, 1e-10);
    EXPECT_NEAR(box.maximum().x(), 1.0, 1e-10);
    EXPECT_NEAR(box.maximum().y(), 1.0, 1e-10);
    EXPECT_NEAR(box.maximum().z(), 0.0, 1e-10);
}

// --- Behavior 4: update_vertices changes geometry ---

TEST(DeformableMeshTest, UpdateVerticesChangesGeometry) {
    auto face_indices = single_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);

    // Initially at z=0
    mesh.update_vertices(xy_triangle_vertices());

    // Move triangle to z=5
    std::vector<Point3> moved = {
        Point3(0, 0, 5), Point3(1, 0, 5), Point3(0, 1, 5)
    };
    mesh.update_vertices(std::move(moved));

    // Ray from z=10, aiming down
    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 10.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 5.0, 1e-6);
    EXPECT_NEAR(rec.point.z(), 5.0, 1e-6);

    // AABB should also reflect new position
    AABB box = mesh.bounding_box();
    EXPECT_NEAR(box.minimum().z(), 5.0, 1e-10);
    EXPECT_NEAR(box.maximum().z(), 5.0, 1e-10);
}

// --- Behavior 5: Two-triangle quad intersection ---

TEST(DeformableMeshTest, TwoTriangleQuadCanBeIntersected) {
    auto face_indices = quad_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);
    mesh.update_vertices(quad_vertices());

    // Hit face 1 (the upper-right triangle: v1, v3, v2)
    // Point (0.8, 0.8) is in the upper-right triangle
    Ray ray(Point3(0.8, 0.8, 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 1.0, 1e-6);
    EXPECT_NEAR(rec.point.x(), 0.8, 1e-6);
    EXPECT_NEAR(rec.point.y(), 0.8, 1e-6);
    EXPECT_NEAR(rec.point.z(), 0.0, 1e-6);
    // Normal should be +z for a flat quad
    EXPECT_NEAR(std::fabs(rec.normal.z()), 1.0, 1e-6);
}

// --- Behavior 6: Material pointer propagated ---

TEST(DeformableMeshTest, MaterialPointerPropagated) {
    int fake_material = 42;
    const Material* mat = reinterpret_cast<const Material*>(&fake_material);

    auto face_indices = single_face_indices();
    DeformableMesh mesh(std::move(face_indices), mat);
    mesh.update_vertices(xy_triangle_vertices());

    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_EQ(rec.material, mat);
}
