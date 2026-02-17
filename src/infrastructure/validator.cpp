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

ValidationResult SceneValidator::validate(const std::string& yaml_content) {
    ValidationResult result;
    YAML::Node root = YAML::Load(yaml_content);

    // Collect defined material names
    std::set<std::string> defined_materials;
    const auto& materials_node = root["materials"];
    if (materials_node && materials_node.IsSequence()) {
        for (const auto& mat : materials_node) {
            if (mat["name"]) {
                defined_materials.insert(mat["name"].as<std::string>());
            }
        }
    }

    // Check camera
    if (!root["camera"] || root["camera"].IsNull()) {
        result.add_error("Scene requires a camera");
    }

    // Check objects
    const auto& objects_node = root["objects"];
    if (!objects_node || !objects_node.IsSequence() || objects_node.size() == 0) {
        result.add_error("Scene requires at least 1 object");
    } else {
        for (const auto& obj : objects_node) {
            std::string obj_name = obj["name"] ? obj["name"].as<std::string>() : "unnamed";
            std::string obj_type = obj["type"] ? obj["type"].as<std::string>() : "unknown";

            // Material reference check
            if (obj["material"]) {
                std::string mat_ref = obj["material"].as<std::string>();
                if (defined_materials.find(mat_ref) == defined_materials.end()) {
                    std::string suggestion;
                    int best_distance = 3; // threshold: suggest only within distance 2
                    for (const auto& defined : defined_materials) {
                        int dist = levenshtein_distance(mat_ref, defined);
                        if (dist <= 2 && dist < best_distance) {
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
            }

            // Parameter range checks
            if (obj["radius"]) {
                double radius = obj["radius"].as<double>();
                if (radius <= 0.0) {
                    std::ostringstream oss;
                    oss << "Object '" << obj_name << "' has non-positive radius: " << radius;
                    result.add_error(oss.str());
                }
            }

            if (obj["physics"]) {
                const auto& physics = obj["physics"];
                if (physics["mass"]) {
                    double mass = physics["mass"].as<double>();
                    if (mass < 0.0) {
                        std::ostringstream oss;
                        oss << "Object '" << obj_name << "' has negative mass: " << mass;
                        result.add_error(oss.str());
                    }
                }
            }
        }
    }

    // Check lights
    const auto& lights_node = root["lights"];
    if (!lights_node || !lights_node.IsSequence() || lights_node.size() == 0) {
        result.add_error("Scene requires at least 1 light");
    }

    return result;
}

} // namespace nwave
