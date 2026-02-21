#ifndef NWAVE_INFRASTRUCTURE_GPU_SCENE_FLATTENER_H
#define NWAVE_INFRASTRUCTURE_GPU_SCENE_FLATTENER_H

#include "core/gpu_types.h"
#include <unordered_map>
#include <vector>

namespace nwave {

class Material;
class Scene;

struct FlatScene {
    std::vector<GPUShape> shapes;
    std::vector<GPUMaterial> materials;
    std::vector<GPULight> lights;
    std::vector<uint8_t> texture_data;  // concatenated RGBA pixel data for all textures
};

class SceneFlattener {
public:
    FlatScene flatten(const Scene& scene);

private:
    std::unordered_map<const Material*, uint32_t> cached_material_map_;
    std::vector<GPUMaterial> cached_materials_;
    std::vector<uint8_t> cached_texture_data_;
    bool materials_cached_ = false;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_GPU_SCENE_FLATTENER_H
