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
#include "application/renderer.h"
#include "infrastructure/ppm_writer.h"
#include <iostream>
#include <memory>
#include <vector>
#include <string>

using namespace nwave;

// 5x7 bitmap font definitions ('#' = filled, '.' = empty)
// Each letter is 7 rows of 5 characters, read top to bottom
static const std::vector<std::string> LETTER_n = {
    ".....",
    ".....",
    ".##..",
    "#..#.",
    "#..#.",
    "#..#.",
    "#..#."
};

static const std::vector<std::string> LETTER_W = {
    "#...#",
    "#...#",
    "#...#",
    "#.#.#",
    "#.#.#",
    "##.##",
    "#...#"
};

static const std::vector<std::string> LETTER_a = {
    ".....",
    ".....",
    ".###.",
    "...#.",
    ".###.",
    "#..#.",
    ".##.#"
};

static const std::vector<std::string> LETTER_v = {
    ".....",
    ".....",
    "#...#",
    "#...#",
    ".#.#.",
    ".#.#.",
    "..#.."
};

static const std::vector<std::string> LETTER_e = {
    ".....",
    ".....",
    ".##..",
    "#..#.",
    "####.",
    "#....",
    ".###."
};

// Add a 3D block letter to the scene at a given position
// letter_grid: 7 rows x 5 cols bitmap
// origin: bottom-left-front corner of the letter bounding box
// block_size: size of each voxel block
// material: material for the blocks
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
                // Row 0 is the top of the letter, so invert y
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

int main() {
    // === Materials ===
    auto white_metal = std::make_shared<Metal>(Color3(0.9, 0.9, 0.9), 0.05);
    auto black_metal = std::make_shared<Metal>(Color3(0.1, 0.1, 0.1), 0.05);
    auto green_glass = std::make_shared<Dielectric>(1.5, Color3(0.4, 0.95, 0.4));

    // Letter materials (colored Lambertian)
    auto red_mat    = std::make_shared<Lambertian>(Color3(0.85, 0.15, 0.15));
    auto blue_mat   = std::make_shared<Lambertian>(Color3(0.15, 0.25, 0.85));
    auto orange_mat = std::make_shared<Lambertian>(Color3(0.9, 0.55, 0.1));
    auto purple_mat = std::make_shared<Lambertian>(Color3(0.6, 0.2, 0.8));

    // === Scene ===
    Scene scene;

    // --- Chessboard (8x8 grid of flat boxes) ---
    // Board centered at x in [-4, 4], z in [-4, 4], surface at y = 0
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
                Point3(x + sq_size, 0.0, z + sq_size),
                mat
            ));
        }
    }

    // --- 3D Block Letters "nWave" standing on the chessboard ---
    // Letter parameters
    double block = 0.12;           // Size of each voxel block
    double letter_width = 5 * block;  // 0.6 units per letter
    double gap = 0.12;             // Gap between letters
    double total_width = 5 * letter_width + 4 * gap;  // Total width of "nWave"
    double start_x = -total_width / 2.0;  // Center the text
    double letter_y = 0.0;         // Bottom of letters on the board surface
    double letter_z = 0.5;         // Z position (slightly in front of center, facing +z camera)
    double depth = block;          // Depth of each letter block

    // Assign materials: n=red, W=green glass, a=blue, v=orange, e=purple
    struct LetterDef {
        const std::vector<std::string>& grid;
        const Material* mat;
    };
    std::vector<LetterDef> letters = {
        {LETTER_n, red_mat.get()},
        {LETTER_W, green_glass.get()},
        {LETTER_a, blue_mat.get()},
        {LETTER_v, orange_mat.get()},
        {LETTER_e, purple_mat.get()}
    };

    double x_cursor = start_x;
    for (const auto& ldef : letters) {
        add_letter(scene, ldef.grid, Point3(x_cursor, letter_y, letter_z), block, ldef.mat);
        x_cursor += letter_width + gap;
    }

    // --- Lights ---
    // Single overhead light, slightly to the left (dramatic, dimmed)
    scene.add_light(std::make_shared<PointLight>(
        Point3(-4, 10, 2), Color3(1.0, 0.97, 0.9), 0.7));

    // === Camera ===
    // Slightly from the right, mostly frontal
    Camera camera(
        Point3(2, 3, 6),         // lookfrom: slight right, mostly frontal
        Point3(0, 0.3, 0.5),    // lookat: center of the letters
        Vec3(0, 1, 0),           // vup
        38.0,                     // vfov
        16.0 / 9.0,              // aspect ratio
        3840                      // image width (4K)
    );

    // === Render Settings ===
    RenderSettings settings;
    settings.samples_per_pixel = 48;  // Higher quality for 4K
    settings.max_depth = 12;          // Deep enough for glass reflections

    std::cout << "Rendering nWave chessboard scene (" << camera.image_width()
              << "x" << camera.image_height() << ", " << settings.samples_per_pixel
              << " SPP)...\n";
    std::cout << "Scene: " << scene.shapes().size() << " objects, "
              << scene.lights().size() << " lights\n";

    Renderer renderer;
    auto pixels = renderer.render(camera, scene, settings);

    // === Output ===
    write_ppm("nwave_scene.ppm", pixels, camera.image_width(), camera.image_height());

    std::cout << "Done! Saved nwave_scene.ppm\n";
    return 0;
}
