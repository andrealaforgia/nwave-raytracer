#ifndef NWAVE_CORE_GPU_TYPES_H
#define NWAVE_CORE_GPU_TYPES_H

#include <cstdint>
#include <cstddef>

namespace nwave {

// ---------------------------------------------------------------------------
// Type-tag enums for GPU tagged unions
// ---------------------------------------------------------------------------

enum class GPUShapeType : uint32_t {
    SPHERE   = 0,
    PLANE    = 1,
    BOX      = 2,
    CYLINDER = 3,
    TRIANGLE = 4
};

enum class GPUMaterialType : uint32_t {
    LAMBERTIAN    = 0,
    METAL         = 1,
    DIELECTRIC    = 2,
    EMISSIVE      = 3,
    CHECKER_METAL = 4
};

enum class GPULightType : uint32_t {
    POINT       = 0,
    DIRECTIONAL = 1
};

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
    float    ambient_factor;       //112: shader ambient multiplier (default 0.05)
    uint32_t batch_index;          //116: current batch (0-based)
    uint32_t num_batches;          //120: total batch count (1 = no batching)
    float    _pad6;                //124: alignment padding
};                                 //128 bytes total

static_assert(sizeof(GPUCamera) == 128,
    "GPUCamera must be 128 bytes for Metal buffer compatibility");
static_assert(alignof(GPUCamera) >= 16,
    "GPUCamera must be 16-byte aligned");

/// GPU shape -- tagged union carrying per-shape parameters.
/// params[12] layout per shape type:
///   Sphere:   params[0-2]=center, params[3]=radius, params[4-11]=unused
///   Plane:    params[0-2]=point, params[3]=_pad, params[4-6]=normal, params[7-11]=unused
///   Box:      params[0-2]=bmin, params[3]=_pad, params[4-6]=bmax, params[7-11]=unused
///   Cylinder: params[0-2]=center, params[3]=radius, params[4-6]=axis, params[7]=half_height, params[8-11]=unused
///   Triangle: params[0-2]=v0, params[3]=_pad, params[4-6]=v1, params[7]=_pad, params[8-10]=v2, params[11]=_pad
struct alignas(16) GPUShape {
    uint32_t shape_type;           //  0: GPUShapeType
    uint32_t material_index;       //  4: index into GPUMaterial array
    uint32_t has_transform;        //  8: 0 or 1
    float    _pad0;                // 12: alignment padding
    float    params[12];           // 16: shape-specific parameters (48 bytes)
    float    inverse_transform[16];// 64: 4x4 inverse transform matrix (64 bytes)
};                                 //128 bytes total

static_assert(sizeof(GPUShape) == 128,
    "GPUShape must be 128 bytes for Metal buffer compatibility");
static_assert(alignof(GPUShape) >= 16,
    "GPUShape must be 16-byte aligned");

/// GPU material -- tagged union for all material types.
/// param1 usage: fuzziness (Metal), ior (Dielectric), unused (Lambertian/Emissive)
/// tint usage: tint color (Dielectric), unused for others
/// texture_offset: -1 = no texture, >= 0 = byte offset into texture data buffer
struct alignas(16) GPUMaterial {
    uint32_t material_type;   //  0: GPUMaterialType
    int32_t  texture_offset;  //  4: -1 = no texture, >= 0 = byte offset into texture data
    float    texture_width;   //  8: texture width in pixels (0 if no texture)
    float    texture_height;  // 12: texture height in pixels (0 if no texture)
    float    albedo[3];       // 16: color/albedo
    float    param1;          // 28: fuzziness (Metal), ior (Dielectric), or unused
    float    tint[3];         // 32: tint color for Dielectric, unused for others
    float    texture_scale;   // 44: UV scale for textures (0 = default/full coverage)
};                            // 48 bytes total

static_assert(sizeof(GPUMaterial) == 48,
    "GPUMaterial must be 48 bytes for Metal buffer compatibility");
static_assert(alignof(GPUMaterial) >= 16,
    "GPUMaterial must be 16-byte aligned");

/// GPU light -- tagged union for point and directional lights.
struct alignas(16) GPULight {
    uint32_t light_type;      //  0: GPULightType
    float    _pad0;           //  4: padding
    float    _pad1;           //  8: padding
    float    _pad2;           // 12: padding
    float    position[3];     // 16: position (point) or direction (directional)
    float    intensity;       // 28: light intensity
    float    color[3];        // 32: light color
    float    _pad3;           // 44: padding
    float    _reserved[4];    // 48: reserved for future use
};                            // 64 bytes total

static_assert(sizeof(GPULight) == 64,
    "GPULight must be 64 bytes for Metal buffer compatibility");
static_assert(alignof(GPULight) >= 16,
    "GPULight must be 16-byte aligned");

/// Linear BVH node for GPU traversal.
/// Interior nodes: count=0, offset=second child index.
/// Leaf nodes: count>0, offset=first primitive index.
struct alignas(16) LinearBVHNode {
    float    aabb_min[3];     //  0: bounding box minimum
    uint32_t offset;          // 12: second child (interior) or first prim (leaf)
    float    aabb_max[3];     // 16: bounding box maximum
    uint32_t count;           // 28: 0 for interior, >0 for leaf (prim count)
};                            // 32 bytes total

static_assert(sizeof(LinearBVHNode) == 32,
    "LinearBVHNode must be 32 bytes for Metal buffer compatibility");
static_assert(alignof(LinearBVHNode) >= 16,
    "LinearBVHNode must be 16-byte aligned");

} // namespace nwave

#endif // NWAVE_CORE_GPU_TYPES_H
