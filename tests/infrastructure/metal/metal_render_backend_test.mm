#include <gtest/gtest.h>
#include "infrastructure/metal/metal_render_backend.h"
#include "application/render_backend.h"
#include "domain/camera.h"
#include "domain/scene.h"
#include "core/vec3.h"

#import <Foundation/Foundation.h>

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

} // namespace
} // namespace nwave
