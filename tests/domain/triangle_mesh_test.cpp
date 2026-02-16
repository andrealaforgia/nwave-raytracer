#include <gtest/gtest.h>
#include "domain/shapes/triangle_mesh.h"
#include "domain/hit_record.h"
#include "core/ray.h"
#include "core/vec3.h"
#include "core/aabb.h"
#include <cmath>
#include <vector>

using namespace nwave;

// Test Budget: 5 behaviors x 2 = 10 max unit tests
//
// Behaviors:
// 1. Ray hits a mesh face (basic Moller-Trumbore intersection)
// 2. Smooth shading: normal interpolated from per-vertex normals
// 3. Interpolated normal is re-normalized
// 4. Flat shading: face normal used when no per-vertex normals
// 5. Bounding box encloses all vertices

// Helper: create a simple single-face mesh in XY plane
static std::vector<Point3> xy_vertices() {
    return {Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0)};
}

static std::vector<int> single_face_indices() {
    return {0, 1, 2};
}

// --- Acceptance test: smooth-shaded mesh produces interpolated normals ---

TEST(TriangleMeshTest, SmoothShadedMeshInterpolatesNormalsFromPerVertexNormals) {
    auto vertices = xy_vertices();

    // Per-vertex normals: tilted away from pure +z to test interpolation
    std::vector<Vec3> normals = {
        normalize(Vec3(0, 0, 1)),        // n0: straight up
        normalize(Vec3(0.5, 0, 1)),      // n1: tilted toward +x
        normalize(Vec3(0, 0.5, 1)),      // n2: tilted toward +y
    };

    auto vi = single_face_indices();
    std::vector<int> ni = {0, 1, 2};

    TriangleMesh mesh(vertices, normals, vi, ni, nullptr);

    // Ray aimed at centroid (1/3, 1/3, 0) from z=1
    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);

    // At centroid, barycentric coords: u ~ 1/3, v ~ 1/3, w = 1-u-v ~ 1/3
    double u = 1.0 / 3.0;
    double v = 1.0 / 3.0;
    double w = 1.0 - u - v;
    Vec3 expected_raw = w * normals[0] + u * normals[1] + v * normals[2];
    Vec3 expected_normal = normalize(expected_raw);

    // The normal must be the interpolated one, not the face normal (0,0,1)
    EXPECT_NEAR(rec.normal.x(), expected_normal.x(), 1e-6);
    EXPECT_NEAR(rec.normal.y(), expected_normal.y(), 1e-6);
    EXPECT_NEAR(rec.normal.z(), expected_normal.z(), 1e-6);

    // And the interpolated normal should differ from the flat face normal
    EXPECT_GT(std::fabs(rec.normal.x()), 0.01);
    EXPECT_GT(std::fabs(rec.normal.y()), 0.01);

    // Normal must be unit length (re-normalized)
    EXPECT_NEAR(rec.normal.length(), 1.0, 1e-10);
}

// --- Behavior 1: Ray hits mesh face, returns correct hit point and t ---

TEST(TriangleMeshTest, RayHitsMeshFaceReturnsCorrectHitPointAndT) {
    auto vertices = xy_vertices();
    auto vi = single_face_indices();

    // No normals -- flat shading
    TriangleMesh mesh(vertices, {}, vi, {}, nullptr);

    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 2.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 2.0, 1e-10);
    EXPECT_NEAR(rec.point.x(), centroid.x(), 1e-10);
    EXPECT_NEAR(rec.point.y(), centroid.y(), 1e-10);
    EXPECT_NEAR(rec.point.z(), 0.0, 1e-10);
}

// --- Behavior 1b: Ray misses all faces ---

TEST(TriangleMeshTest, RayMissesAllFacesReturnsFalse) {
    auto vertices = xy_vertices();
    auto vi = single_face_indices();
    TriangleMesh mesh(vertices, {}, vi, {}, nullptr);

    // Point (1,1,0) is outside triangle (u+v > 1)
    Ray ray(Point3(1, 1, 1), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}

// --- Behavior 4: Without per-vertex normals, face normal is used ---

TEST(TriangleMeshTest, FlatShadingUsesFaceNormalWhenNoPerVertexNormals) {
    auto vertices = xy_vertices();
    auto vi = single_face_indices();

    // No normals provided -- flat shading
    TriangleMesh mesh(vertices, {}, vi, {}, nullptr);

    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    // Face normal of XY plane triangle is +z
    EXPECT_NEAR(rec.normal.x(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.y(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.z(), 1.0, 1e-10);
    EXPECT_TRUE(rec.front_face);
}

// --- Behavior 5: Bounding box encloses all vertices ---

TEST(TriangleMeshTest, BoundingBoxEnclosesAllVertices) {
    std::vector<Point3> vertices = {
        Point3(-1, -2, -3),
        Point3(4, 5, 6),
        Point3(0, 0, 0),
        Point3(2, 3, 1),
    };
    // Two-face mesh sharing vertex 0 and 2
    std::vector<int> vi = {0, 1, 2, 0, 2, 3};

    TriangleMesh mesh(vertices, {}, vi, {}, nullptr);

    AABB box = mesh.bounding_box();

    // Minimum must be <= all vertex components
    EXPECT_LE(box.minimum().x(), -1.0);
    EXPECT_LE(box.minimum().y(), -2.0);
    EXPECT_LE(box.minimum().z(), -3.0);

    // Maximum must be >= all vertex components
    EXPECT_GE(box.maximum().x(), 4.0);
    EXPECT_GE(box.maximum().y(), 5.0);
    EXPECT_GE(box.maximum().z(), 6.0);
}

// --- Behavior 1c: Multi-face mesh finds closest hit ---

TEST(TriangleMeshTest, MultiFaceMeshReturnsClosestHit) {
    // Two faces: one at z=0, one at z=2, both in XY plane
    std::vector<Point3> vertices = {
        Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0),  // face 0 at z=0
        Point3(0, 0, 2), Point3(1, 0, 2), Point3(0, 1, 2),  // face 1 at z=2
    };
    std::vector<int> vi = {0, 1, 2, 3, 4, 5};

    TriangleMesh mesh(vertices, {}, vi, {}, nullptr);

    // Ray from z=5 going toward -z, should hit face at z=2 first
    Ray ray(Point3(0.2, 0.2, 5.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 3.0, 1e-10);
    EXPECT_NEAR(rec.point.z(), 2.0, 1e-10);
}

// --- Behavior 2b: Smooth shading near vertex uses that vertex's normal ---

TEST(TriangleMeshTest, SmoothShadingNearVertexUsesVertexNormal) {
    auto vertices = xy_vertices();

    // Normals: n0 along +z, n1 tilted heavily toward +x
    std::vector<Vec3> normals = {
        Vec3(0, 0, 1),
        normalize(Vec3(1, 0, 1)),
        Vec3(0, 0, 1),
    };

    auto vi = single_face_indices();
    std::vector<int> ni = {0, 1, 2};

    TriangleMesh mesh(vertices, normals, vi, ni, nullptr);

    // Ray near vertex v1 (u close to 1, v close to 0)
    Ray ray(Point3(0.9, 0.05, 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    // Normal should be strongly influenced by n1 (tilted toward +x)
    EXPECT_GT(rec.normal.x(), 0.3);
    EXPECT_NEAR(rec.normal.length(), 1.0, 1e-10);
}

// --- Material pointer propagated ---

TEST(TriangleMeshTest, MaterialPointerPropagated) {
    int fake_material = 42;
    const Material* mat = reinterpret_cast<const Material*>(&fake_material);

    auto vertices = xy_vertices();
    auto vi = single_face_indices();

    TriangleMesh mesh(vertices, {}, vi, {}, mat);

    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = mesh.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_EQ(rec.material, mat);
}
