#ifndef NWAVE_INFRASTRUCTURE_GPU_SCENE_FLATTENER_H
#define NWAVE_INFRASTRUCTURE_GPU_SCENE_FLATTENER_H

#include "core/gpu_types.h"
#include <vector>

namespace nwave {

class Scene;

struct FlatScene {
    std::vector<GPUShape> shapes;
    std::vector<GPUMaterial> materials;
};

class SceneFlattener {
public:
    FlatScene flatten(const Scene& scene) const;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_GPU_SCENE_FLATTENER_H
