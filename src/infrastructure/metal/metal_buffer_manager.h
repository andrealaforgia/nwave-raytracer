#ifndef NWAVE_INFRASTRUCTURE_METAL_METAL_BUFFER_MANAGER_H
#define NWAVE_INFRASTRUCTURE_METAL_METAL_BUFFER_MANAGER_H

#include "core/vec3.h"
#include "core/gpu_types.h"
#include "infrastructure/gpu/scene_flattener.h"

#include <memory>
#include <vector>

namespace nwave {

class MetalDevice;

/// Manages Metal buffer allocation, compute dispatch, and GPU readback.
/// Pure C++ header -- Objective-C types are hidden behind pimpl.
class MetalBufferManager {
public:
    explicit MetalBufferManager(MetalDevice& device);
    ~MetalBufferManager();

    MetalBufferManager(const MetalBufferManager&) = delete;
    MetalBufferManager& operator=(const MetalBufferManager&) = delete;

    /// Dispatches the gradient kernel and reads back pixel data.
    /// Requires MetalDevice to have loaded the metallib and created
    /// the "gradient_kernel" pipeline before calling this method.
    std::vector<Color3> dispatch_gradient(int width, int height);

    /// Dispatches the ray_trace_kernel with the given GPUCamera and
    /// reads back the resulting sky gradient pixel data.
    std::vector<Color3> dispatch_ray_trace(const GPUCamera& camera);

    /// Dispatches the ray_trace_kernel with camera and full scene data.
    /// Uploads GPUShape[], GPUMaterial[], GPULight[] arrays to Metal buffers.
    std::vector<Color3> dispatch_ray_trace(const GPUCamera& camera,
                                           const FlatScene& scene);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_METAL_METAL_BUFFER_MANAGER_H
