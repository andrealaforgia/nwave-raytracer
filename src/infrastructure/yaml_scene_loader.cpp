#include "infrastructure/yaml_scene_loader.h"
#include "domain/materials/lambertian.h"
#include "domain/materials/metal.h"
#include "domain/materials/dielectric.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

namespace nwave {

namespace {

Color3 parse_color3(const YAML::Node& node) {
    return Color3(node[0].as<double>(), node[1].as<double>(), node[2].as<double>());
}

std::shared_ptr<Material> create_lambertian(const YAML::Node& node) {
    return std::make_shared<Lambertian>(parse_color3(node["albedo"]));
}

std::shared_ptr<Material> create_metal(const YAML::Node& node) {
    auto albedo = parse_color3(node["albedo"]);
    double fuzz = node["fuzz"] ? node["fuzz"].as<double>() : 0.0;
    return std::make_shared<Metal>(albedo, fuzz);
}

std::shared_ptr<Material> create_dielectric(const YAML::Node& node) {
    double ior = node["ior"].as<double>();
    Color3 tint(1.0, 1.0, 1.0);
    if (node["tint"]) {
        tint = parse_color3(node["tint"]);
    }
    return std::make_shared<Dielectric>(ior, tint);
}

} // anonymous namespace

std::map<std::string, std::shared_ptr<Material>> YamlSceneLoader::parse_materials(const std::string& yaml_content) {
    std::map<std::string, std::shared_ptr<Material>> materials;

    YAML::Node root = YAML::Load(yaml_content);
    const auto& materials_node = root["materials"];
    if (!materials_node || !materials_node.IsSequence()) {
        return materials;
    }

    for (const auto& mat_node : materials_node) {
        std::string name = mat_node["name"].as<std::string>();
        std::string type = mat_node["type"].as<std::string>();

        std::shared_ptr<Material> material;
        if (type == "lambertian") {
            material = create_lambertian(mat_node);
        } else if (type == "metal") {
            material = create_metal(mat_node);
        } else if (type == "dielectric") {
            material = create_dielectric(mat_node);
        } else {
            throw std::runtime_error("Unknown material type: " + type);
        }

        materials[name] = material;
    }

    return materials;
}

} // namespace nwave
