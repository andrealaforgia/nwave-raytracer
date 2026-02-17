#ifndef NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H
#define NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H

#include "domain/materials/material.h"
#include <map>
#include <memory>
#include <string>

namespace nwave {

class YamlSceneLoader {
public:
    std::map<std::string, std::shared_ptr<Material>> parse_materials(const std::string& yaml_content);
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_YAML_SCENE_LOADER_H
