#include <gtest/gtest.h>
#include "infrastructure/metal/metal_render_backend.h"
#include "application/render_backend.h"
#include "application/renderer.h"
#include "domain/camera.h"
#include "domain/scene.h"
#include "core/gpu_types.h"
#include "core/vec3.h"

#import <Foundation/Foundation.h>

// Forward declaration for pack function (defined in metal_render_backend.mm)
namespace nwave {
GPUCamera pack_gpu_camera(const Camera& camera, const RenderSettings& settings);
}

namespace nwave {
namespace {

class MetalRenderBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        NSFileManager* fm = [NSFileManager defaultManager];
        NSString* path = @"nwave_shaders.metallib";
        if (![fm fileExistsAtPath:path]) {
            GTEST_SKIP() << "nwave_shaders.metallib not found (metal compiler requires Xcode)";
        }

        backend_ = std::make_unique<MetalRenderBackend>();
        if (!backend_->initialise("nwave_shaders.metallib")) {
            GTEST_SKIP() << "Failed to initialise MetalRenderBackend";
        }
    }

    std::unique_ptr<MetalRenderBackend> backend_;
};

// Acceptance: Given MetalRenderBackend
// When called via RenderBackend pointer
// Then it returns a valid vector of Color3 of correct size
TEST_F(MetalRenderBackendTest, IsCallableThroughRenderBackendInterface) {
    RenderBackend* base = backend_.get();
    Camera camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 16);
    Scene scene;
    RenderSettings settings;

    std::vector<Color3> pixels = base->render(camera, scene, settings);

    int expected_count = camera.image_width() * camera.image_height();
    ASSERT_EQ(static_cast<int>(pixels.size()), expected_count);
}

// Acceptance: Given MetalRenderBackend via RenderBackend pointer
// When render() is called with a 400x225 camera
// Then it returns exactly 400*225 Color3 values with valid data
TEST_F(MetalRenderBackendTest, RenderReturnsCorrectSizeVector) {
    RenderBackend* base = backend_.get();
    Camera camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 400);
    Scene scene;
    RenderSettings settings;

    std::vector<Color3> pixels = base->render(camera, scene, settings);

    ASSERT_EQ(pixels.size(), static_cast<size_t>(400 * 225));
    // Verify pixels contain valid (non-NaN) data
    EXPECT_FALSE(std::isnan(pixels[0].r()));
    EXPECT_FALSE(std::isnan(pixels[0].g()));
    EXPECT_FALSE(std::isnan(pixels[0].b()));
}

// Acceptance: Given MetalRenderBackend constructed multiple times
// When each instance is initialised
// Then both initialise successfully without crash (device reuse stability)
TEST_F(MetalRenderBackendTest, MultipleInstancesInitialiseWithoutCrash) {
    // backend_ is already initialised from SetUp
    MetalRenderBackend second;
    ASSERT_TRUE(second.initialise("nwave_shaders.metallib"));

    Camera camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 16);
    Scene scene;
    RenderSettings settings;

    // Both should produce valid output
    RenderBackend* base1 = backend_.get();
    RenderBackend* base2 = &second;

    auto pixels1 = base1->render(camera, scene, settings);
    auto pixels2 = base2->render(camera, scene, settings);

    int expected_count = camera.image_width() * camera.image_height();
    EXPECT_EQ(static_cast<int>(pixels1.size()), expected_count);
    EXPECT_EQ(static_cast<int>(pixels2.size()), expected_count);
}

// ---------------------------------------------------------------------------
// Step 03-01: GPUCamera packing and ray generation shader
// ---------------------------------------------------------------------------

// Helper: compute CPU sky gradient for a given camera and settings
static std::vector<Color3> cpu_sky_gradient(const Camera& camera,
                                            const RenderSettings& settings) {
    Renderer renderer;
    renderer.set_quiet(true);
    RenderSettings cpu_settings = settings;
    cpu_settings.samples_per_pixel = 1;
    cpu_settings.num_threads = 1;
    Scene empty_scene;
    return renderer.render(camera, empty_scene, cpu_settings);
}

// Helper: compare two pixel buffers; max per-channel diff in 0-255 range
static int max_channel_diff(const std::vector<Color3>& a,
                            const std::vector<Color3>& b) {
    int worst = 0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        for (int c = 0; c < 3; ++c) {
            int va = static_cast<int>(std::round(a[i][c] * 255.0));
            int vb = static_cast<int>(std::round(b[i][c] * 255.0));
            worst = std::max(worst, std::abs(va - vb));
        }
    }
    return worst;
}

// Acceptance: Given a Camera with known parameters
// When packed into GPUCamera
// Then float fields match Camera members within 1e-5 tolerance
// NOTE: This test does NOT require Metal -- pure C++ packing function.
TEST(GPUCameraPackingTest, PackingMatchesCameraParameters) {
    Camera camera(Point3(1.0, 2.0, -3.0), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 400);
    RenderSettings settings;
    settings.samples_per_pixel = 4;
    settings.max_depth = 8;
    settings.background_top = Color3(0.5, 0.7, 1.0);
    settings.background_bottom = Color3(1.0, 1.0, 1.0);

    GPUCamera gpu = pack_gpu_camera(camera, settings);

    // Verify lookfrom
    EXPECT_NEAR(gpu.lookfrom[0], 1.0f, 1e-5f);
    EXPECT_NEAR(gpu.lookfrom[1], 2.0f, 1e-5f);
    EXPECT_NEAR(gpu.lookfrom[2], -3.0f, 1e-5f);

    // Verify pixel00_loc matches camera getter
    EXPECT_NEAR(gpu.pixel00_loc[0], static_cast<float>(camera.pixel00_loc().x()), 1e-5f);
    EXPECT_NEAR(gpu.pixel00_loc[1], static_cast<float>(camera.pixel00_loc().y()), 1e-5f);
    EXPECT_NEAR(gpu.pixel00_loc[2], static_cast<float>(camera.pixel00_loc().z()), 1e-5f);

    // Verify pixel_delta_u matches camera getter
    EXPECT_NEAR(gpu.pixel_delta_u[0], static_cast<float>(camera.pixel_delta_u().x()), 1e-5f);
    EXPECT_NEAR(gpu.pixel_delta_u[1], static_cast<float>(camera.pixel_delta_u().y()), 1e-5f);
    EXPECT_NEAR(gpu.pixel_delta_u[2], static_cast<float>(camera.pixel_delta_u().z()), 1e-5f);

    // Verify pixel_delta_v matches camera getter
    EXPECT_NEAR(gpu.pixel_delta_v[0], static_cast<float>(camera.pixel_delta_v().x()), 1e-5f);
    EXPECT_NEAR(gpu.pixel_delta_v[1], static_cast<float>(camera.pixel_delta_v().y()), 1e-5f);
    EXPECT_NEAR(gpu.pixel_delta_v[2], static_cast<float>(camera.pixel_delta_v().z()), 1e-5f);

    // Verify background colors
    EXPECT_NEAR(gpu.background_top[0], 0.5f, 1e-5f);
    EXPECT_NEAR(gpu.background_top[1], 0.7f, 1e-5f);
    EXPECT_NEAR(gpu.background_top[2], 1.0f, 1e-5f);
    EXPECT_NEAR(gpu.background_bottom[0], 1.0f, 1e-5f);
    EXPECT_NEAR(gpu.background_bottom[1], 1.0f, 1e-5f);
    EXPECT_NEAR(gpu.background_bottom[2], 1.0f, 1e-5f);

    // Verify integer fields
    EXPECT_EQ(gpu.image_width, 400u);
    EXPECT_EQ(gpu.image_height, static_cast<uint32_t>(camera.image_height()));
    EXPECT_EQ(gpu.samples_per_pixel, 4u);
    EXPECT_EQ(gpu.max_depth, 8u);
}

// Acceptance: Given an empty scene at 400x225, 1 SPP
// When rendered via CPU and GPU
// Then per-pixel RGB difference is at most 1 (float rounding)
TEST_F(MetalRenderBackendTest, SkyGradientMatchesCpuForEmptyScene) {
    Camera camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 400);
    Scene empty_scene;
    RenderSettings settings;
    settings.samples_per_pixel = 1;

    auto gpu_pixels = backend_->render(camera, empty_scene, settings);
    auto cpu_pixels = cpu_sky_gradient(camera, settings);

    ASSERT_EQ(gpu_pixels.size(), cpu_pixels.size());
    int diff = max_channel_diff(gpu_pixels, cpu_pixels);
    EXPECT_LE(diff, 1) << "Max per-channel diff (0-255) was " << diff;
}

// Acceptance: Given a camera with vfov=20 (narrow)
// When rendered via GPU
// Then the sky gradient matches CPU narrow-FOV output
TEST_F(MetalRenderBackendTest, NarrowFovSkyGradientMatchesCpu) {
    Camera camera(Point3(0, 0, -10), Point3(0, 0, 0), Vec3(0, 1, 0),
                  20.0, 16.0 / 9.0, 400);
    Scene empty_scene;
    RenderSettings settings;
    settings.samples_per_pixel = 1;

    auto gpu_pixels = backend_->render(camera, empty_scene, settings);
    auto cpu_pixels = cpu_sky_gradient(camera, settings);

    ASSERT_EQ(gpu_pixels.size(), cpu_pixels.size());
    int diff = max_channel_diff(gpu_pixels, cpu_pixels);
    EXPECT_LE(diff, 1) << "Max per-channel diff (0-255) was " << diff;
}

// Acceptance: Given a camera with vup=(1,0,0)
// When rendered via GPU
// Then the gradient rotates accordingly, matching CPU
TEST_F(MetalRenderBackendTest, RotatedVupSkyGradientMatchesCpu) {
    Camera camera(Point3(0, 0, -2), Point3(0, 0, 0), Vec3(1, 0, 0),
                  90.0, 16.0 / 9.0, 400);
    Scene empty_scene;
    RenderSettings settings;
    settings.samples_per_pixel = 1;

    auto gpu_pixels = backend_->render(camera, empty_scene, settings);
    auto cpu_pixels = cpu_sky_gradient(camera, settings);

    ASSERT_EQ(gpu_pixels.size(), cpu_pixels.size());
    int diff = max_channel_diff(gpu_pixels, cpu_pixels);
    EXPECT_LE(diff, 1) << "Max per-channel diff (0-255) was " << diff;
}

} // namespace
} // namespace nwave
