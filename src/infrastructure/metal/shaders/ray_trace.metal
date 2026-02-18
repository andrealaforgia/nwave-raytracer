#include <metal_stdlib>
using namespace metal;

struct GPUCamera {
    float3 lookfrom;        //  0
    float  _pad0;           // 12
    float3 pixel00_loc;     // 16
    float  _pad1;           // 28
    float3 pixel_delta_u;   // 32
    float  _pad2;           // 44
    float3 pixel_delta_v;   // 48
    float  _pad3;           // 60
    float3 background_top;  // 64
    float  _pad4;           // 76
    float3 background_bottom; // 80
    float  _pad5;           // 92
    uint   image_width;     // 96
    uint   image_height;    //100
    uint   samples_per_pixel; //104
    uint   max_depth;       //108
};

kernel void ray_trace_kernel(
    constant GPUCamera& camera     [[buffer(0)]],
    device float4* output          [[buffer(1)]],
    constant uchar* shapes         [[buffer(2)]],
    constant uchar* materials      [[buffer(3)]],
    constant uchar* lights         [[buffer(4)]],
    constant uint* scene_counts    [[buffer(5)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= camera.image_width || gid.y >= camera.image_height) return;

    uint idx = gid.y * camera.image_width + gid.x;

    // Compute ray for this pixel (center sample, no jitter)
    float3 pixel_center = camera.pixel00_loc
                        + float(gid.x) * camera.pixel_delta_u
                        + float(gid.y) * camera.pixel_delta_v;
    float3 ray_direction = pixel_center - camera.lookfrom;

    // Normalize direction for sky gradient
    float3 unit_dir = normalize(ray_direction);

    // Sky gradient: matches CPU formula exactly
    //   a = 0.5 * (unit_dir.y + 1.0)
    //   color = (1 - a) * background_bottom + a * background_top
    float a = 0.5f * (unit_dir.y + 1.0f);
    float3 color = (1.0f - a) * camera.background_bottom + a * camera.background_top;

    // Gamma correction (gamma 2.0) to match CPU renderer
    color = sqrt(clamp(color, 0.0f, 1.0f));

    output[idx] = float4(color, 1.0f);
}
