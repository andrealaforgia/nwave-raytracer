#include <gtest/gtest.h>
#include "core/gpu_types.h"
#include <cstddef>
#include <cstring>

// Acceptance: GPUCamera struct has correct size and alignment for Metal buffer compatibility
TEST(GPUTypesTest, GPUCameraIs112BytesAnd16ByteAligned) {
    EXPECT_EQ(sizeof(nwave::GPUCamera), 112u);
    EXPECT_GE(alignof(nwave::GPUCamera), 16u);
}

// Acceptance: GPUCamera fields are at correct offsets matching Metal buffer layout
TEST(GPUTypesTest, GPUCameraFieldOffsetsMatchMetalBufferLayout) {
    EXPECT_EQ(offsetof(nwave::GPUCamera, lookfrom),          0u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, _pad0),            12u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, pixel00_loc),      16u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, _pad1),            28u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, pixel_delta_u),    32u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, _pad2),            44u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, pixel_delta_v),    48u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, _pad3),            60u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, background_top),   64u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, _pad4),            76u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, background_bottom), 80u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, _pad5),            92u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, image_width),      96u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, image_height),    100u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, samples_per_pixel), 104u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, max_depth),       108u);
}

// Acceptance: GPUCamera can be zero-initialized and fields are writable
TEST(GPUTypesTest, GPUCameraFieldsAreReadableAndWritable) {
    nwave::GPUCamera cam{};
    cam.lookfrom[0] = 1.0f;
    cam.lookfrom[1] = 2.0f;
    cam.lookfrom[2] = 3.0f;
    cam.pixel00_loc[0] = -1.0f;
    cam.pixel_delta_u[1] = 0.5f;
    cam.pixel_delta_v[2] = -0.5f;
    cam.background_top[0] = 0.5f;
    cam.background_bottom[2] = 0.2f;
    cam.image_width = 1920;
    cam.image_height = 1080;
    cam.samples_per_pixel = 64;
    cam.max_depth = 10;

    EXPECT_FLOAT_EQ(cam.lookfrom[0], 1.0f);
    EXPECT_FLOAT_EQ(cam.lookfrom[1], 2.0f);
    EXPECT_FLOAT_EQ(cam.lookfrom[2], 3.0f);
    EXPECT_FLOAT_EQ(cam.pixel00_loc[0], -1.0f);
    EXPECT_FLOAT_EQ(cam.pixel_delta_u[1], 0.5f);
    EXPECT_FLOAT_EQ(cam.pixel_delta_v[2], -0.5f);
    EXPECT_FLOAT_EQ(cam.background_top[0], 0.5f);
    EXPECT_FLOAT_EQ(cam.background_bottom[2], 0.2f);
    EXPECT_EQ(cam.image_width, 1920u);
    EXPECT_EQ(cam.image_height, 1080u);
    EXPECT_EQ(cam.samples_per_pixel, 64u);
    EXPECT_EQ(cam.max_depth, 10u);
}
