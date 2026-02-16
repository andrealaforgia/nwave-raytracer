#include <gtest/gtest.h>
#include "domain/shapes/plane.h"
#include "domain/materials/material.h"
#include "core/ray.h"
#include "core/vec3.h"
#include <cmath>

using namespace nwave;

// --- Plane hit tests ---

TEST(PlaneTest, RayHitsPlaneFromAbove) {
    // Plane at y=0, normal pointing up (+Y). Ray from y=5 going down.
    Plane plane(Point3(0, 0, 0), Vec3(0, 1, 0), nullptr);
    Ray ray(Point3(0, 5, 0), Vec3(0, -1, 0));

    HitRecord rec;
    bool hit = plane.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 5.0, 1e-10);
    EXPECT_NEAR(rec.point.x(), 0.0, 1e-10);
    EXPECT_NEAR(rec.point.y(), 0.0, 1e-10);
    EXPECT_NEAR(rec.point.z(), 0.0, 1e-10);
    // Ray opposes outward normal => front face
    EXPECT_TRUE(rec.front_face);
    EXPECT_NEAR(rec.normal.y(), 1.0, 1e-10);
}

TEST(PlaneTest, RayHitsPlaneFromBelow) {
    // Plane at y=0, normal pointing up (+Y). Ray from y=-3 going up.
    Plane plane(Point3(0, 0, 0), Vec3(0, 1, 0), nullptr);
    Ray ray(Point3(0, -3, 0), Vec3(0, 1, 0));

    HitRecord rec;
    bool hit = plane.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 3.0, 1e-10);
    // Ray aligns with outward normal => back face
    EXPECT_FALSE(rec.front_face);
    // Normal flipped to oppose ray direction
    EXPECT_NEAR(rec.normal.y(), -1.0, 1e-10);
}

TEST(PlaneTest, RayParallelToPlaneReturnsFalse) {
    Plane plane(Point3(0, 0, 0), Vec3(0, 1, 0), nullptr);
    // Ray traveling in X direction, parallel to XZ plane
    Ray ray(Point3(0, 5, 0), Vec3(1, 0, 0));

    HitRecord rec;
    bool hit = plane.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}

TEST(PlaneTest, RayNearlyParallelToPlaneReturnsFalse) {
    Plane plane(Point3(0, 0, 0), Vec3(0, 1, 0), nullptr);
    // Ray direction has a tiny Y component within the epsilon threshold
    Ray ray(Point3(0, 5, 0), Vec3(1, 1e-9, 0));

    HitRecord rec;
    bool hit = plane.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}

TEST(PlaneTest, HitOutsideTRangeReturnsFalse) {
    Plane plane(Point3(0, 0, 0), Vec3(0, 1, 0), nullptr);
    Ray ray(Point3(0, 5, 0), Vec3(0, -1, 0));

    HitRecord rec;
    // t would be 5.0, but t_max is 3.0
    bool hit = plane.hit(ray, 0.001, 3.0, rec);

    EXPECT_FALSE(hit);
}

TEST(PlaneTest, MaterialPointerPropagated) {
    int fake_material = 42;
    const Material* mat = reinterpret_cast<const Material*>(&fake_material);

    Plane plane(Point3(0, 0, 0), Vec3(0, 1, 0), mat);
    Ray ray(Point3(0, 5, 0), Vec3(0, -1, 0));

    HitRecord rec;
    bool hit = plane.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_EQ(rec.material, mat);
}

TEST(PlaneTest, ConstructorNormalizesNormal) {
    // Pass non-unit normal, should be normalized
    Plane plane(Point3(0, 0, 0), Vec3(0, 3, 0), nullptr);

    EXPECT_NEAR(plane.normal().length(), 1.0, 1e-10);
    EXPECT_NEAR(plane.normal().y(), 1.0, 1e-10);
}

TEST(PlaneTest, AccessorsReturnConstructorValues) {
    Point3 point(1, 2, 3);
    Vec3 normal(0, 0, 1);
    Plane plane(point, normal, nullptr);

    EXPECT_DOUBLE_EQ(plane.point().x(), 1.0);
    EXPECT_DOUBLE_EQ(plane.point().y(), 2.0);
    EXPECT_DOUBLE_EQ(plane.point().z(), 3.0);
    EXPECT_NEAR(plane.normal().z(), 1.0, 1e-10);
}
