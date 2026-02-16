#include <gtest/gtest.h>
#include "application/renderer.h"
#include "domain/camera.h"
#include "domain/scene.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/materials/lambertian.h"
#include "domain/lights/point_light.h"
#include "core/vec3.h"
#include "core/math_utils.h"
#include <cmath>

using namespace nwave;

class RendererTest : public ::testing::Test {
protected:
    Renderer renderer;
    RenderSettings settings;

    RendererTest() {
        settings.samples_per_pixel = 1;
        settings.max_depth = 10;
        settings.background_top = Color3(0.5, 0.7, 1.0);
        settings.background_bottom = Color3(1.0, 1.0, 1.0);
    }
};

TEST_F(RendererTest, TraceRayReturnsSkyGradientForMiss) {
    Scene scene;
    // Ray pointing straight up
    Ray ray(Point3(0, 0, 0), Vec3(0, 1, 0));

    Color3 color = renderer.trace_ray(ray, scene, settings, settings.max_depth);

    // Straight up: unit_dir.y = 1.0, a = 0.5*(1+1) = 1.0
    // color = 0*white + 1*sky_blue = (0.5, 0.7, 1.0)
    EXPECT_NEAR(color.r(), 0.5, 0.01);
    EXPECT_NEAR(color.g(), 0.7, 0.01);
    EXPECT_NEAR(color.b(), 1.0, 0.01);
}

TEST_F(RendererTest, TraceRayReturnsSkyGradientHorizon) {
    Scene scene;
    // Ray pointing along +x (horizontal)
    Ray ray(Point3(0, 0, 0), Vec3(1, 0, 0));

    Color3 color = renderer.trace_ray(ray, scene, settings, settings.max_depth);

    // Horizontal: unit_dir.y = 0, a = 0.5*(0+1) = 0.5
    // color = 0.5*white + 0.5*sky_blue = (0.75, 0.85, 1.0)
    EXPECT_NEAR(color.r(), 0.75, 0.01);
    EXPECT_NEAR(color.g(), 0.85, 0.01);
    EXPECT_NEAR(color.b(), 1.0, 0.01);
}

TEST_F(RendererTest, TraceRayReturnsBlackAtDepthZero) {
    Scene scene;
    auto mat = std::make_shared<Lambertian>(Color3(0.5, 0.5, 0.5));
    scene.add_shape(std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, mat.get()));

    Ray ray(Point3(0, 0, -5), Vec3(0, 0, 1));
    Color3 color = renderer.trace_ray(ray, scene, settings, 0);

    EXPECT_DOUBLE_EQ(color.r(), 0.0);
    EXPECT_DOUBLE_EQ(color.g(), 0.0);
    EXPECT_DOUBLE_EQ(color.b(), 0.0);
}

TEST_F(RendererTest, TraceRayReturnsShadedColorForSphereHit) {
    auto red_mat = std::make_shared<Lambertian>(Color3(0.7, 0.1, 0.1));
    Scene scene;
    scene.add_shape(std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, red_mat.get()));
    scene.add_light(std::make_shared<PointLight>(Point3(0, 5, 0), Color3(1, 1, 1), 1.0));

    // Ray from -z hitting the sphere
    Ray ray(Point3(0, 0, -5), Vec3(0, 0, 1));
    Color3 color = renderer.trace_ray(ray, scene, settings, settings.max_depth);

    // Should have some color (not black, not sky)
    EXPECT_GT(color.r(), 0.0);
    // Red component should dominate because of red material
    EXPECT_GT(color.r(), color.g());
    EXPECT_GT(color.r(), color.b());
}

TEST_F(RendererTest, RenderProducesPixelBufferOfCorrectSize) {
    Camera camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 80);
    Scene scene;

    auto pixels = renderer.render(camera, scene, settings);

    EXPECT_EQ(static_cast<int>(pixels.size()),
              camera.image_width() * camera.image_height());
}

TEST_F(RendererTest, CenterPixelIsReddishWhenSphereInCenter) {
    Camera camera(Point3(0, 0, -1.5), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 80);

    auto red_mat = std::make_shared<Lambertian>(Color3(0.7, 0.1, 0.1));
    Scene scene;
    scene.add_shape(std::make_shared<Sphere>(Point3(0, 0, 0), 0.5, red_mat.get()));
    scene.add_light(std::make_shared<PointLight>(Point3(-2, 3, -1), Color3(1, 1, 1), 1.0));

    auto pixels = renderer.render(camera, scene, settings);

    int cx = camera.image_width() / 2;
    int cy = camera.image_height() / 2;
    Color3 center_pixel = pixels[cy * camera.image_width() + cx];

    // Center pixel should be reddish (R > G and R > B)
    EXPECT_GT(center_pixel.r(), center_pixel.g());
    EXPECT_GT(center_pixel.r(), center_pixel.b());
}

TEST_F(RendererTest, TopPixelIsBluishSkyColor) {
    Camera camera(Point3(0, 0, -1.5), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 80);
    Scene scene;
    // No objects - only sky

    auto pixels = renderer.render(camera, scene, settings);

    int cx = camera.image_width() / 2;
    Color3 top_pixel = pixels[0 * camera.image_width() + cx];

    // Top pixel should be bluish (sky gradient, B >= R and B >= G)
    EXPECT_GE(top_pixel.b(), top_pixel.r());
    EXPECT_GE(top_pixel.b(), top_pixel.g());
}
