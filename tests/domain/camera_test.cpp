#include <gtest/gtest.h>
#include "domain/camera.h"
#include "core/vec3.h"
#include "core/ray.h"
#include "core/math_utils.h"
#include <cmath>

using namespace nwave;

TEST(CameraTest, ImageDimensionsMatchConstructorArgs) {
    Camera camera(Point3(0, 0, 0), Point3(0, 0, -1), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 400);

    EXPECT_EQ(camera.image_width(), 400);
    EXPECT_EQ(camera.image_height(), 225);
}

TEST(CameraTest, ImageHeightClampsToAtLeastOne) {
    // Extreme aspect ratio to push height below 1
    Camera camera(Point3(0, 0, 0), Point3(0, 0, -1), Vec3(0, 1, 0),
                  90.0, 10000.0, 1);

    EXPECT_EQ(camera.image_width(), 1);
    EXPECT_GE(camera.image_height(), 1);
}

TEST(CameraTest, LookfromAccessorReturnsConstructorValue) {
    Point3 lookfrom(1, 2, 3);
    Camera camera(lookfrom, Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 400);

    EXPECT_DOUBLE_EQ(camera.lookfrom().x(), 1.0);
    EXPECT_DOUBLE_EQ(camera.lookfrom().y(), 2.0);
    EXPECT_DOUBLE_EQ(camera.lookfrom().z(), 3.0);
}

TEST(CameraTest, CenterRayPointsAlongNegativeZForForwardLookingCamera) {
    // Camera at z=-2 looking at origin
    Camera camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 400);

    int cx = camera.image_width() / 2;
    int cy = camera.image_height() / 2;
    Ray ray = camera.generate_ray(cx, cy);

    // Origin should be the camera position
    EXPECT_DOUBLE_EQ(ray.origin().x(), 0.0);
    EXPECT_DOUBLE_EQ(ray.origin().y(), 0.0);
    EXPECT_DOUBLE_EQ(ray.origin().z(), -2.0);

    // Direction should point roughly along +z (toward the lookat point)
    Vec3 dir = normalize(ray.direction());
    EXPECT_NEAR(dir.x(), 0.0, 0.02);
    EXPECT_NEAR(dir.y(), 0.0, 0.02);
    EXPECT_GT(dir.z(), 0.9);
}

TEST(CameraTest, CornerRaysDivergeFromCenterRay) {
    Camera camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 400);

    Ray center_ray = camera.generate_ray(camera.image_width() / 2,
                                          camera.image_height() / 2);
    Ray corner_ray = camera.generate_ray(0, 0);

    Vec3 center_dir = normalize(center_ray.direction());
    Vec3 corner_dir = normalize(corner_ray.direction());

    // Corner ray should point in a different direction than center
    double cos_angle = dot(center_dir, corner_dir);
    EXPECT_LT(cos_angle, 0.99); // Significant angular difference
    EXPECT_GT(cos_angle, 0.0);  // But still roughly forward
}

TEST(CameraTest, DifferentVfovProducesDifferentRaySpread) {
    Camera narrow(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  30.0, 16.0 / 9.0, 400);
    Camera wide(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                120.0, 16.0 / 9.0, 400);

    // Compare corner rays: wider FOV should diverge more from center
    Ray narrow_corner = narrow.generate_ray(0, 0);
    Ray narrow_center = narrow.generate_ray(narrow.image_width() / 2,
                                             narrow.image_height() / 2);
    Ray wide_corner = wide.generate_ray(0, 0);
    Ray wide_center = wide.generate_ray(wide.image_width() / 2,
                                         wide.image_height() / 2);

    double narrow_cos = dot(normalize(narrow_corner.direction()),
                            normalize(narrow_center.direction()));
    double wide_cos = dot(normalize(wide_corner.direction()),
                          normalize(wide_center.direction()));

    // Wider FOV means corner ray diverges more (smaller cosine)
    EXPECT_GT(narrow_cos, wide_cos);
}
