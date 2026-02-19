#ifndef NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H
#define NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H

#include "domain/materials/material.h"
#include "domain/scene.h"
#include "domain/camera.h"
#include "domain/physics_properties.h"
#include "domain/animation_config.h"
#include "domain/soft_body_desc.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nwave {

struct SceneLoadResult {
    Scene scene;
    Camera camera;
    std::vector<std::shared_ptr<Material>> materials_storage;
    std::vector<PhysicsProperties> shape_physics;
    std::optional<AnimationConfig> animation_config;
    std::vector<SoftBodyDesc> soft_body_descs;

    SceneLoadResult(Scene s, Camera c, std::vector<std::shared_ptr<Material>> mats,
                    std::vector<PhysicsProperties> physics = {},
                    std::optional<AnimationConfig> anim = std::nullopt,
                    std::vector<SoftBodyDesc> soft_bodies = {})
        : scene(std::move(s)), camera(std::move(c)), materials_storage(std::move(mats)),
          shape_physics(std::move(physics)), animation_config(std::move(anim)),
          soft_body_descs(std::move(soft_bodies)) {}
};

class YamlSceneLoader {
public:
    std::map<std::string, std::shared_ptr<Material>> parse_materials(const std::string& yaml_content);
    SceneLoadResult load(const std::string& yaml_content);
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H
