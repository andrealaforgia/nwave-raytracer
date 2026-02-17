#include <gtest/gtest.h>
#include "application/animation_renderer.h"
#include "application/physics_simulator.h"
#include "domain/animation_config.h"
#include "domain/scene.h"
#include "domain/camera.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/box.h"
#include "domain/shapes/plane.h"
#include "domain/materials/lambertian.h"
#include "domain/physics_properties.h"
#include "core/matrix4x4.h"
#include <string>
#include <vector>
#include <functional>

namespace nwave {
namespace {

// --- Fake PhysicsSimulator that records calls ---

class FakePhysicsSimulator : public PhysicsSimulator {
public:
    struct AddBodyCall {
        PhysicsBodyDesc desc;
    };

    struct StepCall {
        double dt;
    };

    int add_body(const PhysicsBodyDesc& desc) override {
        add_body_calls.push_back({desc});
        int id = next_body_id_++;
        // Store initial transform
        transforms_[id] = PhysicsTransform{desc.position, desc.rotation};
        return id;
    }

    void step(double dt) override {
        step_calls.push_back({dt});
        // Simulate simple downward motion for dynamic bodies
        for (auto& [id, transform] : transforms_) {
            transform.position = Point3(
                transform.position.x(),
                transform.position.y() - 0.01,
                transform.position.z()
            );
        }
    }

    PhysicsTransform get_transform(int body_id) const override {
        get_transform_calls.push_back(body_id);
        auto it = transforms_.find(body_id);
        if (it != transforms_.end()) {
            return it->second;
        }
        return PhysicsTransform{};
    }

    void set_gravity(const Vec3& gravity) override {
        gravity_ = gravity;
    }

    // Recorded calls for verification
    std::vector<AddBodyCall> add_body_calls;
    std::vector<StepCall> step_calls;
    mutable std::vector<int> get_transform_calls;
    Vec3 gravity_{0, 0, 0};

private:
    int next_body_id_ = 0;
    std::map<int, PhysicsTransform> transforms_;
};

// --- Helper to create a test scene with shapes and physics ---

struct TestSceneSetup {
    Scene scene;
    std::vector<PhysicsProperties> shape_physics;
    std::vector<std::shared_ptr<Material>> materials;
    Camera camera{Point3(0, 0, 5), Point3(0, 0, 0), Vec3(0, 1, 0), 60.0, 1.0, 100};
};

TestSceneSetup create_test_scene_with_dynamic_box() {
    TestSceneSetup setup;

    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.2, 0.2));
    setup.materials.push_back(mat);

    // Ground plane (static)
    auto ground = std::make_shared<Plane>(Point3(0, -1, 0), Vec3(0, 1, 0), mat.get());
    setup.scene.add_shape(ground);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::STATIC, 0.0, Vec3(0, 0, 0), 0.5, 0.3});

    // Dynamic box
    auto box = std::make_shared<Box>(Point3(-0.5, 0, -0.5), Point3(0.5, 1, 0.5), mat.get());
    setup.scene.add_shape(box);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), 0.5, 0.3});

    return setup;
}

// ==========================================================
// ACCEPTANCE TEST: Full animation render cycle
// ==========================================================

TEST(AnimationRendererAcceptance, RendersAnimationWithPhysicsSimulation) {
    // Setup: 1 second at 10fps = 10 frames, physics at 1/100s = 10 steps/frame
    AnimationConfig config{1.0, 0.01, 10.0, "frames/"};
    ASSERT_EQ(config.total_frames(), 10);
    ASSERT_EQ(config.steps_per_frame(), 10);

    auto test_scene = create_test_scene_with_dynamic_box();
    auto physics = std::make_unique<FakePhysicsSimulator>();
    auto* physics_ptr = physics.get();

    // Track write callback invocations
    std::vector<std::string> written_filenames;
    auto write_callback = [&](const std::string& filename, const Scene& scene,
                              const Camera& camera, int width, int spp) {
        written_filenames.push_back(filename);
    };

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_callback);

    int frame_count = renderer.render();

    // AC1: Frame count equals ceil(duration * render_fps)
    EXPECT_EQ(frame_count, 10);

    // AC2: Physics stepped steps_per_frame times between each rendered frame
    // 10 frames * 10 steps/frame = 100 total physics steps
    EXPECT_EQ(static_cast<int>(physics_ptr->step_calls.size()), 100);

    // AC3: TransformedShape transforms updated from physics each frame
    // The dynamic box (body 1) should have get_transform called once per frame = 10 times
    int dynamic_body_transform_reads = 0;
    for (int id : physics_ptr->get_transform_calls) {
        if (id == 1) { // body id 1 = the dynamic box
            dynamic_body_transform_reads++;
        }
    }
    EXPECT_EQ(dynamic_body_transform_reads, 10);

    // AC4: Write callback invoked once per frame with valid filename
    ASSERT_EQ(static_cast<int>(written_filenames.size()), 10);
    EXPECT_EQ(written_filenames[0], "frames/frame_0000.ppm");
    EXPECT_EQ(written_filenames[9], "frames/frame_0009.ppm");
}

// ==========================================================
// UNIT TESTS: Behavior-focused through AnimationRenderer driving port
// ==========================================================

// Behavior 1: Frame count calculation
TEST(AnimationRenderer, ReturnsCorrectFrameCountForFractionalDuration) {
    // 0.15 seconds at 10fps = ceil(1.5) = 2 frames
    AnimationConfig config{0.15, 0.01, 10.0, "output/"};
    ASSERT_EQ(config.total_frames(), 2);

    auto test_scene = create_test_scene_with_dynamic_box();
    auto physics = std::make_unique<FakePhysicsSimulator>();

    std::vector<std::string> filenames;
    auto write_cb = [&](const std::string& filename, const Scene&,
                        const Camera&, int, int) {
        filenames.push_back(filename);
    };

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    int frame_count = renderer.render();

    EXPECT_EQ(frame_count, 2);
    EXPECT_EQ(static_cast<int>(filenames.size()), 2);
}

// Behavior 2: Physics stepping frequency
TEST(AnimationRenderer, StepsPhysicsCorrectNumberOfTimesPerFrame) {
    // 3 frames, 5 steps per frame = 15 total steps
    // duration=0.3, fps=10 => 3 frames; physics_timestep=0.02, render_dt=0.1 => 5 steps/frame
    AnimationConfig config{0.3, 0.02, 10.0, "out/"};
    ASSERT_EQ(config.total_frames(), 3);
    ASSERT_EQ(config.steps_per_frame(), 5);

    auto test_scene = create_test_scene_with_dynamic_box();
    auto physics = std::make_unique<FakePhysicsSimulator>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, int) {};

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // Each step should use physics_timestep as dt
    EXPECT_EQ(static_cast<int>(physics_ptr->step_calls.size()), 15);
    for (const auto& call : physics_ptr->step_calls) {
        EXPECT_DOUBLE_EQ(call.dt, 0.02);
    }
}

// Behavior 3: TransformedShape transforms updated from physics
TEST(AnimationRenderer, UpdatesTransformedShapeTransformsFromPhysics) {
    AnimationConfig config{0.1, 0.01, 10.0, "out/"};
    ASSERT_EQ(config.total_frames(), 1);

    auto test_scene = create_test_scene_with_dynamic_box();
    auto physics = std::make_unique<FakePhysicsSimulator>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, int) {};

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // Dynamic body (id=1) should have its transform queried
    bool dynamic_body_queried = false;
    for (int id : physics_ptr->get_transform_calls) {
        if (id == 1) {
            dynamic_body_queried = true;
            break;
        }
    }
    EXPECT_TRUE(dynamic_body_queried);
}

// Behavior 4: Write callback filenames
TEST(AnimationRenderer, GeneratesCorrectFrameFilenames) {
    AnimationConfig config{0.3, 0.01, 10.0, "my_frames/"};
    ASSERT_EQ(config.total_frames(), 3);

    auto test_scene = create_test_scene_with_dynamic_box();
    auto physics = std::make_unique<FakePhysicsSimulator>();

    std::vector<std::string> filenames;
    auto write_cb = [&](const std::string& filename, const Scene&,
                        const Camera&, int, int) {
        filenames.push_back(filename);
    };

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    ASSERT_EQ(static_cast<int>(filenames.size()), 3);
    EXPECT_EQ(filenames[0], "my_frames/frame_0000.ppm");
    EXPECT_EQ(filenames[1], "my_frames/frame_0001.ppm");
    EXPECT_EQ(filenames[2], "my_frames/frame_0002.ppm");
}

// Behavior 5: Bodies added to physics simulator
TEST(AnimationRenderer, AddsBodiesForAllShapes) {
    AnimationConfig config{0.1, 0.01, 10.0, "out/"};

    auto test_scene = create_test_scene_with_dynamic_box();
    auto physics = std::make_unique<FakePhysicsSimulator>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, int) {};

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // Should add 2 bodies: ground plane (static) and box (dynamic)
    EXPECT_EQ(static_cast<int>(physics_ptr->add_body_calls.size()), 2);

    // Check body types
    EXPECT_EQ(physics_ptr->add_body_calls[0].desc.properties.body_type, BodyType::STATIC);
    EXPECT_EQ(physics_ptr->add_body_calls[1].desc.properties.body_type, BodyType::DYNAMIC);
}

} // namespace
} // namespace nwave
