#ifndef NWAVE_INFRASTRUCTURE_METAL_METAL_RENDER_BACKEND_H
#define NWAVE_INFRASTRUCTURE_METAL_METAL_RENDER_BACKEND_H

#include "application/render_backend.h"
#include "core/vec3.h"
#include <memory>
#include <string>
#include <vector>

namespace nwave {

/// Metal render backend that dispatches GPU compute shaders.
/// Currently produces a gradient (camera/scene ignored until Phase 03).
/// Implements RenderBackend for polymorphic dispatch via the application layer.
class MetalRenderBackend : public RenderBackend {
public:
    MetalRenderBackend();
    ~MetalRenderBackend() override;

    MetalRenderBackend(const MetalRenderBackend&) = delete;
    MetalRenderBackend& operator=(const MetalRenderBackend&) = delete;

    /// Initialises the Metal device, loads the shader library, and
    /// creates the gradient_kernel pipeline.
    /// Returns true if all setup succeeded.
    bool initialise(const std::string& metallib_path);

    /// RenderBackend interface -- dispatches gradient kernel using
    /// camera dimensions. Scene/camera content ignored until Phase 03.
    std::vector<Color3> render(const Camera& camera, const Scene& scene,
                               const RenderSettings& settings) override;

    /// Dispatches the gradient kernel for the given dimensions.
    /// Returns an empty vector on failure.
    std::vector<Color3> render_gradient(int width, int height);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_METAL_METAL_RENDER_BACKEND_H
