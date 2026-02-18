#include "application/cpu_render_backend.h"

namespace nwave {

void CpuRenderBackend::set_quiet(bool quiet) {
    renderer_.set_quiet(quiet);
}

std::vector<Color3> CpuRenderBackend::render(const Camera& camera, const Scene& scene,
                                              const RenderSettings& settings) {
    return renderer_.render(camera, scene, settings);
}

} // namespace nwave
