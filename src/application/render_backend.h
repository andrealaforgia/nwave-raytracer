#ifndef NWAVE_APPLICATION_RENDER_BACKEND_H
#define NWAVE_APPLICATION_RENDER_BACKEND_H

#include "application/renderer.h"
#include "core/vec3.h"
#include "domain/camera.h"
#include "domain/scene.h"
#include <vector>

namespace nwave {

class RenderBackend {
public:
    virtual ~RenderBackend() = default;
    virtual std::vector<Color3> render(const Camera& camera, const Scene& scene,
                                       const RenderSettings& settings) = 0;
};

} // namespace nwave

#endif // NWAVE_APPLICATION_RENDER_BACKEND_H
