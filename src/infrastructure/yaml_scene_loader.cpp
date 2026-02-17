#include "infrastructure/yaml_scene_loader.h"
#include "domain/materials/lambertian.h"
#include "domain/materials/metal.h"
#include "domain/materials/dielectric.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/box.h"
#include "domain/shapes/cylinder.h"
#include "domain/shapes/triangle.h"
#include "domain/shapes/triangle_mesh.h"
#include "domain/lights/point_light.h"
#include "domain/lights/directional_light.h"
#include "domain/materials/emissive.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

namespace nwave {

namespace {

Vec3 parse_vec3(const YAML::Node& node) {
    return Vec3(node[0].as<double>(), node[1].as<double>(), node[2].as<double>());
}

std::shared_ptr<Material> create_lambertian(const YAML::Node& node) {
    return std::make_shared<Lambertian>(parse_vec3(node["albedo"]));
}

std::shared_ptr<Material> create_metal(const YAML::Node& node) {
    auto albedo = parse_vec3(node["albedo"]);
    double fuzz = node["fuzz"] ? node["fuzz"].as<double>() : 0.0;
    return std::make_shared<Metal>(albedo, fuzz);
}

std::shared_ptr<Material> create_dielectric(const YAML::Node& node) {
    double ior = node["ior"].as<double>();
    Color3 tint(1.0, 1.0, 1.0);
    if (node["tint"]) {
        tint = parse_vec3(node["tint"]);
    }
    return std::make_shared<Dielectric>(ior, tint);
}

std::shared_ptr<Material> create_emissive(const YAML::Node& node) {
    Color3 color = parse_vec3(node["color"]);
    double intensity = node["intensity"] ? node["intensity"].as<double>() : 1.0;
    return std::make_shared<Emissive>(color, intensity);
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
        } else if (type == "emissive") {
            material = create_emissive(mat_node);
        } else {
            throw std::runtime_error("Unknown material type: " + type);
        }

        materials[name] = material;
    }

    return materials;
}

SceneLoadResult YamlSceneLoader::load(const std::string& yaml_content) {
    auto materials = parse_materials(yaml_content);

    std::vector<std::shared_ptr<Material>> materials_storage;
    for (const auto& [name, mat] : materials) {
        materials_storage.push_back(mat);
    }

    YAML::Node root = YAML::Load(yaml_content);
    Scene scene;

    // Parse objects
    const auto& objects_node = root["objects"];
    if (objects_node && objects_node.IsSequence()) {
        for (const auto& obj_node : objects_node) {
            std::string type = obj_node["type"].as<std::string>();
            std::string mat_ref = obj_node["material"].as<std::string>();

            auto it = materials.find(mat_ref);
            if (it == materials.end()) {
                throw std::runtime_error("Unknown material reference: " + mat_ref);
            }
            const Material* mat_ptr = it->second.get();

            if (type == "sphere") {
                Point3 center = parse_vec3(obj_node["center"]);
                double radius = obj_node["radius"].as<double>();
                scene.add_shape(std::make_shared<Sphere>(center, radius, mat_ptr));
            } else if (type == "plane") {
                Point3 point = parse_vec3(obj_node["point"]);
                Vec3 normal = parse_vec3(obj_node["normal"]);
                scene.add_shape(std::make_shared<Plane>(point, normal, mat_ptr));
            } else if (type == "box") {
                Point3 min_corner = parse_vec3(obj_node["min"]);
                Point3 max_corner = parse_vec3(obj_node["max"]);
                scene.add_shape(std::make_shared<Box>(min_corner, max_corner, mat_ptr));
            } else if (type == "cylinder") {
                Point3 center = parse_vec3(obj_node["center"]);
                double radius = obj_node["radius"].as<double>();
                double height = obj_node["height"].as<double>();
                scene.add_shape(std::make_shared<Cylinder>(center, radius, height, mat_ptr));
            } else if (type == "triangle") {
                Point3 v0 = parse_vec3(obj_node["v0"]);
                Point3 v1 = parse_vec3(obj_node["v1"]);
                Point3 v2 = parse_vec3(obj_node["v2"]);
                scene.add_shape(std::make_shared<Triangle>(v0, v1, v2, mat_ptr));
            } else if (type == "triangle_mesh") {
                const auto& verts_node = obj_node["vertices"];
                std::vector<Point3> vertices;
                for (const auto& v : verts_node) {
                    vertices.push_back(parse_vec3(v));
                }

                const auto& normals_node = obj_node["normals"];
                std::vector<Vec3> normals;
                for (const auto& n : normals_node) {
                    normals.push_back(parse_vec3(n));
                }

                const auto& indices_node = obj_node["indices"];
                std::vector<int> vertex_indices;
                for (const auto& face : indices_node) {
                    vertex_indices.push_back(face[0].as<int>());
                    vertex_indices.push_back(face[1].as<int>());
                    vertex_indices.push_back(face[2].as<int>());
                }

                // Use same indices for normals (per-vertex normals)
                std::vector<int> normal_indices = vertex_indices;

                scene.add_shape(std::make_shared<TriangleMesh>(
                    std::move(vertices), std::move(normals),
                    std::move(vertex_indices), std::move(normal_indices), mat_ptr));
            } else {
                throw std::runtime_error(
                    "Unknown object type: " + type +
                    ". Supported types: sphere, plane, box, cylinder, triangle, triangle_mesh");
            }
        }
    }

    // Parse lights
    const auto& lights_node = root["lights"];
    if (lights_node && lights_node.IsSequence()) {
        for (const auto& light_node : lights_node) {
            std::string type = light_node["type"].as<std::string>();

            if (type == "point") {
                Point3 position = parse_vec3(light_node["position"]);
                Color3 color = parse_vec3(light_node["color"]);
                double intensity = light_node["intensity"].as<double>();
                scene.add_light(std::make_shared<PointLight>(position, color, intensity));
            } else if (type == "directional") {
                Vec3 direction = parse_vec3(light_node["direction"]);
                Color3 color = parse_vec3(light_node["color"]);
                double intensity = light_node["intensity"].as<double>();
                scene.add_light(std::make_shared<DirectionalLight>(direction, color, intensity));
            } else {
                throw std::runtime_error("Unknown light type: " + type);
            }
        }
    }

    // Parse camera
    const auto& cam_node = root["camera"];
    Point3 lookfrom = parse_vec3(cam_node["lookfrom"]);
    Point3 lookat = parse_vec3(cam_node["lookat"]);
    Vec3 vup = parse_vec3(cam_node["vup"]);
    double vfov = cam_node["vfov"].as<double>();
    int image_width = cam_node["image_width"].as<int>();
    double aspect_ratio = cam_node["aspect_ratio"] ? cam_node["aspect_ratio"].as<double>() : 16.0 / 9.0;

    Camera camera(lookfrom, lookat, vup, vfov, aspect_ratio, image_width);

    return SceneLoadResult(std::move(scene), std::move(camera), std::move(materials_storage));
}

} // namespace nwave
