#include "infrastructure/cli_dispatcher.h"
#include <cstring>
#include <ostream>

namespace nwave {

CliDispatcher::CliDispatcher(std::ostream& out, std::ostream& err)
    : out_(out), err_(err) {}

void CliDispatcher::set_render_handler(RenderHandler handler) {
    render_handler_ = std::move(handler);
}

void CliDispatcher::set_validate_handler(ValidateHandler handler) {
    validate_handler_ = std::move(handler);
}

void CliDispatcher::set_legacy_handler(LegacyHandler handler) {
    legacy_handler_ = std::move(handler);
}

void CliDispatcher::print_usage() {
    out_ << "Usage: nwave <command> [options]\n"
         << "\n"
         << "Commands:\n"
         << "  render <file.yaml>   Load a YAML scene and render to PPM\n"
         << "    --width <N>        Override image width\n"
         << "    --spp <N>          Override samples per pixel\n"
         << "    -o <file>          Output filename (default: output.ppm)\n"
         << "    --fps <N>          Override frames per second\n"
         << "    --output-dir <dir> Override output directory\n"
         << "    --backend <name>   Render backend: cpu, metal (default: cpu)\n"
         << "    --physics-animate  Run physics-driven animation\n"
         << "  validate <file.yaml> Validate a YAML scene (no render)\n"
         << "\n"
         << "Flags:\n"
         << "  --animate            Run legacy animation (360-degree orbit)\n"
         << "  --help               Show this help message\n";
}

int CliDispatcher::handle_render(int argc, char* argv[]) {
    // argv[0] = "nwave", argv[1] = "render"
    // We need at least argv[2] = filename
    if (argc < 3) {
        err_ << "Error: render command requires a scene file\n";
        return 1;
    }

    RenderCommand cmd;
    cmd.scene_file = argv[2];

    // Parse optional flags starting at argv[3]
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            cmd.width = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--spp") == 0 && i + 1 < argc) {
            cmd.spp = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            cmd.output = argv[++i];
        } else if (std::strcmp(argv[i], "--physics-animate") == 0) {
            cmd.physics_animate = true;
        } else if (std::strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            cmd.fps = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            cmd.output_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            cmd.backend = argv[++i];
        }
    }

    // Validate backend value
    if (cmd.backend != "cpu" && cmd.backend != "metal") {
        err_ << "Unknown backend: " << cmd.backend
             << ". Available backends: cpu, metal\n";
        return 1;
    }

    if (render_handler_) {
        return render_handler_(cmd);
    }

    err_ << "Error: no render handler configured\n";
    return 1;
}

int CliDispatcher::handle_validate(int argc, char* argv[]) {
    if (argc < 3) {
        err_ << "Error: validate command requires a scene file\n";
        return 1;
    }

    ValidateCommand cmd;
    cmd.scene_file = argv[2];

    if (validate_handler_) {
        return validate_handler_(cmd);
    }

    err_ << "Error: no validate handler configured\n";
    return 1;
}

int CliDispatcher::dispatch(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char* arg = argv[1];

    if (std::strcmp(arg, "--help") == 0) {
        print_usage();
        return 0;
    }

    if (std::strcmp(arg, "--animate") == 0) {
        if (legacy_handler_) {
            return legacy_handler_();
        }
        // If no legacy handler set, still recognize the flag
        return 0;
    }

    if (std::strcmp(arg, "render") == 0) {
        return handle_render(argc, argv);
    }

    if (std::strcmp(arg, "validate") == 0) {
        return handle_validate(argc, argv);
    }

    // Unknown subcommand
    err_ << "Unknown command: " << arg << "\n";
    print_usage();
    return 1;
}

} // namespace nwave
