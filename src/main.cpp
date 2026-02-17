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
#include "infrastructure/ppm_writer.h"
#include "infrastructure/yaml_scene_loader.h"
#include "infrastructure/cli_dispatcher.h"
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

static Camera build_camera_with_overrides(const std::string& yaml_content,
                                          const Camera& base_camera,
                                          int width_override) {
    if (width_override <= 0) {
        return base_camera;
    }

    // Re-parse YAML to get camera parameters for reconstruction with new width
    YAML::Node root = YAML::Load(yaml_content);
    const auto& cam = root["camera"];

    auto parse_vec3 = [](const YAML::Node& node) {
        return Vec3(node[0].as<double>(), node[1].as<double>(), node[2].as<double>());
    };

    Point3 lookfrom = parse_vec3(cam["lookfrom"]);
    Point3 lookat = parse_vec3(cam["lookat"]);
    Vec3 vup = parse_vec3(cam["vup"]);
    double vfov = cam["vfov"].as<double>();
    double aspect_ratio = cam["aspect_ratio"] ? cam["aspect_ratio"].as<double>() : 16.0 / 9.0;

    return Camera(lookfrom, lookat, vup, vfov, aspect_ratio, width_override);
}

static int run_render(const RenderCommand& cmd) {
    std::ifstream file(cmd.scene_file);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open scene file: " << cmd.scene_file << "\n";
        return 1;
    }

    std::string yaml_content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    YamlSceneLoader loader;
    auto result = loader.load(yaml_content);

    Camera camera = build_camera_with_overrides(yaml_content, result.camera, cmd.width);

    RenderSettings settings;
    settings.samples_per_pixel = (cmd.spp > 0) ? cmd.spp : 16;
    settings.max_depth = 10;

    std::cout << "Rendering scene from " << cmd.scene_file << " ("
              << camera.image_width() << "x" << camera.image_height()
              << ", " << settings.samples_per_pixel << " SPP)...\n";
    std::cout << "Scene: " << result.scene.shapes().size() << " objects, "
              << result.scene.lights().size() << " lights\n";

    Renderer renderer;
    auto pixels = renderer.render(camera, result.scene, settings);
    write_ppm(cmd.output, pixels, camera.image_width(), camera.image_height());
    std::cout << "Done! Saved " << cmd.output << "\n";
    return 0;
}

int main(int argc, char* argv[]) {
    // Preserve legacy behavior: no args runs the default hardcoded scene
    if (argc == 1) {
        return run_legacy_single_frame();
    }

    CliDispatcher dispatcher(std::cout, std::cerr);
    dispatcher.set_render_handler(run_render);
    dispatcher.set_legacy_handler([&]() {
        // --animate is dispatched here
        return run_legacy_animate();
    });

    return dispatcher.dispatch(argc, argv);
}
