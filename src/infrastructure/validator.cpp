#include "infrastructure/validator.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace nwave {

int SceneValidator::levenshtein_distance(const std::string& a, const std::string& b) {
    const auto m = a.size();
    const auto n = b.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; ++j) dp[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i - 1][j] + 1,
                                 dp[i][j - 1] + 1,
                                 dp[i - 1][j - 1] + cost});
        }
    }
    return dp[m][n];
}

namespace {

std::set<std::string> collect_material_names(const YAML::Node& materials_node) {
    std::set<std::string> names;
    if (materials_node && materials_node.IsSequence()) {
        for (const auto& mat : materials_node) {
            if (mat["name"]) {
                names.insert(mat["name"].as<std::string>());
            }
        }
    }
    return names;
}

void validate_material_reference(const std::string& obj_name,
                                 const std::string& mat_ref,
                                 const std::set<std::string>& defined_materials,
                                 ValidationResult& result) {
    if (defined_materials.find(mat_ref) != defined_materials.end()) {
        return;
    }

    static constexpr int max_suggestion_distance = 2;
    std::string suggestion;
    int best_distance = max_suggestion_distance + 1;
    for (const auto& defined : defined_materials) {
        int dist = SceneValidator::levenshtein_distance(mat_ref, defined);
        if (dist <= max_suggestion_distance && dist < best_distance) {
            best_distance = dist;
            suggestion = defined;
        }
    }

    std::ostringstream oss;
    oss << "Object '" << obj_name << "' references unknown material '" << mat_ref << "'";
    if (!suggestion.empty()) {
        oss << " (did you mean '" << suggestion << "'?)";
    }
    result.add_error(oss.str());
}

void validate_physics_properties(const std::string& obj_name,
                                 const std::string& obj_type,
                                 const YAML::Node& physics,
                                 ValidationResult& result) {
    if (physics["mass"]) {
        double mass = physics["mass"].as<double>();
        if (mass < 0.0) {
            std::ostringstream oss;
            oss << "Object '" << obj_name << "' has negative mass: " << mass;
            result.add_error(oss.str());
        }
    }
    if (physics["friction"]) {
        double friction = physics["friction"].as<double>();
        if (friction < 0.0 || friction > 1.0) {
            std::ostringstream oss;
            oss << "Object '" << obj_name << "' has friction out of [0, 1]: " << friction;
            result.add_error(oss.str());
        }
    }
    if (physics["restitution"]) {
        double restitution = physics["restitution"].as<double>();
        if (restitution < 0.0 || restitution > 1.0) {
            std::ostringstream oss;
            oss << "Object '" << obj_name << "' has restitution out of [0, 1]: " << restitution;
            result.add_error(oss.str());
        }
    }
    std::string body_type_str = physics["body_type"]
        ? physics["body_type"].as<std::string>() : "static";
    if (obj_type == "triangle_mesh" && body_type_str == "dynamic") {
        std::ostringstream oss;
        oss << "Object '" << obj_name
            << "': concave triangle_mesh must be static, not dynamic";
        result.add_error(oss.str());
    }
}

void validate_object(const YAML::Node& obj,
                     const std::set<std::string>& defined_materials,
                     ValidationResult& result) {
    std::string obj_name = obj["name"] ? obj["name"].as<std::string>() : "unnamed";
    std::string obj_type = obj["type"] ? obj["type"].as<std::string>() : "unknown";

    if (obj["material"]) {
        validate_material_reference(obj_name, obj["material"].as<std::string>(),
                                    defined_materials, result);
    }

    if (obj["radius"]) {
        double radius = obj["radius"].as<double>();
        if (radius <= 0.0) {
            std::ostringstream oss;
            oss << "Object '" << obj_name << "' has non-positive radius: " << radius;
            result.add_error(oss.str());
        }
    }

    if (obj["physics"]) {
        validate_physics_properties(obj_name, obj_type, obj["physics"], result);
    }
}

bool is_node_present(const YAML::Node& node) {
    return node && !node.IsNull();
}

bool is_non_empty_sequence(const YAML::Node& node) {
    return node && node.IsSequence() && node.size() > 0;
}

} // anonymous namespace

ValidationResult SceneValidator::validate(const std::string& yaml_content,
                                          bool physics_animate) {
    ValidationResult result;
    YAML::Node root = YAML::Load(yaml_content);

    auto defined_materials = collect_material_names(root["materials"]);

    if (!is_node_present(root["camera"])) {
        result.add_error("Scene requires a camera");
    }

    const auto& objects_node = root["objects"];
    if (!is_non_empty_sequence(objects_node)) {
        result.add_error("Scene requires at least 1 object");
    } else {
        for (const auto& obj : objects_node) {
            validate_object(obj, defined_materials, result);
        }
    }

    if (!is_non_empty_sequence(root["lights"])) {
        result.add_error("Scene requires at least 1 light");
    }

    if (physics_animate && !is_node_present(root["animation"])) {
        result.add_error(
            "Missing 'animation' section required for physics-animate mode");
    }

    return result;
}

} // namespace nwave
