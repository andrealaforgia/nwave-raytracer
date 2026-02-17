#include "infrastructure/cli_dispatcher.h"
#include <cstring>
#include <ostream>

namespace nwave {

CliDispatcher::CliDispatcher(std::ostream& out, std::ostream& err)
    : out_(out), err_(err) {}

void CliDispatcher::set_render_handler(RenderHandler handler) {
    render_handler_ = std::move(handler);
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
        }
    }

    if (render_handler_) {
        return render_handler_(cmd);
    }

    err_ << "Error: no render handler configured\n";
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

    // Unknown subcommand
    err_ << "Unknown command: " << arg << "\n";
    print_usage();
    return 1;
}

} // namespace nwave
