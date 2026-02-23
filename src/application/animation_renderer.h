#ifndef NWAVE_APPLICATION_ANIMATION_RENDERER_H
#define NWAVE_APPLICATION_ANIMATION_RENDERER_H

#include "application/physics_simulator.h"
#include "application/progress_reporter.h"
#include "application/renderer.h"
#include "domain/animation_config.h"
#include "domain/scene.h"
#include "domain/camera.h"
#include "domain/physics_properties.h"
#include "domain/soft_body_desc.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nwave {

using WriteCallback = std::function<void(const std::string& filename,
                                         const Scene& scene,
                                         const Camera& camera,
                                         int width,
                                         const RenderSettings& settings)>;

class AnimationRenderer {
public:
    AnimationRenderer(const AnimationConfig& config,
                      const Scene& scene,
                      const std::vector<PhysicsProperties>& shape_physics,
                      std::unique_ptr<PhysicsSimulator> physics,
                      const Camera& camera,
                      WriteCallback write_callback,
                      ProgressReporter* progress = nullptr,
                      std::vector<SoftBodyDesc> soft_body_descs = {});

    int render();

private:
    AnimationConfig config_;
    Scene scene_;
    std::vector<PhysicsProperties> shape_physics_;
    std::unique_ptr<PhysicsSimulator> physics_;
    Camera camera_;
    WriteCallback write_callback_;
    ProgressReporter* progress_;
    std::vector<SoftBodyDesc> soft_body_descs_;
};

} // namespace nwave

#endif // NWAVE_APPLICATION_ANIMATION_RENDERER_H
