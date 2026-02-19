#ifndef NWAVE_INFRASTRUCTURE_CLI_DISPATCHER_H
#define NWAVE_INFRASTRUCTURE_CLI_DISPATCHER_H

#include <functional>
#include <iosfwd>
#include <string>

namespace nwave {

struct RenderCommand {
    std::string scene_file;
    int width = 0;       // 0 = use scene default
    int spp = 0;         // 0 = use scene default
    std::string output = "output.ppm";
    bool physics_animate = false;
    int fps = 0;                // 0 = use YAML/default
    std::string output_dir;     // empty = use YAML/default
    std::string backend = "cpu"; // cpu, metal
};

struct ValidateCommand {
    std::string scene_file;
};

// Handler types for dependency injection (testability)
using RenderHandler = std::function<int(const RenderCommand&)>;
using ValidateHandler = std::function<int(const ValidateCommand&)>;
using LegacyHandler = std::function<int()>;

class CliDispatcher {
public:
    CliDispatcher(std::ostream& out, std::ostream& err);

    void set_render_handler(RenderHandler handler);
    void set_validate_handler(ValidateHandler handler);
    void set_legacy_handler(LegacyHandler handler);

    int dispatch(int argc, char* argv[]);

private:
    void print_usage();
    int handle_render(int argc, char* argv[]);
    int handle_validate(int argc, char* argv[]);

    std::ostream& out_;
    std::ostream& err_;
    RenderHandler render_handler_;
    ValidateHandler validate_handler_;
    LegacyHandler legacy_handler_;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_CLI_DISPATCHER_H
