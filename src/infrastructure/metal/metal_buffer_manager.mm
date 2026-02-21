#include "infrastructure/metal/metal_buffer_manager.h"
#include "infrastructure/metal/metal_device.h"

#import <Metal/Metal.h>

namespace nwave {

struct MetalBufferManager::Impl {
    MetalDevice& device;

    // Cached GPU buffers reused across animation frames
    id<MTLBuffer> cached_output_buffer;
    NSUInteger cached_output_pixel_count;
    id<MTLBuffer> cached_texture_buffer;
    NSUInteger cached_texture_size;

    explicit Impl(MetalDevice& dev)
        : device(dev)
        , cached_output_buffer(nil)
        , cached_output_pixel_count(0)
        , cached_texture_buffer(nil)
        , cached_texture_size(0) {}
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
    std::vector<Color3> pixels(pixel_count);

    @autoreleasepool {
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

        for (NSUInteger i = 0; i < pixel_count; ++i) {
            const float* px = raw + i * 4;
            pixels[i] = Color3(
                static_cast<double>(px[0]),
                static_cast<double>(px[1]),
                static_cast<double>(px[2])
            );
        }
    } // @autoreleasepool

    return pixels;
}

std::vector<Color3> MetalBufferManager::dispatch_ray_trace(const GPUCamera& camera) {
    auto* mtl_device = (__bridge id<MTLDevice>)impl_->device.native_device();
    auto* command_queue = (__bridge id<MTLCommandQueue>)impl_->device.native_command_queue();
    auto* pipeline = (__bridge id<MTLComputePipelineState>)impl_->device.native_pipeline("ray_trace_kernel");

    if (!mtl_device || !command_queue || !pipeline) {
        return {};
    }

    const NSUInteger width = camera.image_width;
    const NSUInteger height = camera.image_height;
    const NSUInteger pixel_count = width * height;
    std::vector<Color3> pixels(pixel_count);

    @autoreleasepool {
        const NSUInteger buffer_size = pixel_count * sizeof(float) * 4; // float4 per pixel

        id<MTLBuffer> output_buffer = [mtl_device newBufferWithLength:buffer_size
                                                              options:MTLResourceStorageModeShared];
        if (!output_buffer) {
            return {};
        }

        // Empty scene: dummy buffers for shapes, materials, lights, bvh
        id<MTLBuffer> dummy_buffer = [mtl_device newBufferWithLength:1
                                                            options:MTLResourceStorageModeShared];

        // Scene counts: all zeros (no shapes, materials, lights, bvh nodes)
        uint32_t scene_counts[4] = {0, 0, 0, 0};

        id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];

        [encoder setComputePipelineState:pipeline];
        [encoder setBytes:&camera length:sizeof(GPUCamera) atIndex:0];
        [encoder setBuffer:output_buffer offset:0 atIndex:1];
        [encoder setBuffer:dummy_buffer offset:0 atIndex:2];  // shapes
        [encoder setBuffer:dummy_buffer offset:0 atIndex:3];  // materials
        [encoder setBuffer:dummy_buffer offset:0 atIndex:4];  // lights
        [encoder setBytes:scene_counts length:sizeof(scene_counts) atIndex:5];
        [encoder setBuffer:dummy_buffer offset:0 atIndex:6];  // bvh_nodes
        [encoder setBuffer:dummy_buffer offset:0 atIndex:7];  // texture_data

        MTLSize threadgroup_size = MTLSizeMake(16, 16, 1);
        MTLSize grid_size = MTLSizeMake(width, height, 1);

        [encoder dispatchThreads:grid_size threadsPerThreadgroup:threadgroup_size];
        [encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];

        // Read back float4 data and convert to Color3
        const float* raw = static_cast<const float*>([output_buffer contents]);

        for (NSUInteger i = 0; i < pixel_count; ++i) {
            const float* px = raw + i * 4;
            pixels[i] = Color3(
                static_cast<double>(px[0]),
                static_cast<double>(px[1]),
                static_cast<double>(px[2])
            );
        }
    } // @autoreleasepool

    return pixels;
}

std::vector<Color3> MetalBufferManager::dispatch_ray_trace(const GPUCamera& camera,
                                                           const FlatScene& scene) {
    auto* mtl_device = (__bridge id<MTLDevice>)impl_->device.native_device();
    auto* command_queue = (__bridge id<MTLCommandQueue>)impl_->device.native_command_queue();
    auto* pipeline = (__bridge id<MTLComputePipelineState>)impl_->device.native_pipeline("ray_trace_kernel");

    if (!mtl_device || !command_queue || !pipeline) {
        return {};
    }

    const NSUInteger width = camera.image_width;
    const NSUInteger height = camera.image_height;
    const NSUInteger pixel_count = width * height;
    std::vector<Color3> pixels(pixel_count);

    @autoreleasepool {
        const NSUInteger output_buffer_size = pixel_count * sizeof(float) * 4;

        id<MTLBuffer> output_buffer = [mtl_device newBufferWithLength:output_buffer_size
                                                              options:MTLResourceStorageModeShared];
        if (!output_buffer) {
            return {};
        }

        // Create scene data buffers (Metal requires non-zero buffer sizes)
        NSUInteger shapes_size = scene.shapes.empty()
            ? 1 : scene.shapes.size() * sizeof(GPUShape);
        NSUInteger materials_size = scene.materials.empty()
            ? 1 : scene.materials.size() * sizeof(GPUMaterial);
        NSUInteger lights_size = scene.lights.empty()
            ? 1 : scene.lights.size() * sizeof(GPULight);

        id<MTLBuffer> shapes_buffer = scene.shapes.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:scene.shapes.data()
                                      length:shapes_size
                                     options:MTLResourceStorageModeShared];

        id<MTLBuffer> materials_buffer = scene.materials.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:scene.materials.data()
                                      length:materials_size
                                     options:MTLResourceStorageModeShared];

        id<MTLBuffer> lights_buffer = scene.lights.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:scene.lights.data()
                                      length:lights_size
                                     options:MTLResourceStorageModeShared];

        uint32_t scene_counts[4] = {
            static_cast<uint32_t>(scene.shapes.size()),
            static_cast<uint32_t>(scene.materials.size()),
            static_cast<uint32_t>(scene.lights.size()),
            0
        };

        id<MTLBuffer> bvh_buffer = [mtl_device newBufferWithLength:1
                                                           options:MTLResourceStorageModeShared];

        NSUInteger tex_size = scene.texture_data.empty()
            ? 1 : scene.texture_data.size();
        id<MTLBuffer> tex_buffer = scene.texture_data.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:scene.texture_data.data()
                                      length:tex_size
                                     options:MTLResourceStorageModeShared];

        id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];

        [encoder setComputePipelineState:pipeline];
        [encoder setBytes:&camera length:sizeof(GPUCamera) atIndex:0];
        [encoder setBuffer:output_buffer offset:0 atIndex:1];
        [encoder setBuffer:shapes_buffer offset:0 atIndex:2];
        [encoder setBuffer:materials_buffer offset:0 atIndex:3];
        [encoder setBuffer:lights_buffer offset:0 atIndex:4];
        [encoder setBytes:scene_counts length:sizeof(scene_counts) atIndex:5];
        [encoder setBuffer:bvh_buffer offset:0 atIndex:6];
        [encoder setBuffer:tex_buffer offset:0 atIndex:7];

        MTLSize threadgroup_size = MTLSizeMake(16, 16, 1);
        MTLSize grid_size = MTLSizeMake(width, height, 1);

        [encoder dispatchThreads:grid_size threadsPerThreadgroup:threadgroup_size];
        [encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];

        const float* raw = static_cast<const float*>([output_buffer contents]);
        for (NSUInteger i = 0; i < pixel_count; ++i) {
            const float* px = raw + i * 4;
            pixels[i] = Color3(
                static_cast<double>(px[0]),
                static_cast<double>(px[1]),
                static_cast<double>(px[2])
            );
        }
    } // @autoreleasepool

    return pixels;
}

std::vector<Color3> MetalBufferManager::dispatch_ray_trace(
    const GPUCamera& camera,
    const FlatScene& scene,
    const std::vector<LinearBVHNode>& bvh_nodes)
{
    auto* mtl_device = (__bridge id<MTLDevice>)impl_->device.native_device();
    auto* command_queue = (__bridge id<MTLCommandQueue>)impl_->device.native_command_queue();
    auto* pipeline = (__bridge id<MTLComputePipelineState>)impl_->device.native_pipeline("ray_trace_kernel");

    if (!mtl_device || !command_queue || !pipeline) {
        return {};
    }

    const NSUInteger width = camera.image_width;
    const NSUInteger height = camera.image_height;
    const NSUInteger pixel_count = width * height;
    std::vector<Color3> pixels(pixel_count);

    // Reuse output buffer across frames (resolution is constant during animation)
    const NSUInteger output_buffer_size = pixel_count * sizeof(float) * 4;
    if (!impl_->cached_output_buffer || impl_->cached_output_pixel_count != pixel_count) {
        impl_->cached_output_buffer = [mtl_device newBufferWithLength:output_buffer_size
                                                              options:MTLResourceStorageModeShared];
        impl_->cached_output_pixel_count = pixel_count;
    }
    if (!impl_->cached_output_buffer) {
        return {};
    }

    // Reuse texture buffer across frames (texture data is static)
    NSUInteger tex_size = scene.texture_data.empty() ? 1 : scene.texture_data.size();
    if (!impl_->cached_texture_buffer || impl_->cached_texture_size != tex_size) {
        impl_->cached_texture_buffer = scene.texture_data.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:scene.texture_data.data()
                                      length:tex_size
                                     options:MTLResourceStorageModeShared];
        impl_->cached_texture_size = tex_size;
    }

    @autoreleasepool {
        // Per-frame buffers: shapes, materials, lights, BVH change each frame
        NSUInteger shapes_size = scene.shapes.empty()
            ? 1 : scene.shapes.size() * sizeof(GPUShape);
        NSUInteger materials_size = scene.materials.empty()
            ? 1 : scene.materials.size() * sizeof(GPUMaterial);
        NSUInteger lights_size = scene.lights.empty()
            ? 1 : scene.lights.size() * sizeof(GPULight);
        NSUInteger bvh_size = bvh_nodes.empty()
            ? 1 : bvh_nodes.size() * sizeof(LinearBVHNode);

        id<MTLBuffer> shapes_buffer = scene.shapes.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:scene.shapes.data()
                                      length:shapes_size
                                     options:MTLResourceStorageModeShared];

        id<MTLBuffer> materials_buffer = scene.materials.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:scene.materials.data()
                                      length:materials_size
                                     options:MTLResourceStorageModeShared];

        id<MTLBuffer> lights_buffer = scene.lights.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:scene.lights.data()
                                      length:lights_size
                                     options:MTLResourceStorageModeShared];

        id<MTLBuffer> bvh_buffer = bvh_nodes.empty()
            ? [mtl_device newBufferWithLength:1 options:MTLResourceStorageModeShared]
            : [mtl_device newBufferWithBytes:bvh_nodes.data()
                                      length:bvh_size
                                     options:MTLResourceStorageModeShared];

        // Scene counts: shape_count, material_count, light_count, bvh_node_count
        uint32_t scene_counts[4] = {
            static_cast<uint32_t>(scene.shapes.size()),
            static_cast<uint32_t>(scene.materials.size()),
            static_cast<uint32_t>(scene.lights.size()),
            static_cast<uint32_t>(bvh_nodes.size())
        };

        id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];

        [encoder setComputePipelineState:pipeline];
        [encoder setBytes:&camera length:sizeof(GPUCamera) atIndex:0];
        [encoder setBuffer:impl_->cached_output_buffer offset:0 atIndex:1];
        [encoder setBuffer:shapes_buffer offset:0 atIndex:2];
        [encoder setBuffer:materials_buffer offset:0 atIndex:3];
        [encoder setBuffer:lights_buffer offset:0 atIndex:4];
        [encoder setBytes:scene_counts length:sizeof(scene_counts) atIndex:5];
        [encoder setBuffer:bvh_buffer offset:0 atIndex:6];
        [encoder setBuffer:impl_->cached_texture_buffer offset:0 atIndex:7];

        MTLSize threadgroup_size = MTLSizeMake(16, 16, 1);
        MTLSize grid_size = MTLSizeMake(width, height, 1);

        [encoder dispatchThreads:grid_size threadsPerThreadgroup:threadgroup_size];
        [encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];

        // Read back float4 data and convert to Color3
        const float* raw = static_cast<const float*>([impl_->cached_output_buffer contents]);

        for (NSUInteger i = 0; i < pixel_count; ++i) {
            const float* px = raw + i * 4;
            pixels[i] = Color3(
                static_cast<double>(px[0]),
                static_cast<double>(px[1]),
                static_cast<double>(px[2])
            );
        }
    } // @autoreleasepool: per-frame Metal buffers released here

    return pixels;
}

} // namespace nwave
