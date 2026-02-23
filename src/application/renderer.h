#ifndef NWAVE_APPLICATION_RENDERER_H
#define NWAVE_APPLICATION_RENDERER_H

#include "core/vec3.h"
#include "core/ray.h"
#include "domain/camera.h"
#include "domain/scene.h"
#include <vector>

namespace nwave {

enum class RenderBackendType { CPU, METAL };

struct RenderSettings {
    int samples_per_pixel = 1;
    int max_depth = 10;
    Color3 background_top = Color3(0.5, 0.7, 1.0);
    Color3 background_bottom = Color3(1.0, 1.0, 1.0);
    int num_threads = 0; // 0 = auto-detect (hardware_concurrency)
    RenderBackendType backend = RenderBackendType::CPU;
    float ambient_factor = 0.05f;
};

class Renderer {
public:
    void set_quiet(bool quiet) { quiet_ = quiet; }

    std::vector<Color3> render(const Camera& camera, const Scene& scene,
                               const RenderSettings& settings) const;

    Color3 trace_ray(const Ray& ray, const Scene& scene,
                     const RenderSettings& settings, int depth) const;

private:
    void render_scanlines(int y_start, int y_end, int width,
                          const Camera& camera, const Scene& scene,
                          const RenderSettings& settings,
                          std::vector<Color3>& pixels) const;

    bool quiet_ = false;
};

} // namespace nwave

#endif // NWAVE_APPLICATION_RENDERER_H
