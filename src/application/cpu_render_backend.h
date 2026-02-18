#ifndef NWAVE_APPLICATION_CPU_RENDER_BACKEND_H
#define NWAVE_APPLICATION_CPU_RENDER_BACKEND_H

#include "application/render_backend.h"
#include "application/renderer.h"

namespace nwave {

class CpuRenderBackend : public RenderBackend {
public:
    void set_quiet(bool quiet);

    std::vector<Color3> render(const Camera& camera, const Scene& scene,
                               const RenderSettings& settings) override;

private:
    Renderer renderer_;
};

} // namespace nwave

#endif // NWAVE_APPLICATION_CPU_RENDER_BACKEND_H
