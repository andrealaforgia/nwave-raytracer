#include <gtest/gtest.h>
#include "core/aabb.h"
#include "core/ray.h"
#include "core/vec3.h"

using namespace nwave;

// --- AABB hit tests ---

TEST(AABBTest, RayHitsAABB) {
    AABB box(Point3(-1, -1, -1), Point3(1, 1, 1));
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));

    EXPECT_TRUE(box.hit(ray, 0.001, infinity));
}

TEST(AABBTest, RayMissesAABB) {
    AABB box(Point3(-1, -1, -1), Point3(1, 1, 1));
    Ray ray(Point3(5, 5, 5), Vec3(0, 0, -1));

    EXPECT_FALSE(box.hit(ray, 0.001, infinity));
}

TEST(AABBTest, RayOriginInsideAABBStillHits) {
    AABB box(Point3(-1, -1, -1), Point3(1, 1, 1));
    Ray ray(Point3(0, 0, 0), Vec3(0, 0, 1));

    EXPECT_TRUE(box.hit(ray, 0.001, infinity));
}

TEST(AABBTest, RayParallelToFaceButWithinSlabHits) {
    AABB box(Point3(-1, -1, -1), Point3(1, 1, 1));
    // Ray parallel to x-axis, within y and z slabs
    Ray ray(Point3(-5, 0, 0), Vec3(1, 0, 0));

    EXPECT_TRUE(box.hit(ray, 0.001, infinity));
}

// --- surrounding_box ---

TEST(AABBTest, SurroundingBoxMergesTwoBoxes) {
    AABB a(Point3(-1, -2, -3), Point3(1, 2, 3));
    AABB b(Point3(0, 0, 0), Point3(4, 5, 6));

    AABB merged = AABB::surrounding_box(a, b);

    EXPECT_DOUBLE_EQ(merged.minimum().x(), -1.0);
    EXPECT_DOUBLE_EQ(merged.minimum().y(), -2.0);
    EXPECT_DOUBLE_EQ(merged.minimum().z(), -3.0);
    EXPECT_DOUBLE_EQ(merged.maximum().x(), 4.0);
    EXPECT_DOUBLE_EQ(merged.maximum().y(), 5.0);
    EXPECT_DOUBLE_EQ(merged.maximum().z(), 6.0);
}

// --- longest_axis ---

TEST(AABBTest, LongestAxisReturnsCorrectAxis) {
    // x extent = 2, y extent = 6, z extent = 4 => longest is y (axis 1)
    AABB box(Point3(0, -3, -2), Point3(2, 3, 2));

    EXPECT_EQ(box.longest_axis(), 1);
}

// --- Constructor swaps min/max ---

TEST(AABBTest, ConstructorSwapsMinMaxIfNeeded) {
    AABB box(Point3(3, 4, 5), Point3(1, 2, 3));

    EXPECT_DOUBLE_EQ(box.minimum().x(), 1.0);
    EXPECT_DOUBLE_EQ(box.minimum().y(), 2.0);
    EXPECT_DOUBLE_EQ(box.minimum().z(), 3.0);
    EXPECT_DOUBLE_EQ(box.maximum().x(), 3.0);
    EXPECT_DOUBLE_EQ(box.maximum().y(), 4.0);
    EXPECT_DOUBLE_EQ(box.maximum().z(), 5.0);
}
