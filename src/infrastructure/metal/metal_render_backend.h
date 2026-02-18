#ifndef NWAVE_INFRASTRUCTURE_METAL_METAL_RENDER_BACKEND_H
#define NWAVE_INFRASTRUCTURE_METAL_METAL_RENDER_BACKEND_H

#include "core/vec3.h"
#include <memory>
#include <string>
#include <vector>

namespace nwave {

/// Stub Metal render backend that dispatches the gradient shader
/// via MetalDevice + MetalBufferManager and returns pixel data.
/// This does NOT perform ray tracing -- it validates the end-to-end
/// Metal pipeline producing valid PPM output.
class MetalRenderBackend {
public:
    MetalRenderBackend();
    ~MetalRenderBackend();

    MetalRenderBackend(const MetalRenderBackend&) = delete;
    MetalRenderBackend& operator=(const MetalRenderBackend&) = delete;

    /// Initialises the Metal device, loads the shader library, and
    /// creates the gradient_kernel pipeline.
    /// Returns true if all setup succeeded.
    bool initialise(const std::string& metallib_path);

    /// Dispatches the gradient kernel for the given dimensions.
    /// Returns an empty vector on failure.
    std::vector<Color3> render_gradient(int width, int height);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_METAL_METAL_RENDER_BACKEND_H
