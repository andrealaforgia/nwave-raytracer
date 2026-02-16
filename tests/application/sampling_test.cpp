#include <gtest/gtest.h>
#include "application/renderer.h"
#include "domain/camera.h"
#include "domain/scene.h"
#include "domain/shapes/sphere.h"
#include "domain/materials/lambertian.h"
#include "domain/lights/point_light.h"
#include "core/vec3.h"
#include "core/math_utils.h"
#include <cmath>

using namespace nwave;

class SamplingTest : public ::testing::Test {
protected:
    Renderer renderer;
    std::shared_ptr<Lambertian> material =
        std::make_shared<Lambertian>(Color3(0.5, 0.5, 0.5));

    Camera make_small_camera() const {
        return Camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                      90.0, 16.0 / 9.0, 16);
    }

    Scene make_lit_sphere_scene() const {
        Scene scene;
        scene.add_shape(std::make_shared<Sphere>(
            Point3(0, 0, 0), 1.0, material.get()));
        scene.add_light(std::make_shared<PointLight>(
            Point3(-2, 3, -1), Color3(1, 1, 1), 1.0));
        return scene;
    }
};

TEST_F(SamplingTest, Spp1ProducesValidImageWithCorrectPixelCount) {
    auto camera = make_small_camera();
    auto scene = make_lit_sphere_scene();
    RenderSettings settings;
    settings.samples_per_pixel = 1;

    auto pixels = renderer.render(camera, scene, settings);

    EXPECT_EQ(static_cast<int>(pixels.size()),
              camera.image_width() * camera.image_height());

    for (const auto& pixel : pixels) {
        EXPECT_GE(pixel.r(), 0.0);
        EXPECT_LE(pixel.r(), 1.0);
        EXPECT_GE(pixel.g(), 0.0);
        EXPECT_LE(pixel.g(), 1.0);
        EXPECT_GE(pixel.b(), 0.0);
        EXPECT_LE(pixel.b(), 1.0);
    }
}

TEST_F(SamplingTest, Spp4ProducesValidImageWithCorrectPixelCount) {
    auto camera = make_small_camera();
    auto scene = make_lit_sphere_scene();
    RenderSettings settings;
    settings.samples_per_pixel = 4;

    auto pixels = renderer.render(camera, scene, settings);

    EXPECT_EQ(static_cast<int>(pixels.size()),
              camera.image_width() * camera.image_height());

    for (const auto& pixel : pixels) {
        EXPECT_GE(pixel.r(), 0.0);
        EXPECT_LE(pixel.r(), 1.0);
        EXPECT_GE(pixel.g(), 0.0);
        EXPECT_LE(pixel.g(), 1.0);
        EXPECT_GE(pixel.b(), 0.0);
        EXPECT_LE(pixel.b(), 1.0);
    }
}

TEST_F(SamplingTest, SppGreaterThan1GeneratesDifferentRaysPerSample) {
    Camera camera = make_small_camera();
    int cx = camera.image_width() / 2;
    int cy = camera.image_height() / 2;

    std::vector<Vec3> directions;
    for (int i = 0; i < 20; ++i) {
        Ray ray = camera.generate_ray_random(cx, cy);
        directions.push_back(ray.direction());
    }

    bool found_different = false;
    for (size_t i = 1; i < directions.size(); ++i) {
        Vec3 diff = directions[i] - directions[0];
        if (diff.length_squared() > 1e-12) {
            found_different = true;
            break;
        }
    }
    EXPECT_TRUE(found_different)
        << "All 20 random rays for the same pixel had identical directions";
}

TEST_F(SamplingTest, MultisampleRenderDiffersFromSingleSample) {
    auto camera = make_small_camera();
    auto scene = make_lit_sphere_scene();

    RenderSettings settings_spp1;
    settings_spp1.samples_per_pixel = 1;

    RenderSettings settings_spp8;
    settings_spp8.samples_per_pixel = 8;

    auto pixels_spp1 = renderer.render(camera, scene, settings_spp1);
    auto pixels_spp8 = renderer.render(camera, scene, settings_spp8);

    ASSERT_EQ(pixels_spp1.size(), pixels_spp8.size());

    int different_count = 0;
    for (size_t i = 0; i < pixels_spp1.size(); ++i) {
        Vec3 diff = pixels_spp1[i] - pixels_spp8[i];
        if (diff.length_squared() > 1e-6) {
            ++different_count;
        }
    }
    EXPECT_GT(different_count, 0)
        << "Multi-sample render should produce at least some pixels "
           "different from single-sample render";
}
