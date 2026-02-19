#include <gtest/gtest.h>
#include "infrastructure/yaml_scene_loader.h"
#include "infrastructure/jolt_physics_simulator.h"
#include "application/animation_renderer.h"
#include "domain/materials/lambertian.h"
#include "domain/shapes/box.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <string>
#include <vector>

namespace nwave {
namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Find the scene file relative to the build directory.
// ctest runs from the build dir; scene is at ../scenes/jelly_cube_demo.yaml
std::string find_scene_path() {
    // Try relative to build directory first
    std::vector<std::string> candidates = {
        "../scenes/jelly_cube_demo.yaml",
        "scenes/jelly_cube_demo.yaml",
        "../../scenes/jelly_cube_demo.yaml",
    };
    for (const auto& path : candidates) {
        std::ifstream f(path);
        if (f.good()) return path;
    }
    // Fallback: absolute path
    return "/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/jelly_cube_demo.yaml";
}

// ===================================================================
// Integration test: Load jelly_cube_demo.yaml and run animation
// ===================================================================

TEST(JellyDemoIntegration, SceneLoadsAndAnimationRunsWithoutCrashes) {
    // AC1: Scene file loads without errors
    std::string yaml_content = read_file(find_scene_path());
    YamlSceneLoader loader;
    SceneLoadResult result = loader.load(yaml_content);

    ASSERT_TRUE(result.animation_config.has_value())
        << "Scene must have an animation config";

    // Verify pink jelly material (AC5): albedo=[0.9, 0.4, 0.6]
    // The soft body desc should reference a Lambertian material with that albedo.
    ASSERT_FALSE(result.soft_body_descs.empty())
        << "Scene must contain at least one soft body";

    const auto* jelly_material = result.soft_body_descs[0].material;
    ASSERT_NE(jelly_material, nullptr);
    const auto* lambertian = dynamic_cast<const Lambertian*>(jelly_material);
    ASSERT_NE(lambertian, nullptr) << "Jelly cube must use Lambertian material";
    EXPECT_NEAR(lambertian->albedo().x(), 0.9, 1e-6);
    EXPECT_NEAR(lambertian->albedo().y(), 0.4, 1e-6);
    EXPECT_NEAR(lambertian->albedo().z(), 0.6, 1e-6);

    // Verify 'e' letter boxes exist: at least 5 dynamic, start_asleep boxes
    int dynamic_asleep_count = 0;
    for (const auto& physics : result.shape_physics) {
        if (physics.body_type == BodyType::DYNAMIC && physics.start_asleep) {
            dynamic_asleep_count++;
        }
    }
    EXPECT_GE(dynamic_asleep_count, 5)
        << "Expected at least 5 dynamic start_asleep boxes for the 'e' letter";

    // Create a real Jolt physics simulator
    auto physics = std::make_unique<JoltPhysicsSimulator>();

    // Capture initial soft body vertex positions for later comparison (AC7)
    // We need the physics world set up before we can check this, so we do it
    // through the animation renderer.

    // Override animation config for shorter test: 60+ frames, but use short timestep
    AnimationConfig test_config = *result.animation_config;
    // Keep original config but ensure at least 60 frames
    EXPECT_GE(test_config.total_frames(), 60)
        << "Animation must produce at least 60 frames (AC2)";

    int frame_count = 0;

    // Use no-op write callback that just counts frames
    auto write_cb = [&](const std::string& filename, const Scene& scene,
                        const Camera& camera, int width, int spp) {
        frame_count++;
    };

    AnimationRenderer renderer(test_config, result.scene, result.shape_physics,
                               std::move(physics), result.camera, write_cb,
                               nullptr, result.soft_body_descs);

    // AC2: Animation produces at least 60 frames without crashes
    int total = renderer.render();
    EXPECT_GE(total, 60) << "Must render at least 60 frames";
    EXPECT_EQ(frame_count, total) << "Write callback must be called for each frame";
}

TEST(JellyDemoIntegration, SoftBodyVerticesChangeAndLetterDisplacedDuringAnimation) {
    std::string yaml_content = read_file(find_scene_path());
    YamlSceneLoader loader;
    SceneLoadResult result = loader.load(yaml_content);

    ASSERT_TRUE(result.animation_config.has_value());
    ASSERT_FALSE(result.soft_body_descs.empty());

    auto physics = std::make_unique<JoltPhysicsSimulator>();
    auto* physics_ptr = physics.get();

    // Add the rigid bodies first to track 'e' letter displacement
    const auto& shapes = result.scene.shapes();
    std::vector<int> body_ids;
    for (size_t i = 0; i < shapes.size(); ++i) {
        PhysicsProperties props;
        if (i < result.shape_physics.size()) {
            props = result.shape_physics[i];
        }

        PhysicsBodyDesc desc;
        desc.properties = props;
        // Simple mapping for boxes
        if (auto* box = dynamic_cast<const Box*>(shapes[i].get())) {
            desc.shape_type = PhysicsShapeType::BOX;
            auto bmin = box->box_min();
            auto bmax = box->box_max();
            desc.dimensions = Vec3(
                (bmax.x() - bmin.x()) / 2.0,
                (bmax.y() - bmin.y()) / 2.0,
                (bmax.z() - bmin.z()) / 2.0);
            desc.position = Point3(
                (bmin.x() + bmax.x()) / 2.0,
                (bmin.y() + bmax.y()) / 2.0,
                (bmin.z() + bmax.z()) / 2.0);
        }
        int id = physics_ptr->add_body(desc);
        body_ids.push_back(id);
    }

    // Record initial positions of dynamic (asleep) bodies
    std::vector<Point3> initial_positions;
    std::vector<int> dynamic_indices;
    for (size_t i = 0; i < result.shape_physics.size(); ++i) {
        if (result.shape_physics[i].body_type == BodyType::DYNAMIC &&
            result.shape_physics[i].start_asleep) {
            initial_positions.push_back(physics_ptr->get_transform(body_ids[i]).position);
            dynamic_indices.push_back(static_cast<int>(i));
        }
    }
    ASSERT_GE(dynamic_indices.size(), 5u)
        << "Need at least 5 'e' letter boxes";

    // Add soft body and record initial vertices
    int sb_id = physics_ptr->add_soft_body(result.soft_body_descs[0]);
    SoftBodyMeshData initial_mesh = physics_ptr->get_soft_body_mesh(sb_id);
    ASSERT_FALSE(initial_mesh.vertices.empty());

    // Wake all and run physics for many steps (simulate the cube falling)
    physics_ptr->wake_all();

    double dt = result.animation_config->physics_timestep;
    int total_steps = static_cast<int>(result.animation_config->duration / dt);
    // Run at least enough steps for the cube to fall and hit the letter
    for (int step = 0; step < total_steps; ++step) {
        physics_ptr->step(dt);
    }

    // AC7: At least one frame shows soft body vertices differ from initial positions
    SoftBodyMeshData final_mesh = physics_ptr->get_soft_body_mesh(sb_id);
    bool vertices_changed = false;
    for (size_t i = 0; i < std::min(initial_mesh.vertices.size(), final_mesh.vertices.size()); ++i) {
        double dx = final_mesh.vertices[i].x() - initial_mesh.vertices[i].x();
        double dy = final_mesh.vertices[i].y() - initial_mesh.vertices[i].y();
        double dz = final_mesh.vertices[i].z() - initial_mesh.vertices[i].z();
        if (std::sqrt(dx*dx + dy*dy + dz*dz) > 0.01) {
            vertices_changed = true;
            break;
        }
    }
    EXPECT_TRUE(vertices_changed)
        << "Soft body vertices must change from initial positions after physics simulation (AC7)";

    // AC6: At least one 'e' box displaced from original position
    bool any_displaced = false;
    for (size_t d = 0; d < dynamic_indices.size(); ++d) {
        int idx = dynamic_indices[d];
        PhysicsTransform t = physics_ptr->get_transform(body_ids[idx]);
        double dx = t.position.x() - initial_positions[d].x();
        double dy = t.position.y() - initial_positions[d].y();
        double dz = t.position.z() - initial_positions[d].z();
        if (std::sqrt(dx*dx + dy*dy + dz*dz) > 0.01) {
            any_displaced = true;
            break;
        }
    }
    EXPECT_TRUE(any_displaced)
        << "At least one 'e' letter box must be displaced from original position (AC6)";
}

} // namespace
} // namespace nwave
