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

namespace {

BodyType parse_body_type(const std::string& type_str) {
    if (type_str == "dynamic") return BodyType::DYNAMIC;
    if (type_str == "kinematic") return BodyType::KINEMATIC;
    if (type_str == "soft") return BodyType::SOFT;
    return BodyType::STATIC;
}

PhysicsProperties parse_physics(const YAML::Node& physics_node) {
    PhysicsProperties props;
    if (!physics_node || !physics_node.IsMap()) {
        return props;
    }
    if (physics_node["body_type"]) {
        props.body_type = parse_body_type(physics_node["body_type"].as<std::string>());
    }
    if (physics_node["mass"]) {
        props.mass = physics_node["mass"].as<double>();
    }
    if (physics_node["initial_velocity"]) {
        props.initial_velocity = parse_vec3(physics_node["initial_velocity"]);
    }
    if (physics_node["friction"]) {
        props.friction = physics_node["friction"].as<double>();
    }
    if (physics_node["restitution"]) {
        props.restitution = physics_node["restitution"].as<double>();
    }
    if (physics_node["start_asleep"]) {
        props.start_asleep = physics_node["start_asleep"].as<bool>();
    }
    return props;
}

std::optional<AnimationConfig> parse_animation_config(const YAML::Node& anim_node) {
    if (!anim_node || !anim_node.IsMap()) {
        return std::nullopt;
    }
    AnimationConfig config;
    config.duration = anim_node["duration"].as<double>();
    config.physics_timestep = anim_node["physics_timestep"].as<double>();
    config.render_fps = anim_node["render_fps"].as<double>();
    if (anim_node["output_directory"]) {
        config.output_directory = anim_node["output_directory"].as<std::string>();
    }
    if (anim_node["wake_frame"]) {
        config.wake_frame = anim_node["wake_frame"].as<int>();
    }
    return config;
}

std::shared_ptr<Shape> create_shape(const std::string& type,
                                    const YAML::Node& obj_node,
                                    const Material* mat_ptr) {
    if (type == "sphere") {
        Point3 center = parse_vec3(obj_node["center"]);
        double radius = obj_node["radius"].as<double>();
        return std::make_shared<Sphere>(center, radius, mat_ptr);
    }
    if (type == "plane") {
        Point3 point = parse_vec3(obj_node["point"]);
        Vec3 normal = parse_vec3(obj_node["normal"]);
        return std::make_shared<Plane>(point, normal, mat_ptr);
    }
    if (type == "box") {
        Point3 min_corner = parse_vec3(obj_node["min"]);
        Point3 max_corner = parse_vec3(obj_node["max"]);
        return std::make_shared<Box>(min_corner, max_corner, mat_ptr);
    }
    if (type == "cylinder") {
        Point3 center = parse_vec3(obj_node["center"]);
        double radius = obj_node["radius"].as<double>();
        double height = obj_node["height"].as<double>();
        return std::make_shared<Cylinder>(center, radius, height, mat_ptr);
    }
    if (type == "triangle") {
        Point3 v0 = parse_vec3(obj_node["v0"]);
        Point3 v1 = parse_vec3(obj_node["v1"]);
        Point3 v2 = parse_vec3(obj_node["v2"]);
        return std::make_shared<Triangle>(v0, v1, v2, mat_ptr);
    }
    if (type == "triangle_mesh") {
        std::vector<Point3> vertices;
        for (const auto& v : obj_node["vertices"]) {
            vertices.push_back(parse_vec3(v));
        }

        std::vector<Vec3> normals;
        for (const auto& n : obj_node["normals"]) {
            normals.push_back(parse_vec3(n));
        }

        std::vector<int> vertex_indices;
        for (const auto& face : obj_node["indices"]) {
            vertex_indices.push_back(face[0].as<int>());
            vertex_indices.push_back(face[1].as<int>());
            vertex_indices.push_back(face[2].as<int>());
        }

        std::vector<int> normal_indices = vertex_indices;

        return std::make_shared<TriangleMesh>(
            std::move(vertices), std::move(normals),
            std::move(vertex_indices), std::move(normal_indices), mat_ptr);
    }
    throw std::runtime_error(
        "Unknown object type: " + type +
        ". Supported types: sphere, plane, box, cylinder, triangle, triangle_mesh");
}

SoftBodyDesc parse_soft_body_desc(const YAML::Node& obj_node, const Material* material) {
    SoftBodyDesc desc;
    desc.material = material;
    if (obj_node["position"]) desc.position = parse_vec3(obj_node["position"]);
    if (obj_node["grid_resolution"]) desc.grid_resolution = obj_node["grid_resolution"].as<int>();
    if (obj_node["grid_spacing"]) desc.grid_spacing = obj_node["grid_spacing"].as<double>();
    if (obj_node["pressure"]) desc.pressure = obj_node["pressure"].as<double>();
    if (obj_node["restitution"]) desc.restitution = obj_node["restitution"].as<double>();
    if (obj_node["friction"]) desc.friction = obj_node["friction"].as<double>();
    if (obj_node["damping"]) desc.damping = obj_node["damping"].as<double>();
    if (obj_node["solver_iterations"]) desc.solver_iterations = obj_node["solver_iterations"].as<int>();
    if (obj_node["edge_compliance"]) desc.edge_compliance = obj_node["edge_compliance"].as<double>();
    if (obj_node["volume_compliance"]) desc.volume_compliance = obj_node["volume_compliance"].as<double>();
    return desc;
}

void parse_objects(const YAML::Node& objects_node,
                   const std::map<std::string, std::shared_ptr<Material>>& materials,
                   Scene& scene,
                   std::vector<PhysicsProperties>& shape_physics,
                   std::vector<SoftBodyDesc>& soft_body_descs) {
    if (!objects_node || !objects_node.IsSequence()) {
        return;
    }
    for (const auto& obj_node : objects_node) {
        std::string type = obj_node["type"].as<std::string>();
        std::string mat_ref = obj_node["material"].as<std::string>();

        auto it = materials.find(mat_ref);
        if (it == materials.end()) {
            throw std::runtime_error("Unknown material reference: " + mat_ref);
        }

        if (type == "soft_body_cube") {
            soft_body_descs.push_back(parse_soft_body_desc(obj_node, it->second.get()));
            continue;
        }

        scene.add_shape(create_shape(type, obj_node, it->second.get()));
        shape_physics.push_back(parse_physics(obj_node["physics"]));
    }
}

void parse_lights(const YAML::Node& lights_node, Scene& scene) {
    if (!lights_node || !lights_node.IsSequence()) {
        return;
    }
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

constexpr double default_aspect_ratio = 16.0 / 9.0;

Camera parse_camera(const YAML::Node& cam_node) {
    Point3 lookfrom = parse_vec3(cam_node["lookfrom"]);
    Point3 lookat = parse_vec3(cam_node["lookat"]);
    Vec3 vup = parse_vec3(cam_node["vup"]);
    double vfov = cam_node["vfov"].as<double>();
    int image_width = cam_node["image_width"].as<int>();
    double aspect_ratio = cam_node["aspect_ratio"]
        ? cam_node["aspect_ratio"].as<double>()
        : default_aspect_ratio;

    return Camera(lookfrom, lookat, vup, vfov, aspect_ratio, image_width);
}

} // anonymous namespace

SceneLoadResult YamlSceneLoader::load(const std::string& yaml_content) {
    auto materials = parse_materials(yaml_content);

    std::vector<std::shared_ptr<Material>> materials_storage;
    for (const auto& [name, mat] : materials) {
        materials_storage.push_back(mat);
    }

    YAML::Node root = YAML::Load(yaml_content);
    Scene scene;
    std::vector<PhysicsProperties> shape_physics;
    std::vector<SoftBodyDesc> soft_body_descs;

    parse_objects(root["objects"], materials, scene, shape_physics, soft_body_descs);
    parse_lights(root["lights"], scene);
    Camera camera = parse_camera(root["camera"]);
    auto animation_config = parse_animation_config(root["animation"]);

    return SceneLoadResult(std::move(scene), std::move(camera), std::move(materials_storage),
                           std::move(shape_physics), std::move(animation_config),
                           std::move(soft_body_descs));
}

} // namespace nwave
