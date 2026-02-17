#ifndef NWAVE_INFRASTRUCTURE_VALIDATOR_H
#define NWAVE_INFRASTRUCTURE_VALIDATOR_H

#include <string>
#include <vector>

namespace nwave {

struct ValidationError {
    std::string message;
};

struct ValidationResult {
    bool is_valid{true};
    std::vector<ValidationError> errors;

    void add_error(const std::string& msg) {
        errors.push_back({msg});
        is_valid = false;
    }
};

class SceneValidator {
public:
    ValidationResult validate(const std::string& yaml_content,
                              bool physics_animate = false);

    static int levenshtein_distance(const std::string& a, const std::string& b);
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_VALIDATOR_H
