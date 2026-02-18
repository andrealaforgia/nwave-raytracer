#include "domain/camera.h"
#include "domain/scene.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/box.h"
#include "domain/shapes/cylinder.h"
#include "domain/shapes/triangle.h"
#include "domain/materials/lambertian.h"
#include "domain/materials/metal.h"
#include "domain/materials/dielectric.h"
#include "domain/materials/emissive.h"
#include "domain/lights/point_light.h"
#include "domain/lights/directional_light.h"
#include "application/renderer.h"
#include "application/animation_renderer.h"
#include "application/cpu_render_backend.h"
#include "infrastructure/ppm_writer.h"
#include "infrastructure/yaml_scene_loader.h"
#include "infrastructure/jolt_physics_simulator.h"
#include "infrastructure/cli_dispatcher.h"
#include "infrastructure/validator.h"
#include "infrastructure/progress_reporter.h"
#ifdef NWAVE_HAS_METAL
#include "infrastructure/metal/metal_render_backend.h"
#endif
#include "core/math_utils.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <filesystem>

using namespace nwave;

// 5x7 bitmap font definitions ('#' = filled, '.' = empty)
static const std::vector<std::string> LETTER_n = {
    ".....", ".....", ".##..", "#..#.", "#..#.", "#..#.", "#..#."
};
static const std::vector<std::string> LETTER_W = {
    "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"
};
static const std::vector<std::string> LETTER_a = {
    ".....", ".....", ".###.", "...#.", ".###.", "#..#.", ".##.#"
};
static const std::vector<std::string> LETTER_v = {
    ".....", ".....", "#...#", "#...#", ".#.#.", ".#.#.", "..#.."
};
static const std::vector<std::string> LETTER_e = {
    ".....", ".....", ".##..", "#..#.", "####.", "#....", ".###."
};

static void add_letter(Scene& scene,
                       const std::vector<std::string>& letter_grid,
                       Point3 origin,
                       double block_size,
                       const Material* material) {
    int rows = static_cast<int>(letter_grid.size());
    for (int row = 0; row < rows; ++row) {
        int cols = static_cast<int>(letter_grid[row].size());
        for (int col = 0; col < cols; ++col) {
            if (letter_grid[row][col] == '#') {
                double x = origin.x() + col * block_size;
                double y = origin.y() + (rows - 1 - row) * block_size;
                double z = origin.z();
                Point3 bmin(x, y, z);
                Point3 bmax(x + block_size, y + block_size, z + block_size);
                scene.add_shape(std::make_shared<Box>(bmin, bmax, material));
            }
        }
    }
}

static Scene build_scene() {
    auto white_metal = std::make_shared<Metal>(Color3(0.9, 0.9, 0.9), 0.05);
    auto black_metal = std::make_shared<Metal>(Color3(0.1, 0.1, 0.1), 0.05);
    auto green_glass = std::make_shared<Dielectric>(1.5, Color3(0.4, 0.95, 0.4));
    auto red_mat    = std::make_shared<Lambertian>(Color3(0.85, 0.15, 0.15));
    auto blue_mat   = std::make_shared<Lambertian>(Color3(0.15, 0.25, 0.85));
    auto orange_mat = std::make_shared<Lambertian>(Color3(0.9, 0.55, 0.1));
    auto purple_mat = std::make_shared<Lambertian>(Color3(0.6, 0.2, 0.8));

    Scene scene;

    static std::vector<std::shared_ptr<Material>> materials;
    materials = {white_metal, black_metal, green_glass, red_mat, blue_mat, orange_mat, purple_mat};

    // Chessboard
    double sq_size = 1.0;
    double sq_height = 0.15;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            double x = -4.0 + col * sq_size;
            double z = -4.0 + row * sq_size;
            bool is_white = (row + col) % 2 == 0;
            const Material* mat = is_white ? white_metal.get() : black_metal.get();
            scene.add_shape(std::make_shared<Box>(
                Point3(x, -sq_height, z),
                Point3(x + sq_size, 0.0, z + sq_size), mat));
        }
    }

    // Letters
    double block = 0.12;
    double letter_width = 5 * block;
    double gap = 0.12;
    double total_width = 5 * letter_width + 4 * gap;
    double start_x = -total_width / 2.0;
    double letter_z = 0.5;

    struct LetterDef { const std::vector<std::string>& grid; const Material* mat; };
    std::vector<LetterDef> letters = {
        {LETTER_n, red_mat.get()},
        {LETTER_W, green_glass.get()},
        {LETTER_a, blue_mat.get()},
        {LETTER_v, orange_mat.get()},
        {LETTER_e, purple_mat.get()}
    };

    double x_cursor = start_x;
    for (const auto& ldef : letters) {
        add_letter(scene, ldef.grid, Point3(x_cursor, 0.0, letter_z), block, ldef.mat);
        x_cursor += letter_width + gap;
    }

    // Light
    scene.add_light(std::make_shared<PointLight>(
        Point3(-4, 10, 2), Color3(1.0, 0.97, 0.9), 0.7));

    return scene;
}

static int run_legacy_single_frame() {
    Scene scene = build_scene();
    Renderer renderer;
    Point3 lookat(0, 0.3, 0.5);

    Camera camera(
        Point3(2, 3, 6), lookat,
        Vec3(0, 1, 0), 38.0, 16.0 / 9.0, 3840);

    RenderSettings settings;
    settings.samples_per_pixel = 48;
    settings.max_depth = 12;

    std::cout << "Rendering nWave chessboard scene (" << camera.image_width()
              << "x" << camera.image_height() << ", " << settings.samples_per_pixel
              << " SPP)...\n";
    std::cout << "Scene: " << scene.shapes().size() << " objects, "
              << scene.lights().size() << " lights\n";

    auto pixels = renderer.render(camera, scene, settings);
    write_ppm("nwave_scene.ppm", pixels, camera.image_width(), camera.image_height());
    std::cout << "Done! Saved nwave_scene.ppm\n";
    return 0;
}

static int run_legacy_animate() {
    Scene scene = build_scene();
    Renderer renderer;
    Point3 lookat(0, 0.3, 0.5);

    std::filesystem::create_directories("frames");

    RenderSettings settings;
    settings.samples_per_pixel = 16;
    settings.max_depth = 10;

    Point3 cam_start(2, 3, 6);
    double cam_height = cam_start.y();
    double dx = cam_start.x() - lookat.x();
    double dz = cam_start.z() - lookat.z();
    double radius = std::sqrt(dx * dx + dz * dz);
    double start_angle = std::atan2(dx, dz);

    int total_frames = 720;
    double angle_step = degrees_to_radians(0.5);

    std::cout << "Animation: " << total_frames << " frames, 800x450, "
              << settings.samples_per_pixel << " SPP\n";
    std::cout << "Orbit radius: " << radius << ", height: " << cam_height << "\n";
    std::cout << "Scene: " << scene.shapes().size() << " objects, "
              << scene.lights().size() << " lights\n";

    for (int frame = 0; frame < total_frames; ++frame) {
        double angle = start_angle + frame * angle_step;
        double cx = lookat.x() + radius * std::sin(angle);
        double cz = lookat.z() + radius * std::cos(angle);

        Camera camera(
            Point3(cx, cam_height, cz), lookat,
            Vec3(0, 1, 0), 38.0, 16.0 / 9.0, 800);

        auto pixels = renderer.render(camera, scene, settings);

        std::ostringstream filename;
        filename << "frames/frame_" << std::setfill('0') << std::setw(4) << frame << ".ppm";
        write_ppm(filename.str(), pixels, camera.image_width(), camera.image_height());

        std::cout << "\rFrame " << (frame + 1) << "/" << total_frames << std::flush;
    }
    std::cout << "\nAll frames rendered! Creating video...\n";
    std::cout << "Run: ffmpeg -framerate 30 -i frames/frame_%04d.ppm -c:v libx264 -pix_fmt yuv420p nwave_orbit.mp4\n";
    return 0;
}

static constexpr double default_aspect_ratio = 16.0 / 9.0;
static constexpr int default_max_depth = 10;

static RenderBackendType parse_backend(const std::string& name) {
    if (name == "metal") return RenderBackendType::METAL;
    return RenderBackendType::CPU;
}
static constexpr int default_render_spp = 16;
static constexpr int animation_spp = 1;

static bool read_scene_file(const std::string& path, std::string& content) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open scene file: " << path << "\n";
        return false;
    }
    content.assign(std::istreambuf_iterator<char>(file),
                   std::istreambuf_iterator<char>());
    return true;
}

static Camera build_camera_with_overrides(const std::string& yaml_content,
                                          const Camera& base_camera,
                                          int width_override) {
    if (width_override <= 0) {
        return base_camera;
    }

    YAML::Node root = YAML::Load(yaml_content);
    const auto& cam = root["camera"];

    auto parse_vec3 = [](const YAML::Node& node) {
        return Vec3(node[0].as<double>(), node[1].as<double>(), node[2].as<double>());
    };

    Point3 lookfrom = parse_vec3(cam["lookfrom"]);
    Point3 lookat = parse_vec3(cam["lookat"]);
    Vec3 vup = parse_vec3(cam["vup"]);
    double vfov = cam["vfov"].as<double>();
    double aspect_ratio = cam["aspect_ratio"]
        ? cam["aspect_ratio"].as<double>() : default_aspect_ratio;

    return Camera(lookfrom, lookat, vup, vfov, aspect_ratio, width_override);
}

static int run_physics_animate(const RenderCommand& cmd) {
    RenderBackendType backend = parse_backend(cmd.backend);

#ifndef NWAVE_HAS_METAL
    if (backend == RenderBackendType::METAL) {
        std::cerr << "Error: Metal backend is only available on macOS\n";
        return 1;
    }
#endif

    std::string yaml_content;
    if (!read_scene_file(cmd.scene_file, yaml_content)) {
        return 1;
    }

    YamlSceneLoader loader;
    auto result = loader.load(yaml_content);

    if (!result.animation_config.has_value()) {
        std::cerr << "Error: scene file does not contain animation_config\n";
        return 1;
    }

    const AnimationConfig& anim_config = result.animation_config.value();
    Camera camera = build_camera_with_overrides(yaml_content, result.camera, cmd.width);

    std::filesystem::create_directories(anim_config.output_directory);

    int body_count = static_cast<int>(result.shape_physics.size());
    int total_steps = anim_config.total_frames() * anim_config.steps_per_frame();
    std::cout << "Physics: " << body_count << " bodies, " << total_steps << " total steps\n";

    auto physics = std::make_unique<JoltPhysicsSimulator>();
    int spp = (cmd.spp > 0) ? cmd.spp : animation_spp;

    std::unique_ptr<RenderBackend> render_backend;

#ifdef NWAVE_HAS_METAL
    if (backend == RenderBackendType::METAL) {
        auto metal_backend = std::make_unique<MetalRenderBackend>();
        if (!metal_backend->initialise("nwave_shaders.metallib")) {
            std::cerr << "Error: failed to initialise Metal backend for animation\n";
            return 1;
        }
        render_backend = std::move(metal_backend);
    }
#endif

    if (!render_backend) {
        auto cpu_backend = std::make_unique<CpuRenderBackend>();
        cpu_backend->set_quiet(true);
        render_backend = std::move(cpu_backend);
    }

    RenderBackend* backend_ptr = render_backend.get();

    WriteCallback write_cb = [spp, backend_ptr](const std::string& filename,
                                                 const Scene& scene,
                                                 const Camera& cam,
                                                 int /*width*/, int /*spp_hint*/) {
        RenderSettings settings;
        settings.samples_per_pixel = spp;
        settings.max_depth = default_max_depth;

        auto pixels = backend_ptr->render(cam, scene, settings);
        write_ppm(filename, pixels, cam.image_width(), cam.image_height());
    };

    StreamProgressReporter progress(std::cerr);
    AnimationRenderer anim_renderer(anim_config, result.scene, result.shape_physics,
                                    std::move(physics), camera, std::move(write_cb),
                                    &progress);
    int frames_rendered = anim_renderer.render();

    std::cout << "Rendered " << frames_rendered << " frames to " << anim_config.output_directory << "\n";
    std::cout << "Run: ffmpeg -framerate " << static_cast<int>(anim_config.render_fps)
              << " -i " << anim_config.output_directory << "frame_%04d.ppm output.mp4\n";

    return 0;
}

static int run_render(const RenderCommand& cmd) {
    RenderBackendType backend = parse_backend(cmd.backend);

#ifndef NWAVE_HAS_METAL
    if (backend == RenderBackendType::METAL) {
        std::cerr << "Error: Metal backend is only available on macOS\n";
        return 1;
    }
#endif

    std::string yaml_content;
    if (!read_scene_file(cmd.scene_file, yaml_content)) {
        return 1;
    }

    YamlSceneLoader loader;
    auto result = loader.load(yaml_content);

    Camera camera = build_camera_with_overrides(yaml_content, result.camera, cmd.width);

    RenderSettings settings;
    settings.samples_per_pixel = (cmd.spp > 0) ? cmd.spp : default_render_spp;
    settings.max_depth = default_max_depth;
    settings.backend = backend;

    std::cout << "Rendering scene from " << cmd.scene_file << " ("
              << camera.image_width() << "x" << camera.image_height()
              << ", " << settings.samples_per_pixel << " SPP)...\n";
    std::cout << "Scene: " << result.scene.shapes().size() << " objects, "
              << result.scene.lights().size() << " lights\n";

    std::unique_ptr<RenderBackend> render_backend;

#ifdef NWAVE_HAS_METAL
    if (backend == RenderBackendType::METAL) {
        std::cout << "Using Metal backend\n";
        auto metal_backend = std::make_unique<MetalRenderBackend>();
        if (!metal_backend->initialise("nwave_shaders.metallib")) {
            std::cerr << "Error: failed to initialise Metal backend\n";
            return 1;
        }
        render_backend = std::move(metal_backend);
    }
#endif

    if (!render_backend) {
        render_backend = std::make_unique<CpuRenderBackend>();
    }

    auto pixels = render_backend->render(camera, result.scene, settings);
    if (pixels.empty()) {
        std::cerr << "Error: render backend returned no pixels\n";
        return 1;
    }
    write_ppm(cmd.output, pixels, camera.image_width(), camera.image_height());
    std::cout << "Done! Saved " << cmd.output << "\n";
    return 0;
}

static int run_validate(const ValidateCommand& cmd) {
    std::string yaml_content;
    if (!read_scene_file(cmd.scene_file, yaml_content)) {
        return 1;
    }

    SceneValidator validator;
    auto result = validator.validate(yaml_content);

    if (result.is_valid) {
        std::cout << "Scene validation passed:\n";
        std::cout << "  [x] Camera present\n";
        std::cout << "  [x] Objects present\n";
        std::cout << "  [x] Lights present\n";
        std::cout << "  [x] Material references valid\n";
        std::cout << "  [x] Parameter ranges valid\n";
    } else {
        std::cout << "Scene validation failed:\n";
        for (const auto& error : result.errors) {
            std::cout << "  [ ] " << error.message << "\n";
        }
    }

    return result.is_valid ? 0 : 1;
}

int main(int argc, char* argv[]) {
    // Preserve legacy behavior: no args runs the default hardcoded scene
    if (argc == 1) {
        return run_legacy_single_frame();
    }

    CliDispatcher dispatcher(std::cout, std::cerr);
    dispatcher.set_render_handler([](const RenderCommand& cmd) {
        if (cmd.physics_animate) {
            return run_physics_animate(cmd);
        }
        return run_render(cmd);
    });
    dispatcher.set_validate_handler(run_validate);
    dispatcher.set_legacy_handler([&]() {
        // --animate is dispatched here
        return run_legacy_animate();
    });

    return dispatcher.dispatch(argc, argv);
}
