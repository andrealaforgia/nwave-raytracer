#include <gtest/gtest.h>
#include "domain/shapes/transformed_shape.h"
#include "domain/shapes/sphere.h"
#include "core/ray.h"
#include "core/vec3.h"
#include "core/matrix4x4.h"

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
