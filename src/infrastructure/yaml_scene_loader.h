#ifndef NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H
#define NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H

#include "domain/materials/material.h"
#include "domain/scene.h"
#include "domain/camera.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nwave {

struct SceneLoadResult {
    Scene scene;
    Camera camera;
    std::vector<std::shared_ptr<Material>> materials_storage;

    SceneLoadResult(Scene s, Camera c, std::vector<std::shared_ptr<Material>> mats)
        : scene(std::move(s)), camera(std::move(c)), materials_storage(std::move(mats)) {}
};

class YamlSceneLoader {
public:
    std::map<std::string, std::shared_ptr<Material>> parse_materials(const std::string& yaml_content);
    SceneLoadResult load(const std::string& yaml_content);
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H
