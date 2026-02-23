#include <gtest/gtest.h>
#include "core/gpu_types.h"
#include <cstddef>
#include <cstring>

// Acceptance: GPUCamera struct has correct size and alignment for Metal buffer compatibility
TEST(GPUTypesTest, GPUCameraIs128BytesAnd16ByteAligned) {
    EXPECT_EQ(sizeof(nwave::GPUCamera), 128u);
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
    EXPECT_EQ(offsetof(nwave::GPUCamera, ambient_factor),  112u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, batch_index),     116u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, num_batches),     120u);
    EXPECT_EQ(offsetof(nwave::GPUCamera, _pad6),           124u);
}

// Acceptance: GPUShape struct has correct size and alignment for Metal buffer compatibility
TEST(GPUTypesTest, GPUShapeIs128BytesAnd16ByteAligned) {
    EXPECT_EQ(sizeof(nwave::GPUShape), 128u);
    EXPECT_GE(alignof(nwave::GPUShape), 16u);
}

// Acceptance: GPUMaterial struct has correct size and alignment for Metal buffer compatibility
TEST(GPUTypesTest, GPUMaterialIs48BytesAnd16ByteAligned) {
    EXPECT_EQ(sizeof(nwave::GPUMaterial), 48u);
    EXPECT_GE(alignof(nwave::GPUMaterial), 16u);
}

// Acceptance: GPULight struct has correct size and alignment for Metal buffer compatibility
TEST(GPUTypesTest, GPULightIs64BytesAnd16ByteAligned) {
    EXPECT_EQ(sizeof(nwave::GPULight), 64u);
    EXPECT_GE(alignof(nwave::GPULight), 16u);
}

// Acceptance: LinearBVHNode struct has correct size and alignment for Metal buffer compatibility
TEST(GPUTypesTest, LinearBVHNodeIs32BytesAnd16ByteAligned) {
    EXPECT_EQ(sizeof(nwave::LinearBVHNode), 32u);
    EXPECT_GE(alignof(nwave::LinearBVHNode), 16u);
}

// Acceptance: GPUShape fields are at correct offsets matching Metal buffer layout
TEST(GPUTypesTest, GPUShapeFieldOffsetsMatchMetalLayout) {
    EXPECT_EQ(offsetof(nwave::GPUShape, shape_type),         0u);
    EXPECT_EQ(offsetof(nwave::GPUShape, material_index),     4u);
    EXPECT_EQ(offsetof(nwave::GPUShape, has_transform),      8u);
    EXPECT_EQ(offsetof(nwave::GPUShape, _pad0),             12u);
    EXPECT_EQ(offsetof(nwave::GPUShape, params),            16u);
    EXPECT_EQ(offsetof(nwave::GPUShape, inverse_transform), 64u);
}

// Acceptance: GPUMaterial fields are at correct offsets matching Metal buffer layout
TEST(GPUTypesTest, GPUMaterialFieldOffsetsMatchMetalLayout) {
    EXPECT_EQ(offsetof(nwave::GPUMaterial, material_type),   0u);
    EXPECT_EQ(offsetof(nwave::GPUMaterial, texture_offset),  4u);
    EXPECT_EQ(offsetof(nwave::GPUMaterial, texture_width),   8u);
    EXPECT_EQ(offsetof(nwave::GPUMaterial, texture_height), 12u);
    EXPECT_EQ(offsetof(nwave::GPUMaterial, albedo),         16u);
    EXPECT_EQ(offsetof(nwave::GPUMaterial, param1),         28u);
    EXPECT_EQ(offsetof(nwave::GPUMaterial, tint),           32u);
    EXPECT_EQ(offsetof(nwave::GPUMaterial, texture_scale),  44u);
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
