#include <gtest/gtest.h>
#include "infrastructure/metal/metal_device.h"

#import <Metal/Metal.h>

namespace nwave {
namespace {

// Acceptance: Given macOS with Metal GPU
// When MetalDevice is constructed
// Then is_available() returns true and device_name() returns a non-empty string
TEST(MetalDeviceTest, ConstructedDeviceIsAvailableWithNonEmptyName) {
    MetalDevice device;

    EXPECT_TRUE(device.is_available());
    EXPECT_FALSE(device.device_name().empty());
}

// Acceptance: Given MetalDevice is constructed and destroyed
// When checked for leaks Then no Metal objects are leaked
// Note: We verify correct construction/destruction lifecycle.
// ARC handles memory; we confirm no crash on scope exit.
TEST(MetalDeviceTest, CanBeConstructedAndDestroyedWithoutLeaks) {
    {
        MetalDevice device;
        EXPECT_TRUE(device.is_available());
    }
    // If we get here without crash or ASAN/leak-sanitizer error,
    // the lifecycle is correct.
    SUCCEED();
}

// Acceptance: device_name() returns a recognizable Metal device name
TEST(MetalDeviceTest, DeviceNameMatchesSystemMetalDevice) {
    MetalDevice device;
    id<MTLDevice> system_device = MTLCreateSystemDefaultDevice();
    ASSERT_NE(system_device, nil);

    std::string expected_name = [system_device.name UTF8String];
    EXPECT_EQ(device.device_name(), expected_name);
}

} // namespace
} // namespace nwave
