#include <gtest/gtest.h>
#include "infrastructure/metal/metal_buffer_manager.h"
#include "infrastructure/metal/metal_device.h"

#import <Foundation/Foundation.h>

namespace nwave {
namespace {

class MetalBufferManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        device_ = std::make_unique<MetalDevice>();
        if (!device_->is_available()) {
            GTEST_SKIP() << "Metal device not available";
        }

        NSFileManager* fm = [NSFileManager defaultManager];
        NSString* path = @"nwave_shaders.metallib";
        if (![fm fileExistsAtPath:path]) {
            GTEST_SKIP() << "nwave_shaders.metallib not found (metal compiler requires Xcode)";
        }

        if (!device_->load_library("nwave_shaders.metallib")) {
            GTEST_SKIP() << "Failed to load metallib";
        }

        if (!device_->create_pipeline("gradient_kernel")) {
            GTEST_SKIP() << "Failed to create gradient_kernel pipeline";
        }
    }

    std::unique_ptr<MetalDevice> device_;
};

// Acceptance: Given a 400x225 dispatch
// When MetalBufferManager reads back the output
// Then the result contains 400*225 Color3 values
TEST_F(MetalBufferManagerTest, DispatchGradientReturnsCorrectPixelCount) {
    MetalBufferManager manager(*device_);

    auto pixels = manager.dispatch_gradient(400, 225);

    EXPECT_EQ(pixels.size(), 400u * 225u);
}

// Acceptance: Given the gradient kernel dispatch at 400x225
// When pixel (0, 112) is read
// Then RGB values are near black (each channel < 0.05)
TEST_F(MetalBufferManagerTest, PixelAtLeftCenterIsNearBlack) {
    MetalBufferManager manager(*device_);

    auto pixels = manager.dispatch_gradient(400, 225);
    ASSERT_EQ(pixels.size(), 400u * 225u);

    // pixel (x=0, y=112) is at index y*width + x = 112*400 + 0
    const auto& pixel = pixels[112 * 400 + 0];
    EXPECT_LT(pixel.r(), 0.05) << "Red channel should be near zero";
    EXPECT_LT(pixel.g(), 0.05) << "Green channel should be near zero";
    EXPECT_LT(pixel.b(), 0.05) << "Blue channel should be near zero";
}

// Acceptance: Given the gradient kernel dispatch at 400x225
// When pixel (399, 112) is read
// Then the blue channel is > 0.8
TEST_F(MetalBufferManagerTest, PixelAtRightCenterHasHighBlueChannel) {
    MetalBufferManager manager(*device_);

    auto pixels = manager.dispatch_gradient(400, 225);
    ASSERT_EQ(pixels.size(), 400u * 225u);

    // pixel (x=399, y=112) is at index y*width + x = 112*400 + 399
    const auto& pixel = pixels[112 * 400 + 399];
    EXPECT_GT(pixel.b(), 0.8) << "Blue channel should be > 0.8 at right edge";
}

// Acceptance: Given a non-power-of-2 dimension (401x225)
// When dispatched
// Then no pixels are missing and the image is complete
TEST_F(MetalBufferManagerTest, NonPowerOfTwoDimensionsProduceCompleteImage) {
    MetalBufferManager manager(*device_);

    auto pixels = manager.dispatch_gradient(401, 225);
    EXPECT_EQ(pixels.size(), 401u * 225u);

    // Verify the last pixel in the last row is valid (non-NaN)
    const auto& last_pixel = pixels[224 * 401 + 400];
    EXPECT_FALSE(std::isnan(last_pixel.r()));
    EXPECT_FALSE(std::isnan(last_pixel.g()));
    EXPECT_FALSE(std::isnan(last_pixel.b()));

    // Right edge should still have high blue
    const auto& right_edge = pixels[112 * 401 + 400];
    EXPECT_GT(right_edge.b(), 0.8) << "Blue channel at right edge of 401-wide image";
}

} // namespace
} // namespace nwave
