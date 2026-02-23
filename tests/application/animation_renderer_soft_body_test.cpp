#include <gtest/gtest.h>
#include "application/animation_renderer.h"
#include "application/physics_simulator.h"
#include "domain/animation_config.h"
#include "domain/scene.h"
#include "domain/camera.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/deformable_mesh.h"
#include "domain/materials/lambertian.h"
#include "domain/physics_properties.h"
#include "domain/soft_body_desc.h"
#include "domain/soft_body_mesh_data.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace nwave {
namespace {

// --- Fake PhysicsSimulator with soft body support ---

class SoftBodyFakePhysics : public PhysicsSimulator {
public:
    struct AddSoftBodyCall {
        SoftBodyDesc desc;
    };

    // Rigid body support (minimal, for mixed-scene tests)
    int add_body(const PhysicsBodyDesc& desc) override {
        int id = next_id_++;
        rigid_ids_.insert(id);
        transforms_[id] = PhysicsTransform{desc.position, desc.rotation};
        return id;
    }

    void step(double dt) override {
        step_count_++;
        // Simulate soft body deformation: shift vertices slightly each step
        for (auto& [id, mesh] : soft_meshes_) {
            for (auto& v : mesh.vertices) {
                v = Point3(v.x(), v.y() - 0.01, v.z());
            }
        }
        // Also shift rigid body transforms
        for (auto& [id, t] : transforms_) {
            t.position = Point3(t.position.x(), t.position.y() - 0.01, t.position.z());
        }
    }

    PhysicsTransform get_transform(int body_id) const override {
        auto it = transforms_.find(body_id);
        if (it != transforms_.end()) return it->second;
        return PhysicsTransform{};
    }

    void set_gravity(const Vec3&) override {}
    void wake_all() override {}
    void wake_body(int /*body_id*/) override {}
    void set_linear_velocity(int /*body_id*/, const Vec3& /*velocity*/) override {}
    void set_angular_velocity(int /*body_id*/, const Vec3& /*angular_velocity*/) override {}
    void set_motion_type(int /*body_id*/, BodyType /*type*/) override {}

    // Soft body support
    int add_soft_body(const SoftBodyDesc& desc) override {
        add_soft_body_calls.push_back({desc});
        int id = next_id_++;
        soft_ids_.insert(id);

        // Create a simple triangle mesh at the desc position
        SoftBodyMeshData mesh;
        double offset_x = desc.position.x();
        double offset_y = desc.position.y();
        double offset_z = desc.position.z();
        mesh.vertices = {
            Point3(offset_x + 0.0, offset_y + 0.0, offset_z + 0.0),
            Point3(offset_x + 1.0, offset_y + 0.0, offset_z + 0.0),
            Point3(offset_x + 0.0, offset_y + 1.0, offset_z + 0.0),
        };
        mesh.face_indices = {0, 1, 2};
        soft_meshes_[id] = mesh;

        return id;
    }

    bool is_soft_body(int body_id) const override {
        return soft_ids_.count(body_id) > 0;
    }

    SoftBodyMeshData get_soft_body_mesh(int body_id) const override {
        get_mesh_calls.push_back(body_id);
        auto it = soft_meshes_.find(body_id);
        if (it != soft_meshes_.end()) return it->second;
        return SoftBodyMeshData{};
    }

    // Recorded calls for verification
    std::vector<AddSoftBodyCall> add_soft_body_calls;
    mutable std::vector<int> get_mesh_calls;
    int step_count_ = 0;

    // Access to internal state for assertions
    const SoftBodyMeshData& current_mesh(int id) const { return soft_meshes_.at(id); }

private:
    int next_id_ = 0;
    std::set<int> rigid_ids_;
    std::set<int> soft_ids_;
    std::map<int, PhysicsTransform> transforms_;
    std::map<int, SoftBodyMeshData> soft_meshes_;
};

// --- Helpers ---

Camera make_test_camera() {
    return Camera(Point3(0, 0, 5), Point3(0, 0, 0), Vec3(0, 1, 0), 60.0, 1.0, 100);
}

AnimationConfig make_config(int total_frames, int steps_per_frame = 1) {
    double render_fps = 10.0;
    double duration = total_frames / render_fps;
    double physics_dt = (1.0 / render_fps) / steps_per_frame;
    return AnimationConfig{duration, physics_dt, render_fps, "out/"};
}

// ==========================================================
// ACCEPTANCE TEST: Soft body integration in animation loop
// ==========================================================

TEST(AnimationRendererSoftBodyAcceptance, RendersSoftBodyAlongsideRigidBodies) {
    // Mixed scene: 1 static rigid plane + 1 soft body, 3 frames
    AnimationConfig config = make_config(3, /*steps_per_frame=*/2);

    // Rigid body scene (ground plane)
    auto mat = std::make_shared<Lambertian>(Vec3(0.5, 0.5, 0.5));
    Scene scene;
    auto ground = std::make_shared<Plane>(Point3(0, -2, 0), Vec3(0, 1, 0), mat.get());
    scene.add_shape(ground);
    std::vector<PhysicsProperties> shape_physics;
    shape_physics.push_back(PhysicsProperties{BodyType::STATIC, 0.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    // Soft body
    auto soft_mat = std::make_shared<Lambertian>(Vec3(0.8, 0.2, 0.2));
    SoftBodyDesc sb_desc;
    sb_desc.position = Point3(0, 2, 0);
    sb_desc.material = soft_mat.get();
    std::vector<SoftBodyDesc> soft_bodies = {sb_desc};

    auto physics = std::make_unique<SoftBodyFakePhysics>();
    auto* physics_ptr = physics.get();

    // Track rendered scenes - capture shape count each frame
    std::vector<int> shapes_per_frame;
    auto write_cb = [&](const std::string& filename, const Scene& s,
                        const Camera&, int, const RenderSettings&) {
        shapes_per_frame.push_back(static_cast<int>(s.shapes().size()));
    };

    AnimationRenderer renderer(config, scene, shape_physics,
                               std::move(physics), make_test_camera(), write_cb,
                               nullptr, soft_bodies);

    int frame_count = renderer.render();

    // AC1: Constructor accepts soft body descs - verified by compilation
    // AC2: Soft bodies registered with physics simulator
    EXPECT_EQ(static_cast<int>(physics_ptr->add_soft_body_calls.size()), 1);
    EXPECT_DOUBLE_EQ(physics_ptr->add_soft_body_calls[0].desc.position.y(), 2.0);

    // AC3: DeformableMesh vertices updated each frame (get_soft_body_mesh called per frame)
    // Frame 0 gets initial mesh + 3 frames query = at least 3 get_mesh calls
    // (1 initial in setup + 3 per-frame updates = 4, or just 3 per-frame if initial
    //  is from the same call. Implementation will clarify.)
    EXPECT_GE(static_cast<int>(physics_ptr->get_mesh_calls.size()), 3);

    // AC4: Rigid body rendering unaffected - we still get 3 frames rendered
    EXPECT_EQ(frame_count, 3);

    // AC5: Mixed scene renders all objects - each frame has ground + soft mesh = 2 shapes
    ASSERT_EQ(static_cast<int>(shapes_per_frame.size()), 3);
    for (int count : shapes_per_frame) {
        EXPECT_EQ(count, 2);  // 1 rigid + 1 soft
    }

    // AC6: Physics stepped AFTER rendering - step_count should be 6 (3 frames * 2 steps)
    EXPECT_EQ(physics_ptr->step_count_, 6);
}

// ==========================================================
// UNIT TESTS
// ==========================================================
// Test Budget: 4 behaviors x 2 = 8 max. Using 4 tests.

// Behavior 1: Soft bodies registered with physics simulator during setup
TEST(AnimationRendererSoftBody, RegistersSoftBodiesWithPhysicsSimulator) {
    AnimationConfig config = make_config(1);

    Scene scene;
    std::vector<PhysicsProperties> shape_physics;

    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.2, 0.2));
    SoftBodyDesc sb1;
    sb1.position = Point3(0, 3, 0);
    sb1.material = mat.get();
    SoftBodyDesc sb2;
    sb2.position = Point3(2, 3, 0);
    sb2.grid_resolution = 3;
    sb2.material = mat.get();
    std::vector<SoftBodyDesc> soft_bodies = {sb1, sb2};

    auto physics = std::make_unique<SoftBodyFakePhysics>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

    AnimationRenderer renderer(config, scene, shape_physics,
                               std::move(physics), make_test_camera(), write_cb,
                               nullptr, soft_bodies);

    renderer.render();

    // Both soft bodies registered
    ASSERT_EQ(static_cast<int>(physics_ptr->add_soft_body_calls.size()), 2);
    EXPECT_DOUBLE_EQ(physics_ptr->add_soft_body_calls[0].desc.position.y(), 3.0);
    EXPECT_DOUBLE_EQ(physics_ptr->add_soft_body_calls[1].desc.position.x(), 2.0);
    EXPECT_EQ(physics_ptr->add_soft_body_calls[1].desc.grid_resolution, 3);
}

// Behavior 2: Soft body mesh extracted and updated each frame
TEST(AnimationRendererSoftBody, UpdatesSoftBodyMeshVerticesEachFrame) {
    AnimationConfig config = make_config(3, /*steps_per_frame=*/1);

    Scene scene;
    std::vector<PhysicsProperties> shape_physics;

    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.2, 0.2));
    SoftBodyDesc sb_desc;
    sb_desc.material = mat.get();
    std::vector<SoftBodyDesc> soft_bodies = {sb_desc};

    auto physics = std::make_unique<SoftBodyFakePhysics>();
    auto* physics_ptr = physics.get();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {};

    AnimationRenderer renderer(config, scene, shape_physics,
                               std::move(physics), make_test_camera(), write_cb,
                               nullptr, soft_bodies);

    renderer.render();

    // get_soft_body_mesh should be called at least once per frame (3 frames)
    // plus once for initial setup = at least 4 total, or 3 per-frame
    // The soft body ID would be 0 (first ID allocated)
    int mesh_queries = 0;
    for (int id : physics_ptr->get_mesh_calls) {
        if (id == 0) mesh_queries++;
    }
    EXPECT_GE(mesh_queries, 3);  // At least one query per frame
}

// Behavior 3: Frame 0 renders soft body at initial position (before physics step)
TEST(AnimationRendererSoftBody, Frame0RendersSoftBodyBeforePhysicsStep) {
    AnimationConfig config = make_config(2, /*steps_per_frame=*/1);

    Scene scene;
    std::vector<PhysicsProperties> shape_physics;

    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.2, 0.2));
    SoftBodyDesc sb_desc;
    sb_desc.position = Point3(0, 5, 0);
    sb_desc.material = mat.get();
    std::vector<SoftBodyDesc> soft_bodies = {sb_desc};

    auto physics = std::make_unique<SoftBodyFakePhysics>();
    auto* physics_ptr = physics.get();

    // Track step count at each render call
    std::vector<int> steps_at_render;
    auto write_cb = [&](const std::string&, const Scene&, const Camera&, int, const RenderSettings&) {
        steps_at_render.push_back(physics_ptr->step_count_);
    };

    AnimationRenderer renderer(config, scene, shape_physics,
                               std::move(physics), make_test_camera(), write_cb,
                               nullptr, soft_bodies);

    renderer.render();

    // Frame 0 rendered BEFORE any physics step
    ASSERT_GE(static_cast<int>(steps_at_render.size()), 2);
    EXPECT_EQ(steps_at_render[0], 0);  // No steps before frame 0 render
    EXPECT_EQ(steps_at_render[1], 1);  // 1 step before frame 1 render
}

// Behavior 4: Empty soft body list leaves rigid path unchanged
TEST(AnimationRendererSoftBody, EmptySoftBodyListLeavesRigidPathUnchanged) {
    AnimationConfig config = make_config(2);

    auto mat = std::make_shared<Lambertian>(Vec3(0.5, 0.5, 0.5));
    Scene scene;
    auto sphere = std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, mat.get());
    scene.add_shape(sphere);
    std::vector<PhysicsProperties> shape_physics;
    shape_physics.push_back(PhysicsProperties{BodyType::DYNAMIC, 1.0, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.5, 0.3});

    auto physics = std::make_unique<SoftBodyFakePhysics>();
    auto* physics_ptr = physics.get();

    std::vector<std::string> filenames;
    auto write_cb = [&](const std::string& filename, const Scene& s,
                        const Camera&, int, const RenderSettings&) {
        filenames.push_back(filename);
    };

    // No soft bodies - default empty vector
    AnimationRenderer renderer(config, scene, shape_physics,
                               std::move(physics), make_test_camera(), write_cb);

    int frame_count = renderer.render();

    EXPECT_EQ(frame_count, 2);
    EXPECT_EQ(static_cast<int>(filenames.size()), 2);
    // No soft body calls
    EXPECT_EQ(static_cast<int>(physics_ptr->add_soft_body_calls.size()), 0);
    EXPECT_EQ(static_cast<int>(physics_ptr->get_mesh_calls.size()), 0);
}

} // namespace
} // namespace nwave
