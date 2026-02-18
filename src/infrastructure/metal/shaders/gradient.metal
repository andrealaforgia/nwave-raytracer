#include <metal_stdlib>
using namespace metal;

kernel void gradient_kernel(
    device float4* output [[buffer(0)]],
    constant uint2& dimensions [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= dimensions.x || gid.y >= dimensions.y) return;
    uint idx = gid.y * dimensions.x + gid.x;
    float u = float(gid.x) / float(dimensions.x - 1);
    output[idx] = float4(0.0, 0.0, u, 1.0);
}
