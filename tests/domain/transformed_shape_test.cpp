#include <gtest/gtest.h>
#include "domain/shapes/transformed_shape.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/box.h"
#include "core/ray.h"
#include "core/vec3.h"
#include "core/matrix4x4.h"
#include "core/quaternion.h"
#include <cmath>

using namespace nwave;

// --- Acceptance tests: TransformedShape with translation ---

TEST(TransformedShapeTest, TranslatedSphereIsHitByRayAimedAtTranslatedPosition) {
    // Sphere at origin, translated by [3,0,0]
    // Ray from [3,0,5] aimed at [3,0,0] should hit
    auto sphere = std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, nullptr);
    auto transform = Matrix4x4::translation(3, 0, 0);
    TransformedShape translated(sphere, transform);

    Ray ray(Point3(3, 0, 5), Vec3(0, 0, -1));
    HitRecord rec;
    bool hit = translated.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    // Hit point should be in world space at approximately [3, 0, 1]
    EXPECT_NEAR(rec.point.x(), 3.0, 1e-9);
    EXPECT_NEAR(rec.point.y(), 0.0, 1e-9);
    EXPECT_NEAR(rec.point.z(), 1.0, 1e-9);
    EXPECT_TRUE(rec.front_face);
}

TEST(TransformedShapeTest, TranslatedSphereIsMissedByRayAimedAtOrigin) {
    // Sphere at origin, translated by [3,0,0]
    // Ray from [0,0,5] aimed at origin [0,0,0] should miss
    auto sphere = std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, nullptr);
    auto transform = Matrix4x4::translation(3, 0, 0);
    TransformedShape translated(sphere, transform);

    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));
    HitRecord rec;
    bool hit = translated.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}

TEST(TransformedShapeTest, SetTransformUpdatesPositionForSubsequentHitTests) {
    // Start with sphere translated to [3,0,0], then move to [-2,0,0]
    auto sphere = std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, nullptr);
    auto initial_transform = Matrix4x4::translation(3, 0, 0);
    TransformedShape translated(sphere, initial_transform);

    // Ray aimed at [-2,0,0] should initially miss
    Ray ray(Point3(-2, 0, 5), Vec3(0, 0, -1));
    HitRecord rec;
    EXPECT_FALSE(translated.hit(ray, 0.001, infinity, rec));

    // After set_transform to [-2,0,0], same ray should hit
    translated.set_transform(Matrix4x4::translation(-2, 0, 0));
    bool hit = translated.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
    EXPECT_NEAR(rec.point.x(), -2.0, 1e-9);
    EXPECT_NEAR(rec.point.y(), 0.0, 1e-9);
    EXPECT_NEAR(rec.point.z(), 1.0, 1e-9);
}

// --- Acceptance tests: TransformedShape with rotation ---

TEST(TransformedShapeTest, BoxRotated45DegAroundYIsHitByRayThatMissesAxisAlignedBox) {
    // Unit box centered at origin: [-0.5, -0.5, -0.5] to [0.5, 0.5, 0.5]
    // After 45deg Y rotation, corners extend to ~0.707 in X/Z
    // Ray at x=0.6 would miss axis-aligned box (max x=0.5) but hits rotated box
    auto box = std::make_shared<Box>(Point3(-0.5, -0.5, -0.5), Point3(0.5, 0.5, 0.5), nullptr);

    auto rotation = Quaternion::from_axis_angle(Vec3(0, 1, 0), pi / 4.0);
    auto transform = Matrix4x4::from_translation_rotation(Vec3(0, 0, 0), rotation);
    TransformedShape rotated_box(box, transform);

    // Verify axis-aligned box would be missed at x=0.6
    HitRecord rec_miss;
    Ray ray_miss(Point3(0.6, 0, -5), Vec3(0, 0, 1));
    EXPECT_FALSE(box->hit(ray_miss, 0.001, infinity, rec_miss));

    // Same ray hits the rotated box
    HitRecord rec;
    Ray ray(Point3(0.6, 0, -5), Vec3(0, 0, 1));
    bool hit = rotated_box.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);
}

TEST(TransformedShapeTest, NormalOnRotatedBoxFaceIsRotated45DegFromOriginalNormal) {
    // Box rotated 45deg around Y, hit from +X direction
    // Use ray at (5, 0, 0.3) going (-1, 0, 0) — hits the rotated +X face
    // In local space, the +X face has normal (1, 0, 0)
    // Rotated 45deg around Y: (cos45, 0, -sin45) = (0.707, 0, -0.707)
    auto box = std::make_shared<Box>(Point3(-0.5, -0.5, -0.5), Point3(0.5, 0.5, 0.5), nullptr);

    auto rotation = Quaternion::from_axis_angle(Vec3(0, 1, 0), pi / 4.0);
    auto transform = Matrix4x4::from_translation_rotation(Vec3(0, 0, 0), rotation);
    TransformedShape rotated_box(box, transform);

    // Ray from +X at z=-0.3 ensures we clearly hit the rotated +X face in local space
    Ray ray(Point3(5, 0, -0.3), Vec3(-1, 0, 0));
    HitRecord rec;
    bool hit = rotated_box.hit(ray, 0.001, infinity, rec);

    ASSERT_TRUE(hit);

    // The +X face normal (1,0,0) rotated 45deg around Y is (cos45, 0, -sin45)
    double cos45 = std::cos(pi / 4.0);
    double sin45 = std::sin(pi / 4.0);
    EXPECT_NEAR(rec.normal.x(), cos45, 1e-6);
    EXPECT_NEAR(rec.normal.y(), 0.0, 1e-6);
    EXPECT_NEAR(rec.normal.z(), -sin45, 1e-6);

    // Normal must be unit length
    EXPECT_NEAR(rec.normal.length(), 1.0, 1e-9);
}
