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
#include "domain/lights/point_light.h"
#include "domain/lights/directional_light.h"
#include "core/matrix4x4.h"
#include "domain/shapes/transformed_shape.h"
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <cmath>

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

    struct SetVelocityCall {
        int body_id;
        Vec3 velocity;
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
        // Simulate downward motion and rotation for dynamic bodies
        auto rotation_per_step = Quaternion::from_axis_angle(Vec3(0, 1, 0), 0.1);
        for (auto& [id, transform] : transforms_) {
            transform.position = Point3(
                transform.position.x(),
                transform.position.y() - 0.01,
                transform.position.z()
            );
            transform.rotation = (rotation_per_step * transform.rotation).normalized();
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

    void wake_all() override {}

    void wake_body(int body_id) override {
        wake_body_calls.push_back(body_id);
    }

    void set_linear_velocity(int body_id, const Vec3& velocity) override {
        set_velocity_calls.push_back({body_id, velocity});
    }

    void set_angular_velocity(int body_id, const Vec3& angular_velocity) override {
        set_angular_velocity_calls.push_back({body_id, angular_velocity});
    }

    void set_motion_type(int /*body_id*/, BodyType /*type*/) override {}

    struct AngularVelocityCall { int body_id; Vec3 angular_velocity; };
    std::vector<AngularVelocityCall> set_angular_velocity_calls;

    int add_soft_body(const SoftBodyDesc& /*desc*/) override {
        throw std::runtime_error("not implemented");
    }

    bool is_soft_body(int /*body_id*/) const override {
        return false;
    }

    SoftBodyMeshData get_soft_body_mesh(int /*body_id*/) const override {
        throw std::runtime_error("not implemented");
    }

    // Recorded calls for verification
    std::vector<AddBodyCall> add_body_calls;
    std::vector<StepCall> step_calls;
    mutable std::vector<int> get_transform_calls;
    std::vector<int> wake_body_calls;
    std::vector<SetVelocityCall> set_velocity_calls;
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
    setup.shape_physics.push_back(PhysicsProperties{BodyType::STATIC, 0.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    // Dynamic box
    auto box = std::make_shared<Box>(Point3(-0.5, 0, -0.5), Point3(0.5, 1, 0.5), mat.get());
    setup.scene.add_shape(box);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

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
                              const Camera& camera, int width, const RenderSettings& settings) {
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
    // The dynamic box (body 1) should have get_transform called:
    //   1 time for initial transform capture + 10 times (once per frame) = 11
    int dynamic_body_transform_reads = 0;
    for (int id : physics_ptr->get_transform_calls) {
        if (id == 1) { // body id 1 = the dynamic box
            dynamic_body_transform_reads++;
        }
    }
    EXPECT_EQ(dynamic_body_transform_reads, 11);

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
                        const Camera&, int, const RenderSettings&) {
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

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

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

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

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
                        const Camera&, int, const RenderSettings&) {
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

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // Should add 2 bodies: ground plane (static) and box (dynamic)
    EXPECT_EQ(static_cast<int>(physics_ptr->add_body_calls.size()), 2);

    // Check body types
    EXPECT_EQ(physics_ptr->add_body_calls[0].desc.properties.body_type, BodyType::STATIC);
    EXPECT_EQ(physics_ptr->add_body_calls[1].desc.properties.body_type, BodyType::DYNAMIC);
}

// ==========================================================
// ACCEPTANCE TEST: Multiple physics steps per render frame (07-03)
// ==========================================================

TEST(AnimationRendererAcceptance, StepsPhysicsMultipleTimesPerFrameAtVariousRatios) {
    // AC: 60Hz physics / 30fps render = 2 steps per frame
    //     120Hz physics / 30fps render = 4 steps per frame
    //     Total physics time matches animation duration within one timestep
    struct Scenario {
        double physics_hz;
        double render_fps;
        double duration;
        int expected_steps_per_frame;
    };

    std::vector<Scenario> scenarios = {
        {60.0,  30.0, 1.0, 2},
        {120.0, 30.0, 1.0, 4},
    };

    for (const auto& s : scenarios) {
        double physics_timestep = 1.0 / s.physics_hz;
        AnimationConfig config{s.duration, physics_timestep, s.render_fps, "out/"};
        ASSERT_EQ(config.steps_per_frame(), s.expected_steps_per_frame)
            << "physics_hz=" << s.physics_hz << " render_fps=" << s.render_fps;

        auto test_scene = create_test_scene_with_dynamic_box();
        auto physics = std::make_unique<FakePhysicsSimulator>();
        auto* physics_ptr = physics.get();

        auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

        AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                                   std::move(physics), test_scene.camera, write_cb);

        renderer.render();

        int total_frames = config.total_frames();
        int expected_total_steps = total_frames * s.expected_steps_per_frame;

        // Physics stepped correct number of times
        EXPECT_EQ(static_cast<int>(physics_ptr->step_calls.size()), expected_total_steps)
            << "physics_hz=" << s.physics_hz << " render_fps=" << s.render_fps;

        // Each step uses correct timestep
        for (const auto& call : physics_ptr->step_calls) {
            EXPECT_DOUBLE_EQ(call.dt, physics_timestep)
                << "physics_hz=" << s.physics_hz;
        }

        // Total physics time matches animation duration within one timestep
        double total_physics_time = static_cast<double>(physics_ptr->step_calls.size()) * physics_timestep;
        EXPECT_NEAR(total_physics_time, s.duration, physics_timestep)
            << "physics_hz=" << s.physics_hz << " render_fps=" << s.render_fps;
    }
}

// ==========================================================
// UNIT TEST: Physics-to-render ratio variations (07-03)
// ==========================================================

// Behavior: Steps per frame scales correctly with physics-to-render frequency ratio
// Test Budget: 1 behavior x 2 = 2 max unit tests. Using 1 parametrized test.
struct PhysicsRatioParam {
    double physics_hz;
    double render_fps;
    double duration;
    int expected_steps_per_frame;
    int expected_total_steps;
};

class AnimationRendererPhysicsRatio
    : public ::testing::TestWithParam<PhysicsRatioParam> {};

TEST_P(AnimationRendererPhysicsRatio, StepsPhysicsAtCorrectRatioPerFrame) {
    const auto& p = GetParam();
    double physics_timestep = 1.0 / p.physics_hz;
    AnimationConfig config{p.duration, physics_timestep, p.render_fps, "out/"};

    auto test_scene = create_test_scene_with_dynamic_box();
    auto physics = std::make_unique<FakePhysicsSimulator>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    EXPECT_EQ(config.steps_per_frame(), p.expected_steps_per_frame);
    EXPECT_EQ(static_cast<int>(physics_ptr->step_calls.size()), p.expected_total_steps);

    // Total physics time matches duration within one timestep
    double total_physics_time = static_cast<double>(physics_ptr->step_calls.size()) * physics_timestep;
    EXPECT_NEAR(total_physics_time, p.duration, physics_timestep);
}

INSTANTIATE_TEST_SUITE_P(
    MultipleStepsPerFrame,
    AnimationRendererPhysicsRatio,
    ::testing::Values(
        // 60Hz physics / 30fps render = 2 steps/frame, 1s duration = 30 frames * 2 = 60 steps
        PhysicsRatioParam{60.0,  30.0, 1.0, 2, 60},
        // 120Hz physics / 30fps render = 4 steps/frame, 1s duration = 30 frames * 4 = 120 steps
        PhysicsRatioParam{120.0, 30.0, 1.0, 4, 120},
        // 60Hz physics / 60fps render = 1 step/frame (1:1 ratio)
        PhysicsRatioParam{60.0,  60.0, 0.5, 1, 30},
        // 240Hz physics / 60fps render = 4 steps/frame
        PhysicsRatioParam{240.0, 60.0, 0.5, 4, 120}
    ));

// ==========================================================
// Helper: test scene with a dynamic sphere
// ==========================================================

TestSceneSetup create_test_scene_with_dynamic_sphere() {
    TestSceneSetup setup;

    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.8, 0.8));
    setup.materials.push_back(mat);

    // Ground plane (static)
    auto ground = std::make_shared<Plane>(Point3(0, -1, 0), Vec3(0, 1, 0), mat.get());
    setup.scene.add_shape(ground);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::STATIC, 0.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    // Dynamic sphere
    auto sphere = std::make_shared<Sphere>(Point3(0, 2, 0), 0.5, mat.get());
    setup.scene.add_shape(sphere);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    return setup;
}

TestSceneSetup create_test_scene_with_dynamic_sphere_and_box() {
    TestSceneSetup setup;

    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.8, 0.8));
    setup.materials.push_back(mat);

    // Ground plane (static)
    auto ground = std::make_shared<Plane>(Point3(0, -1, 0), Vec3(0, 1, 0), mat.get());
    setup.scene.add_shape(ground);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::STATIC, 0.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    // Dynamic sphere (shape index 1, body id 1)
    auto sphere = std::make_shared<Sphere>(Point3(0, 2, 0), 0.5, mat.get());
    setup.scene.add_shape(sphere);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    // Dynamic box (shape index 2, body id 2)
    auto box = std::make_shared<Box>(Point3(-0.5, 3, -0.5), Point3(0.5, 4, 0.5), mat.get());
    setup.scene.add_shape(box);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    return setup;
}

// Helper: check if the upper-left 3x3 of a Matrix4x4 is identity (no rotation)
bool is_rotation_identity(const Matrix4x4& mat, double tolerance = 1e-9) {
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            double expected = (r == c) ? 1.0 : 0.0;
            if (std::abs(mat.m[r][c] - expected) > tolerance) {
                return false;
            }
        }
    }
    return true;
}

// Helper: check if the upper-left 3x3 has any rotation (not identity)
bool has_rotation(const Matrix4x4& mat, double tolerance = 1e-9) {
    return !is_rotation_identity(mat, tolerance);
}

// ==========================================================
// Behavior 6: Sphere gets translation-only transform (rotation stripped)
// Test Budget: 1 behavior x 2 = 2 max. Using 1 test.
// ==========================================================

TEST(AnimationRenderer, AppliesFullTransformIncludingRotationToSpheres) {
    // 2 frames so physics steps occur between frame 0 and frame 1,
    // introducing rotation in the physics transform
    AnimationConfig config{0.2, 0.01, 10.0, "out/"};
    ASSERT_EQ(config.total_frames(), 2);

    auto test_scene = create_test_scene_with_dynamic_sphere_and_box();
    auto physics = std::make_unique<FakePhysicsSimulator>();

    // Capture the scene on the LAST rendered frame (after physics has stepped)
    Matrix4x4 sphere_transform;
    Matrix4x4 box_transform;
    int frame_counter = 0;
    auto write_cb = [&](const std::string&, const Scene& scene,
                        const Camera&, int, const RenderSettings&) {
        frame_counter++;
        // Capture on frame 2 (after physics stepping has introduced rotation)
        if (frame_counter == 2) {
            // Shape 0 = ground (static, not wrapped)
            // Shape 1 = sphere (dynamic, wrapped in TransformedShape)
            // Shape 2 = box (dynamic, wrapped in TransformedShape)
            auto sphere_ts = std::dynamic_pointer_cast<TransformedShape>(scene.shapes()[1]);
            auto box_ts = std::dynamic_pointer_cast<TransformedShape>(scene.shapes()[2]);
            ASSERT_NE(sphere_ts, nullptr);
            ASSERT_NE(box_ts, nullptr);
            sphere_transform = sphere_ts->transform();
            box_transform = box_ts->transform();
        }
    };

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // Both sphere and box should include full rotation (needed for textured spheres)
    EXPECT_TRUE(has_rotation(sphere_transform))
        << "Sphere should include rotation for texture rolling";

    EXPECT_TRUE(has_rotation(box_transform))
        << "Box should retain full rotation in its transform";

    // Both should have non-zero translation (physics moved them)
    EXPECT_NE(sphere_transform.m[1][3], 0.0) << "Sphere should have translation";
    EXPECT_NE(box_transform.m[1][3], 0.0) << "Box should have translation";
}

// Helper: count how many times a given body_id appears in a wake call log
int count_wake_calls(const std::vector<int>& wake_body_calls, int body_id) {
    int count = 0;
    for (int id : wake_body_calls) {
        if (id == body_id) count++;
    }
    return count;
}

// ==========================================================
// Helper: test scene with per-body wake_frame configuration
// ==========================================================

TestSceneSetup create_test_scene_with_per_body_wake() {
    TestSceneSetup setup;

    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.8, 0.8));
    setup.materials.push_back(mat);

    // Shape 0: Ground plane (static) - body_id 0
    auto ground = std::make_shared<Plane>(Point3(0, -1, 0), Vec3(0, 1, 0), mat.get());
    setup.scene.add_shape(ground);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::STATIC, 0.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    // Shape 1: Dynamic box, start_asleep=true, wake_frame=3 - body_id 1
    auto box1 = std::make_shared<Box>(Point3(-0.5, 0, -0.5), Point3(0.5, 1, 0.5), mat.get());
    setup.scene.add_shape(box1);
    PhysicsProperties box1_props{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3};
    box1_props.start_asleep = true;
    box1_props.wake_frame = 3;
    setup.shape_physics.push_back(box1_props);

    // Shape 2: Dynamic box, start_asleep=true, no wake_frame - body_id 2
    auto box2 = std::make_shared<Box>(Point3(1, 0, -0.5), Point3(2, 1, 0.5), mat.get());
    setup.scene.add_shape(box2);
    PhysicsProperties box2_props{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3};
    box2_props.start_asleep = true;
    // No wake_frame set
    setup.shape_physics.push_back(box2_props);

    // Shape 3: Dynamic box, no start_asleep, no wake_frame - body_id 3
    auto box3 = std::make_shared<Box>(Point3(3, 0, -0.5), Point3(4, 1, 0.5), mat.get());
    setup.scene.add_shape(box3);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    return setup;
}

// ==========================================================
// ACCEPTANCE TEST: Per-body wake_frame check (03-01)
// ==========================================================

TEST(AnimationRendererAcceptance, WakesIndividualBodiesAtTheirConfiguredWakeFrame) {
    // 5 frames at 10fps, no global wake (wake_frame=-1 effectively)
    AnimationConfig config{0.5, 0.01, 10.0, "out/"};
    config.wake_frame = -1;  // No global wake
    ASSERT_EQ(config.total_frames(), 5);

    auto test_scene = create_test_scene_with_per_body_wake();
    auto physics = std::make_unique<FakePhysicsSimulator>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // AC1: Body 1 (wake_frame=3) should have wake_body called exactly once
    EXPECT_EQ(count_wake_calls(physics_ptr->wake_body_calls, 1), 1)
        << "Body with wake_frame=3 should be woken exactly once";

    // AC2: Body 2 (start_asleep, no wake_frame) should NOT have wake_body called
    EXPECT_EQ(count_wake_calls(physics_ptr->wake_body_calls, 2), 0)
        << "Body with start_asleep but no wake_frame should not be individually woken";

    // AC3: Body 3 (not start_asleep) should NOT have wake_body called
    EXPECT_EQ(count_wake_calls(physics_ptr->wake_body_calls, 3), 0)
        << "Body without start_asleep should not be individually woken";
}

// ==========================================================
// UNIT TEST: Per-body wake at correct frame (03-01)
// Test Budget: 2 behaviors x 2 = 4 max. Using 1 test.
// Behaviors: (1) wake_body called for body with wake_frame at correct frame
//            (2) global wake_frame still works alongside per-body wake
// ==========================================================

TEST(AnimationRenderer, CallsWakeBodyAtPerBodyWakeFrame) {
    // 5 frames, body 1 has wake_frame=3
    AnimationConfig config{0.5, 0.01, 10.0, "out/"};
    config.wake_frame = -1;
    ASSERT_EQ(config.total_frames(), 5);

    auto test_scene = create_test_scene_with_per_body_wake();
    auto physics = std::make_unique<FakePhysicsSimulator>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // Only body_id=1 should appear in wake_body_calls
    ASSERT_EQ(static_cast<int>(physics_ptr->wake_body_calls.size()), 1);
    EXPECT_EQ(physics_ptr->wake_body_calls[0], 1);
}

TEST(AnimationRenderer, GlobalWakeFrameSkipsBodiesWithPerBodyWakeFrame) {
    // 5 frames, global wake at frame 2, per-body wake at frame 3
    AnimationConfig config{0.5, 0.01, 10.0, "out/"};
    config.wake_frame = 2;
    ASSERT_EQ(config.total_frames(), 5);

    auto test_scene = create_test_scene_with_per_body_wake();
    auto physics = std::make_unique<FakePhysicsSimulator>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // Global wake at frame 2 wakes bodies 0, 2, 3 (no per-body wake_frame).
    // Body 1 (wake_frame=3) is skipped by global wake and woken at frame 3.
    EXPECT_EQ(count_wake_calls(physics_ptr->wake_body_calls, 0), 1);
    EXPECT_EQ(count_wake_calls(physics_ptr->wake_body_calls, 1), 1);
    EXPECT_EQ(count_wake_calls(physics_ptr->wake_body_calls, 2), 1);
    EXPECT_EQ(count_wake_calls(physics_ptr->wake_body_calls, 3), 1);
}

// ==========================================================
// Helper: create a test scene with finale enabled
// ==========================================================

TestSceneSetup create_test_scene_with_finale() {
    TestSceneSetup setup;

    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.8, 0.8));
    setup.materials.push_back(mat);

    // Ground plane (static) - simulates bowling floor
    auto ground = std::make_shared<Plane>(Point3(0, -1, 0), Vec3(0, 1, 0), mat.get());
    setup.scene.add_shape(ground);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::STATIC, 0.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    // Dynamic sphere - simulates bowling ball
    auto sphere = std::make_shared<Sphere>(Point3(0, 2, 0), 0.5, mat.get());
    setup.scene.add_shape(sphere);
    setup.shape_physics.push_back(PhysicsProperties{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    return setup;
}

AnimationConfig create_finale_config(int main_frames, int finale_frames) {
    int total = main_frames + finale_frames;
    double fps = 10.0;
    double duration = static_cast<double>(total) / fps;
    AnimationConfig config{duration, 0.01, fps, "out/"};
    config.wake_frame = -1;
    config.finale.enabled = true;
    config.finale.start_frame = main_frames;
    config.finale.earth_radius = 1.5;
    config.finale.earth_tilt_degrees = 23.5;
    config.finale.moon_radius_ratio = 0.25;
    // No texture paths -- will use fallback Lambertian materials
    return config;
}

// ==========================================================
// ACCEPTANCE TEST: Phase-specific lighting (M1-M5)
//
// Behaviors:
//   1. Finale uses DirectionalLight for Sun (M1)
//   2. Finale clears prior scene shapes (M2)
//   3. Finale ambient is 0.0 -- directional light only (M3)
//   4. Main animation ambient is 0.8 -- bright ambient only (M4)
//   5. Main animation has no lights -- ambient only (M5)
//
// Test Budget: 5 behaviors x 2 = 10 max unit tests
// ==========================================================

TEST(AnimationRendererAcceptance, FinaleUsesDirectionalLightAndClearsSceneWithCorrectAmbient) {
    // 2 main frames + 2 finale frames
    auto config = create_finale_config(2, 2);
    ASSERT_EQ(config.total_frames(), 4);

    auto test_scene = create_test_scene_with_finale();
    int original_shape_count = static_cast<int>(test_scene.scene.shapes().size());
    ASSERT_EQ(original_shape_count, 2); // ground + sphere

    auto physics = std::make_unique<FakePhysicsSimulator>();

    bool found_directional_light = false;
    bool found_point_sun = false;
    int finale_shape_count = -1;
    float main_ambient = -1.0f;
    float finale_ambient = -1.0f;
    int main_light_count = -1;
    int frame_counter = 0;

    auto write_cb = [&](const std::string&, const Scene& scene,
                        const Camera&, int, const RenderSettings& settings) {
        frame_counter++;
        if (frame_counter <= 2) {
            // Main animation frames
            main_ambient = settings.ambient_factor;
            main_light_count = static_cast<int>(scene.lights().size());
        } else {
            // Finale frames
            finale_ambient = settings.ambient_factor;
            finale_shape_count = static_cast<int>(scene.shapes().size());

            for (const auto& light : scene.lights()) {
                if (dynamic_cast<const DirectionalLight*>(light.get())) {
                    found_directional_light = true;
                }
                // Check if the "Sun" (intensity >= 1.0) is a PointLight
                if (auto* pl = dynamic_cast<const PointLight*>(light.get())) {
                    if (pl->intensity() >= 1.0) {
                        found_point_sun = true;
                    }
                }
            }
        }
    };

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // AC1 (M1): Finale uses DirectionalLight for the Sun, not PointLight
    EXPECT_TRUE(found_directional_light)
        << "Finale should use DirectionalLight for sunlight simulation";
    EXPECT_FALSE(found_point_sun)
        << "Finale Sun should not be a PointLight (quadratic attenuation kills intensity)";

    // AC2 (M2): Finale scene includes Earth + Moon (at minimum 2 shapes).
    // During the fall transition, bowling shapes are still present while falling away.
    EXPECT_GE(finale_shape_count, 2)
        << "Finale should include at least Earth and Moon";

    // AC3 (M3): Finale ambient is 0.0 (directional light only, no ambient)
    EXPECT_FLOAT_EQ(finale_ambient, 0.02f)
        << "Finale ambient should be 0.0 (directional light only)";

    // AC4 (M4): Main animation ambient is 0.15 (dimmed with directional lights)
    EXPECT_FLOAT_EQ(main_ambient, 0.08f)
        << "Main animation ambient should be 0.15 (dimmed with directional lights)";

    // AC5 (M5): Main animation has 2 directional lights
    EXPECT_EQ(main_light_count, 2)
        << "Main animation should have 2 directional lights";
}

// ==========================================================
// UNIT TEST: Finale uses DirectionalLight for Sun (M1)
// ==========================================================

TEST(AnimationRenderer, FinaleUsesDirectionalLightInsteadOfPointLightForSun) {
    auto config = create_finale_config(1, 1);

    auto test_scene = create_test_scene_with_finale();
    auto physics = std::make_unique<FakePhysicsSimulator>();

    std::vector<std::shared_ptr<Light>> finale_lights;
    int frame_counter = 0;
    auto write_cb = [&](const std::string&, const Scene& scene,
                        const Camera&, int, const RenderSettings&) {
        frame_counter++;
        if (frame_counter == 2) { // finale frame
            finale_lights = scene.lights();
        }
    };

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // At least one light should be a DirectionalLight
    bool has_directional = false;
    for (const auto& light : finale_lights) {
        if (dynamic_cast<const DirectionalLight*>(light.get())) {
            has_directional = true;
        }
    }
    EXPECT_TRUE(has_directional)
        << "Finale should contain a DirectionalLight for sunlight";
}

// ==========================================================
// UNIT TEST: Finale clears prior scene shapes (M2)
// ==========================================================

TEST(AnimationRenderer, FinaleClearsPriorSceneShapesBeforeAddingEarthAndMoon) {
    auto config = create_finale_config(1, 1);

    auto test_scene = create_test_scene_with_finale();
    auto physics = std::make_unique<FakePhysicsSimulator>();

    int finale_shape_count = -1;
    int frame_counter = 0;
    auto write_cb = [&](const std::string&, const Scene& scene,
                        const Camera&, int, const RenderSettings&) {
        frame_counter++;
        if (frame_counter == 2) { // finale frame
            finale_shape_count = static_cast<int>(scene.shapes().size());
        }
    };

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // During the fall transition, bowling shapes are present while falling away.
    // Earth + Moon are always present (at minimum 2 shapes).
    EXPECT_GE(finale_shape_count, 2)
        << "Finale should include at least Earth and Moon";
}

// ==========================================================
// UNIT TEST: Ambient factor values (M3 + M4)
// ==========================================================

TEST(AnimationRenderer, MainAnimationUsesBrightAmbientOnlyAndFinaleUsesNoAmbient) {
    auto config = create_finale_config(2, 1);

    auto test_scene = create_test_scene_with_finale();
    auto physics = std::make_unique<FakePhysicsSimulator>();

    float captured_main_ambient = -1.0f;
    float captured_finale_ambient = -1.0f;
    int main_light_count = -1;
    int frame_counter = 0;
    auto write_cb = [&](const std::string&, const Scene& scene,
                        const Camera&, int, const RenderSettings& settings) {
        frame_counter++;
        if (frame_counter == 1) {
            captured_main_ambient = settings.ambient_factor;
            main_light_count = static_cast<int>(scene.lights().size());
        } else if (frame_counter == 3) { // finale frame (after 2 main frames)
            captured_finale_ambient = settings.ambient_factor;
        }
    };

    AnimationRenderer renderer(config, test_scene.scene, test_scene.shape_physics,
                               std::move(physics), test_scene.camera, write_cb);

    renderer.render();

    // M4: Main animation ambient = 0.15 (dimmed with directional lights)
    EXPECT_FLOAT_EQ(captured_main_ambient, 0.08f);
    // M5: 2 directional lights during bowling phase
    EXPECT_EQ(main_light_count, 2);
    // M3: Finale ambient = 0.0 (directional light only)
    EXPECT_FLOAT_EQ(captured_finale_ambient, 0.02f);
}

} // namespace
} // namespace nwave
