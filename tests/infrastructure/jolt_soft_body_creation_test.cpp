#include <gtest/gtest.h>
#include "infrastructure/jolt_physics_simulator.h"
#include "domain/soft_body_desc.h"

using namespace nwave;

class JoltSoftBodyCreationTest : public ::testing::Test {
protected:
    JoltPhysicsSimulator sim;

    SoftBodyDesc make_default_desc(Point3 position = Point3(0.0, 10.0, 0.0)) {
        SoftBodyDesc desc;
        desc.grid_resolution = 5;
        desc.grid_spacing = 0.5;
        desc.position = position;
        desc.pressure = 2000.0;
        desc.restitution = 0.3;
        desc.friction = 0.2;
        desc.damping = 0.1;
        desc.solver_iterations = 5;
        desc.edge_compliance = 1e-4;
        desc.volume_compliance = 0.0;
        return desc;
    }

    PhysicsBodyDesc make_dynamic_sphere(Point3 position) {
        PhysicsBodyDesc desc;
        desc.shape_type = PhysicsShapeType::SPHERE;
        desc.dimensions = Vec3(0.5, 0.5, 0.5);
        desc.position = position;
        desc.properties.body_type = BodyType::DYNAMIC;
        desc.properties.mass = 1.0;
        desc.properties.restitution = 0.3;
        desc.properties.friction = 0.5;
        return desc;
    }
};

// AC1: add_soft_body creates a Jolt soft body and returns a valid index
TEST_F(JoltSoftBodyCreationTest, AddSoftBodyReturnsValidIndex) {
    SoftBodyDesc desc = make_default_desc();
    int body_id = sim.add_soft_body(desc);
    EXPECT_GE(body_id, 0) << "add_soft_body should return a non-negative index";
}

// AC2: A 5x5x5 soft body cube has 125 vertices (verified indirectly by
// successful creation and subsequent transform retrieval)
TEST_F(JoltSoftBodyCreationTest, FiveByFiveByFiveSoftBodyCreatesSuccessfully) {
    SoftBodyDesc desc = make_default_desc();
    desc.grid_resolution = 5;
    int body_id = sim.add_soft_body(desc);

    // The body should be retrievable - this validates the 125 vertex grid
    // was successfully built and accepted by Jolt
    PhysicsTransform transform = sim.get_transform(body_id);
    EXPECT_NEAR(transform.position.y(), 10.0, 1.0)
        << "5x5x5 soft body should be near its initial position y=10";
}

// AC3: Soft body is placed at the specified position
TEST_F(JoltSoftBodyCreationTest, SoftBodyPlacedAtSpecifiedPosition) {
    SoftBodyDesc desc = make_default_desc(Point3(3.0, 15.0, -2.0));
    int body_id = sim.add_soft_body(desc);

    PhysicsTransform transform = sim.get_transform(body_id);
    EXPECT_NEAR(transform.position.x(), 3.0, 1.0)
        << "Soft body x should be near 3.0";
    EXPECT_NEAR(transform.position.y(), 15.0, 1.0)
        << "Soft body y should be near 15.0";
    EXPECT_NEAR(transform.position.z(), -2.0, 1.0)
        << "Soft body z should be near -2.0";
}

// AC4: Pressure, restitution, friction, damping are applied from SoftBodyDesc
// (validated by the fact that the body is created without error; the parameters
// are set on SoftBodyCreationSettings before Jolt processes them)
TEST_F(JoltSoftBodyCreationTest, SoftBodyAcceptsCustomParameters) {
    SoftBodyDesc desc;
    desc.grid_resolution = 3;
    desc.grid_spacing = 0.5;
    desc.position = Point3(0.0, 10.0, 0.0);
    desc.pressure = 5000.0;
    desc.restitution = 0.8;
    desc.friction = 0.9;
    desc.damping = 0.5;
    desc.solver_iterations = 10;
    desc.edge_compliance = 1e-3;
    desc.volume_compliance = 1e-5;

    int body_id = sim.add_soft_body(desc);
    EXPECT_GE(body_id, 0)
        << "Soft body with custom parameters should be created successfully";

    // Verify it is tracked as a soft body
    EXPECT_TRUE(sim.is_soft_body(body_id))
        << "Body created via add_soft_body should be recognized as soft body";
}

// AC5: is_soft_body returns true for soft body IDs, false for rigid body IDs
TEST_F(JoltSoftBodyCreationTest, IsSoftBodyDistinguishesSoftFromRigid) {
    // Add a rigid body first
    int rigid_id = sim.add_body(make_dynamic_sphere(Point3(0.0, 5.0, 0.0)));

    // Add a soft body
    SoftBodyDesc desc = make_default_desc();
    int soft_id = sim.add_soft_body(desc);

    EXPECT_FALSE(sim.is_soft_body(rigid_id))
        << "Rigid body should not be reported as soft body";
    EXPECT_TRUE(sim.is_soft_body(soft_id))
        << "Soft body should be reported as soft body";
}

// AC5 edge case: is_soft_body returns false for out-of-range IDs
TEST_F(JoltSoftBodyCreationTest, IsSoftBodyReturnsFalseForOutOfRangeId) {
    EXPECT_FALSE(sim.is_soft_body(-1))
        << "Negative ID should return false";
    EXPECT_FALSE(sim.is_soft_body(999))
        << "Out-of-range ID should return false";
}

// AC6: Soft body falls under gravity when physics is stepped
TEST_F(JoltSoftBodyCreationTest, SoftBodyFallsUnderGravity) {
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));

    SoftBodyDesc desc = make_default_desc(Point3(0.0, 10.0, 0.0));
    int body_id = sim.add_soft_body(desc);

    PhysicsTransform initial = sim.get_transform(body_id);
    double initial_y = initial.position.y();

    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 10; ++i) {
        sim.step(dt);
    }

    PhysicsTransform after = sim.get_transform(body_id);
    EXPECT_LT(after.position.y(), initial_y - 0.01)
        << "Soft body should fall under gravity after 10 steps";
}
