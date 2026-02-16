#include <gtest/gtest.h>
#include "domain/hit_record.h"
#include "domain/shapes/triangle.h"
#include "core/ray.h"
#include "core/vec3.h"
#include <cmath>

using namespace nwave;

// Triangle in XY plane: v0=(0,0,0), v1=(1,0,0), v2=(0,1,0)
// Outward normal (from cross(edge1, edge2)) points in +z direction.

// --- Ray hits triangle dead center ---

TEST(TriangleTest, RayHitsTriangleDeadCenter) {
    Triangle tri(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), nullptr);
    // Ray from z=1 aiming at centroid (1/3, 1/3, 0)
    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = tri.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 1.0, 1e-10);
    EXPECT_NEAR(rec.point.x(), centroid.x(), 1e-10);
    EXPECT_NEAR(rec.point.y(), centroid.y(), 1e-10);
    EXPECT_NEAR(rec.point.z(), 0.0, 1e-10);
    // Normal should be +z (front face, ray opposes outward normal)
    EXPECT_NEAR(rec.normal.x(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.y(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.z(), 1.0, 1e-10);
    EXPECT_TRUE(rec.front_face);
    // Barycentric coords at centroid: u ~ 1/3, v ~ 1/3
    EXPECT_NEAR(rec.u, 1.0 / 3.0, 1e-10);
    EXPECT_NEAR(rec.v, 1.0 / 3.0, 1e-10);
}

// --- Ray hits triangle near edge (still inside) ---

TEST(TriangleTest, RayHitsTriangleNearEdge) {
    Triangle tri(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), nullptr);
    // Point near v1 edge but still inside: (0.8, 0.1, 0)
    Ray ray(Point3(0.8, 0.1, 2.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = tri.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 2.0, 1e-10);
    EXPECT_NEAR(rec.point.x(), 0.8, 1e-10);
    EXPECT_NEAR(rec.point.y(), 0.1, 1e-10);
    // u + v = 0.8 + 0.1 = 0.9 <= 1.0, so inside
    EXPECT_GE(rec.u, 0.0);
    EXPECT_LE(rec.u, 1.0);
    EXPECT_GE(rec.v, 0.0);
    EXPECT_LE(rec.u + rec.v, 1.0);
}

// --- Ray misses triangle (passes through plane but outside edges) ---

TEST(TriangleTest, RayMissesTriangleOutsideEdges) {
    Triangle tri(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), nullptr);
    // Point (1, 1, 0) is outside the triangle (u + v > 1)
    Ray ray(Point3(1, 1, 1), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = tri.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}

// --- Ray parallel to triangle ---

TEST(TriangleTest, RayParallelToTriangleReturnsFalse) {
    Triangle tri(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), nullptr);
    // Ray in XY plane, parallel to the triangle surface
    Ray ray(Point3(0.2, 0.2, 0.0), Vec3(1, 0, 0));

    HitRecord rec;
    bool hit = tri.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
    // Ensure no NaN produced in rec fields
    EXPECT_FALSE(std::isnan(rec.t));
}

// --- Ray hits from back side ---

TEST(TriangleTest, RayHitsFromBackSide) {
    Triangle tri(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), nullptr);
    // Ray from z=-1 going in +z direction (hitting the back face)
    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), -1.0), Vec3(0, 0, 1));

    HitRecord rec;
    bool hit = tri.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_FALSE(rec.front_face);
    // Normal should be flipped to -z when hit from back
    EXPECT_NEAR(rec.normal.z(), -1.0, 1e-10);
}

// --- Hit outside t_min/t_max range ---

TEST(TriangleTest, HitOutsideTRangeReturnsFalse) {
    Triangle tri(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), nullptr);
    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    // t would be 1.0, but range is [2.0, 10.0]
    bool hit = tri.hit(ray, 2.0, 10.0, rec);

    EXPECT_FALSE(hit);
}

// --- Normal is perpendicular and normalized ---

TEST(TriangleTest, NormalIsPerpendicularAndNormalized) {
    Triangle tri(Point3(0, 0, 0), Point3(2, 0, 0), Point3(0, 3, 0), nullptr);
    Point3 centroid(2.0 / 3.0, 1.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 5.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = tri.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    // Normal should be unit length
    EXPECT_NEAR(rec.normal.length(), 1.0, 1e-10);
    // Normal should be perpendicular to both edges
    Vec3 edge1 = tri.v1() - tri.v0();
    Vec3 edge2 = tri.v2() - tri.v0();
    EXPECT_NEAR(dot(rec.normal, edge1), 0.0, 1e-10);
    EXPECT_NEAR(dot(rec.normal, edge2), 0.0, 1e-10);
}

// --- Barycentric coordinates in valid range ---

TEST(TriangleTest, BarycentricCoordinatesInValidRange) {
    Triangle tri(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), nullptr);
    // Test multiple points via inner checks
    struct TestCase { double ox; double oy; };
    TestCase cases[] = {
        {0.1, 0.1}, {0.5, 0.1}, {0.1, 0.5}, {0.3, 0.3}
    };

    for (const auto& tc : cases) {
        Ray ray(Point3(tc.ox, tc.oy, 1.0), Vec3(0, 0, -1));
        HitRecord rec;
        bool hit = tri.hit(ray, 0.001, infinity, rec);

        ASSERT_TRUE(hit) << "ox=" << tc.ox << " oy=" << tc.oy;
        EXPECT_GE(rec.u, 0.0) << "ox=" << tc.ox << " oy=" << tc.oy;
        EXPECT_LE(rec.u, 1.0) << "ox=" << tc.ox << " oy=" << tc.oy;
        EXPECT_GE(rec.v, 0.0) << "ox=" << tc.ox << " oy=" << tc.oy;
        EXPECT_LE(rec.v, 1.0) << "ox=" << tc.ox << " oy=" << tc.oy;
        EXPECT_LE(rec.u + rec.v, 1.0 + 1e-10) << "ox=" << tc.ox << " oy=" << tc.oy;
    }
}

// --- Material pointer propagated ---

TEST(TriangleTest, MaterialPointerPropagated) {
    int fake_material = 42;
    const Material* mat = reinterpret_cast<const Material*>(&fake_material);

    Triangle tri(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), mat);
    Point3 centroid(1.0 / 3.0, 1.0 / 3.0, 0.0);
    Ray ray(Point3(centroid.x(), centroid.y(), 1.0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = tri.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_EQ(rec.material, mat);
}

// --- Accessors ---

TEST(TriangleTest, AccessorsReturnConstructorValues) {
    Point3 v0(1, 2, 3);
    Point3 v1(4, 5, 6);
    Point3 v2(7, 8, 9);
    Triangle tri(v0, v1, v2, nullptr);

    EXPECT_DOUBLE_EQ(tri.v0().x(), 1.0);
    EXPECT_DOUBLE_EQ(tri.v0().y(), 2.0);
    EXPECT_DOUBLE_EQ(tri.v0().z(), 3.0);
    EXPECT_DOUBLE_EQ(tri.v1().x(), 4.0);
    EXPECT_DOUBLE_EQ(tri.v1().y(), 5.0);
    EXPECT_DOUBLE_EQ(tri.v1().z(), 6.0);
    EXPECT_DOUBLE_EQ(tri.v2().x(), 7.0);
    EXPECT_DOUBLE_EQ(tri.v2().y(), 8.0);
    EXPECT_DOUBLE_EQ(tri.v2().z(), 9.0);
}
