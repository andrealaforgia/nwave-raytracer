#ifndef NWAVE_CORE_GPU_TYPES_H
#define NWAVE_CORE_GPU_TYPES_H

#include <cstdint>

namespace nwave {

/// GPU camera buffer layout for Metal compute shaders.
/// Fields are packed with explicit padding for 16-byte alignment
/// to match Metal's float4 alignment requirements.
struct alignas(16) GPUCamera {
    float    lookfrom[3];          //  0: camera position
    float    _pad0;                // 12: alignment padding
    float    pixel00_loc[3];       // 16: world position of pixel (0,0) center
    float    _pad1;                // 28: alignment padding
    float    pixel_delta_u[3];     // 32: per-pixel offset in U direction
    float    _pad2;                // 44: alignment padding
    float    pixel_delta_v[3];     // 48: per-pixel offset in V direction
    float    _pad3;                // 60: alignment padding
    float    background_top[3];    // 64: sky gradient top color
    float    _pad4;                // 76: alignment padding
    float    background_bottom[3]; // 80: sky gradient bottom color
    float    _pad5;                // 92: alignment padding
    uint32_t image_width;          // 96: image width in pixels
    uint32_t image_height;         //100: image height in pixels
    uint32_t samples_per_pixel;    //104: SPP count
    uint32_t max_depth;            //108: maximum ray bounce depth
};                                 //112 bytes total

static_assert(sizeof(GPUCamera) == 112,
    "GPUCamera must be 112 bytes for Metal buffer compatibility");
static_assert(alignof(GPUCamera) >= 16,
    "GPUCamera must be 16-byte aligned");

} // namespace nwave

#endif // NWAVE_CORE_GPU_TYPES_H
