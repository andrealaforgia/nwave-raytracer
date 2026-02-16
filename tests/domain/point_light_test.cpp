#include <gtest/gtest.h>
#include "domain/lights/point_light.h"
#include "core/vec3.h"
#include <cmath>

using namespace nwave;

TEST(PointLightTest, SampleReturnsCorrectDirectionTowardLight) {
    PointLight light(Point3(0, 10, 0), Color3(1, 1, 1), 1.0);
    Point3 hit_point(0, 0, 0);

    LightSample sample = light.sample(hit_point);

    // Direction should be normalized and pointing toward the light (upward)
    EXPECT_NEAR(sample.direction.x(), 0.0, 1e-10);
    EXPECT_NEAR(sample.direction.y(), 1.0, 1e-10);
    EXPECT_NEAR(sample.direction.z(), 0.0, 1e-10);
    EXPECT_NEAR(sample.direction.length(), 1.0, 1e-10);
}

TEST(PointLightTest, SampleReturnsCorrectDistance) {
    PointLight light(Point3(3, 4, 0), Color3(1, 1, 1), 1.0);
    Point3 hit_point(0, 0, 0);

    LightSample sample = light.sample(hit_point);

    EXPECT_NEAR(sample.distance, 5.0, 1e-10);
}

TEST(PointLightTest, SampleReturnsCorrectIntensity) {
    PointLight light(Point3(0, 10, 0), Color3(1, 0.5, 0.2), 2.0);
    Point3 hit_point(0, 0, 0);

    LightSample sample = light.sample(hit_point);

    // intensity = color * intensity_scalar
    EXPECT_NEAR(sample.intensity.r(), 2.0, 1e-10);
    EXPECT_NEAR(sample.intensity.g(), 1.0, 1e-10);
    EXPECT_NEAR(sample.intensity.b(), 0.4, 1e-10);
}

TEST(PointLightTest, SampleWithDiagonalDirection) {
    PointLight light(Point3(1, 1, 1), Color3(1, 1, 1), 1.0);
    Point3 hit_point(0, 0, 0);

    LightSample sample = light.sample(hit_point);

    double expected_dist = std::sqrt(3.0);
    EXPECT_NEAR(sample.distance, expected_dist, 1e-10);

    // Direction should be normalized
    EXPECT_NEAR(sample.direction.length(), 1.0, 1e-10);
    double inv_sqrt3 = 1.0 / std::sqrt(3.0);
    EXPECT_NEAR(sample.direction.x(), inv_sqrt3, 1e-10);
    EXPECT_NEAR(sample.direction.y(), inv_sqrt3, 1e-10);
    EXPECT_NEAR(sample.direction.z(), inv_sqrt3, 1e-10);
}

TEST(PointLightTest, AccessorsReturnConstructorValues) {
    Point3 pos(1, 2, 3);
    Color3 color(0.5, 0.6, 0.7);
    double intensity = 3.5;
    PointLight light(pos, color, intensity);

    EXPECT_DOUBLE_EQ(light.position().x(), 1.0);
    EXPECT_DOUBLE_EQ(light.position().y(), 2.0);
    EXPECT_DOUBLE_EQ(light.position().z(), 3.0);
    EXPECT_DOUBLE_EQ(light.color().r(), 0.5);
    EXPECT_DOUBLE_EQ(light.color().g(), 0.6);
    EXPECT_DOUBLE_EQ(light.color().b(), 0.7);
    EXPECT_DOUBLE_EQ(light.intensity(), 3.5);
}
