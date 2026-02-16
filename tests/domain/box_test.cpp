#include <gtest/gtest.h>
#include "domain/shapes/box.h"
#include "domain/hit_record.h"
#include "core/ray.h"
#include "core/vec3.h"

using namespace nwave;

// --- Box hit tests ---

TEST(BoxTest, RayHitsFrontFaceNegativeZ) {
    Box box(Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);
    // Ray from z=5 toward -z, hits the +z face first (front face from ray perspective)
    // Actually, ray going in -z hits the z=+1 face at t=4
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = box.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 4.0, 1e-10);
    EXPECT_NEAR(rec.point.z(), 1.0, 1e-10);
    EXPECT_NEAR(rec.normal.x(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.y(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.z(), 1.0, 1e-10);
    EXPECT_TRUE(rec.front_face);
}

TEST(BoxTest, RayHitsSideFacePositiveX) {
    Box box(Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);
    // Ray from +x direction hitting the +x face
    Ray ray(Point3(5, 0, 0), Vec3(-1, 0, 0));

    HitRecord rec;
    bool hit = box.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 4.0, 1e-10);
    EXPECT_NEAR(rec.point.x(), 1.0, 1e-10);
    EXPECT_NEAR(rec.normal.x(), 1.0, 1e-10);
    EXPECT_NEAR(rec.normal.y(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.z(), 0.0, 1e-10);
    EXPECT_TRUE(rec.front_face);
}

TEST(BoxTest, RayHitsTopFacePositiveY) {
    Box box(Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);
    // Ray from above hitting the +y face
    Ray ray(Point3(0, 5, 0), Vec3(0, -1, 0));

    HitRecord rec;
    bool hit = box.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 4.0, 1e-10);
    EXPECT_NEAR(rec.point.y(), 1.0, 1e-10);
    EXPECT_NEAR(rec.normal.x(), 0.0, 1e-10);
    EXPECT_NEAR(rec.normal.y(), 1.0, 1e-10);
    EXPECT_NEAR(rec.normal.z(), 0.0, 1e-10);
    EXPECT_TRUE(rec.front_face);
}

TEST(BoxTest, RayMissesBox) {
    Box box(Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);
    // Ray passes beside the box entirely
    Ray ray(Point3(5, 5, 5), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = box.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}

TEST(BoxTest, RayOriginInsideBoxHitsExitFace) {
    Box box(Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);
    Ray ray(Point3(0, 0, 0), Vec3(0, 0, 1));

    HitRecord rec;
    bool hit = box.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.t, 1.0, 1e-10);
    EXPECT_NEAR(rec.point.z(), 1.0, 1e-10);
    // Inside the box: back face
    EXPECT_FALSE(rec.front_face);
}

TEST(BoxTest, HitOutsideTRangeReturnsFalse) {
    Box box(Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));

    HitRecord rec;
    // t=4 is where we'd hit, but range is [0.001, 3.0]
    bool hit = box.hit(ray, 0.001, 3.0, rec);

    EXPECT_FALSE(hit);
}

TEST(BoxTest, HitFillsMaterialPointer) {
    int fake_material = 42;
    const Material* mat = reinterpret_cast<const Material*>(&fake_material);

    Box box(Point3(-1, -1, -1), Point3(1, 1, 1), mat);
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));

    HitRecord rec;
    bool hit = box.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_EQ(rec.material, mat);
}

TEST(BoxTest, ConstructorSwapsMinMaxIfNeeded) {
    Box box(Point3(3, 4, 5), Point3(1, 2, 3), nullptr);

    EXPECT_DOUBLE_EQ(box.box_min().x(), 1.0);
    EXPECT_DOUBLE_EQ(box.box_min().y(), 2.0);
    EXPECT_DOUBLE_EQ(box.box_min().z(), 3.0);
    EXPECT_DOUBLE_EQ(box.box_max().x(), 3.0);
    EXPECT_DOUBLE_EQ(box.box_max().y(), 4.0);
    EXPECT_DOUBLE_EQ(box.box_max().z(), 5.0);
}
