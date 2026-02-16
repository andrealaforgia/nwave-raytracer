#include <gtest/gtest.h>
#include "domain/hit_record.h"
#include "domain/shapes/sphere.h"
#include "core/ray.h"
#include "core/vec3.h"
#include <cmath>

using namespace nwave;

// --- HitRecord::set_face_normal ---

TEST(HitRecordTest, SetFaceNormalFrontFaceWhenRayOpposesOutwardNormal) {
    HitRecord rec;
    Ray ray(Point3(0, 0, 0), Vec3(0, 0, -1));
    Vec3 outward_normal(0, 0, 1);

    rec.set_face_normal(ray, outward_normal);

    EXPECT_TRUE(rec.front_face);
    EXPECT_DOUBLE_EQ(rec.normal.x(), 0.0);
    EXPECT_DOUBLE_EQ(rec.normal.y(), 0.0);
    EXPECT_DOUBLE_EQ(rec.normal.z(), 1.0);
}

TEST(HitRecordTest, SetFaceNormalBackFaceWhenRaySameDirectionAsOutwardNormal) {
    HitRecord rec;
    Ray ray(Point3(0, 0, 0), Vec3(0, 0, 1));
    Vec3 outward_normal(0, 0, 1);

    rec.set_face_normal(ray, outward_normal);

    EXPECT_FALSE(rec.front_face);
    EXPECT_DOUBLE_EQ(rec.normal.x(), 0.0);
    EXPECT_DOUBLE_EQ(rec.normal.y(), 0.0);
    EXPECT_DOUBLE_EQ(rec.normal.z(), -1.0);
}

// --- Sphere hit tests ---

TEST(SphereTest, RayHitsSphereFrontFace) {
    // Sphere at origin, radius 1. Ray from z=5 going toward -z.
    Sphere sphere(Point3(0, 0, 0), 1.0, nullptr);
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = sphere.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 4.0, 1e-10);
    EXPECT_NEAR(rec.point.z(), 1.0, 1e-10);
    EXPECT_NEAR(rec.normal.x(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.y(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.z(), 1.0, 1e-10);
    EXPECT_TRUE(rec.front_face);
}

TEST(SphereTest, RayHitsSphereAtObliqueAngle) {
    Sphere sphere(Point3(0, 0, 0), 1.0, nullptr);
    Ray ray(Point3(0, 2, 2), Vec3(0, -1, -1));

    HitRecord rec;
    bool hit = sphere.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_GT(rec.t, 0.0);
    // Hit point should be on the sphere surface (distance from origin = 1)
    EXPECT_NEAR(rec.point.length(), 1.0, 1e-10);
    EXPECT_TRUE(rec.front_face);
}

TEST(SphereTest, RayMissesSphere) {
    Sphere sphere(Point3(0, 0, 0), 1.0, nullptr);
    Ray ray(Point3(0, 5, 0), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = sphere.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}

TEST(SphereTest, RayTangentToSphere) {
    Sphere sphere(Point3(0, 0, 0), 1.0, nullptr);
    Ray ray(Point3(1, 0, 5), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = sphere.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 5.0, 1e-10);
    EXPECT_NEAR(rec.point.x(), 1.0, 1e-10);
    EXPECT_NEAR(rec.point.z(), 0.0, 1e-10);
}

TEST(SphereTest, RayOriginInsideSphereUsesPositiveRoot) {
    Sphere sphere(Point3(0, 0, 0), 1.0, nullptr);
    Ray ray(Point3(0, 0, 0), Vec3(0, 0, 1));

    HitRecord rec;
    bool hit = sphere.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 1.0, 1e-10);
    EXPECT_NEAR(rec.point.z(), 1.0, 1e-10);
    // Ray direction aligns with outward normal, so we are inside: back face
    EXPECT_FALSE(rec.front_face);
    EXPECT_NEAR(rec.normal.z(), -1.0, 1e-10);
}

TEST(SphereTest, SphereWithNegativeRadiusInvertsNormal) {
    Sphere sphere(Point3(0, 0, 0), -1.0, nullptr);
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = sphere.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 4.0, 1e-10);
    // outward_normal = (point - center) / radius = (0,0,1) / -1 = (0,0,-1)
    // dot(ray.dir, outward_normal) = dot((0,0,-1),(0,0,-1)) = 1 > 0
    // front_face = false, normal = -outward_normal = (0,0,1)
    EXPECT_FALSE(rec.front_face);
    EXPECT_NEAR(rec.normal.z(), 1.0, 1e-10);
}

TEST(SphereTest, HitFillsMaterialPointer) {
    int fake_material = 42;
    const Material* mat = reinterpret_cast<const Material*>(&fake_material);

    Sphere sphere(Point3(0, 0, 0), 1.0, mat);
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = sphere.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_EQ(rec.material, mat);
}

TEST(SphereTest, HitOutsideTRangeReturnsFalse) {
    Sphere sphere(Point3(0, 0, 0), 1.0, nullptr);
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));

    HitRecord rec;
    // Both roots (t=4 and t=6) are outside [0.001, 3.0]
    bool hit = sphere.hit(ray, 0.001, 3.0, rec);
    EXPECT_FALSE(hit);
}

// --- Accessors ---

TEST(SphereTest, AccessorsReturnConstructorValues) {
    Point3 center(1, 2, 3);
    Sphere sphere(center, 4.5, nullptr);

    EXPECT_DOUBLE_EQ(sphere.center().x(), 1.0);
    EXPECT_DOUBLE_EQ(sphere.center().y(), 2.0);
    EXPECT_DOUBLE_EQ(sphere.center().z(), 3.0);
    EXPECT_DOUBLE_EQ(sphere.radius(), 4.5);
}
