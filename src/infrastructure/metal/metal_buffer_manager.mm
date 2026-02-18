#include "infrastructure/metal/metal_buffer_manager.h"
#include "infrastructure/metal/metal_device.h"

#import <Metal/Metal.h>

namespace nwave {

struct MetalBufferManager::Impl {
    MetalDevice& device;

    explicit Impl(MetalDevice& dev) : device(dev) {}
};

MetalBufferManager::MetalBufferManager(MetalDevice& device)
    : impl_(std::make_unique<Impl>(device)) {
}

MetalBufferManager::~MetalBufferManager() = default;

std::vector<Color3> MetalBufferManager::dispatch_gradient(int width, int height) {
    auto* mtl_device = (__bridge id<MTLDevice>)impl_->device.native_device();
    auto* command_queue = (__bridge id<MTLCommandQueue>)impl_->device.native_command_queue();
    auto* pipeline = (__bridge id<MTLComputePipelineState>)impl_->device.native_pipeline("gradient_kernel");

    if (!mtl_device || !command_queue || !pipeline) {
        return {};
    }

    const NSUInteger pixel_count = static_cast<NSUInteger>(width) * static_cast<NSUInteger>(height);
    const NSUInteger buffer_size = pixel_count * sizeof(float) * 4; // float4 per pixel

    id<MTLBuffer> output_buffer = [mtl_device newBufferWithLength:buffer_size
                                                          options:MTLResourceStorageModeShared];
    if (!output_buffer) {
        return {};
    }

    // Prepare dimensions constant
    uint32_t dimensions[2] = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];

    [encoder setComputePipelineState:pipeline];
    [encoder setBuffer:output_buffer offset:0 atIndex:0];
    [encoder setBytes:dimensions length:sizeof(dimensions) atIndex:1];

    // Threadgroup sizing: use 16x16 but handle non-power-of-2 dimensions
    MTLSize threadgroup_size = MTLSizeMake(16, 16, 1);
    MTLSize grid_size = MTLSizeMake(
        static_cast<NSUInteger>(width),
        static_cast<NSUInteger>(height),
        1
    );

    [encoder dispatchThreads:grid_size threadsPerThreadgroup:threadgroup_size];
    [encoder endEncoding];
    [command_buffer commit];
    [command_buffer waitUntilCompleted];

    // Read back float4 data and convert to Color3
    const float* raw = static_cast<const float*>([output_buffer contents]);
    std::vector<Color3> pixels(pixel_count);

    for (NSUInteger i = 0; i < pixel_count; ++i) {
        const float* px = raw + i * 4;
        pixels[i] = Color3(
            static_cast<double>(px[0]),
            static_cast<double>(px[1]),
            static_cast<double>(px[2])
        );
    }

    return pixels;
}

} // namespace nwave
