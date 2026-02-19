#include <gtest/gtest.h>
#include "infrastructure/jolt_physics_simulator.h"

using namespace nwave;

class JoltPhysicsSimulatorTest : public ::testing::Test {
protected:
    PhysicsBodyDesc make_sphere(double radius, Point3 position, BodyType body_type,
                                double restitution = 0.3, double friction = 0.5) {
        PhysicsBodyDesc desc;
        desc.shape_type = PhysicsShapeType::SPHERE;
        desc.dimensions = Vec3(radius, radius, radius);
        desc.position = position;
        desc.properties.body_type = body_type;
        desc.properties.mass = 1.0;
        desc.properties.restitution = restitution;
        desc.properties.friction = friction;
        return desc;
    }

    PhysicsBodyDesc make_static_plane(double y_position) {
        PhysicsBodyDesc desc;
        desc.shape_type = PhysicsShapeType::PLANE;
        desc.position = Point3(0.0, y_position, 0.0);
        desc.properties.body_type = BodyType::STATIC;
        desc.properties.restitution = 1.0;
        desc.properties.friction = 0.5;
        return desc;
    }

    PhysicsBodyDesc make_box_from_min_max(Point3 min, Point3 max, BodyType body_type,
                                           double restitution = 0.3, double friction = 0.5) {
        PhysicsBodyDesc desc;
        desc.shape_type = PhysicsShapeType::BOX;
        desc.position = Point3(
            (min.x() + max.x()) * 0.5,
            (min.y() + max.y()) * 0.5,
            (min.z() + max.z()) * 0.5);
        desc.dimensions = Vec3(
            max.x() - min.x(),
            max.y() - min.y(),
            max.z() - min.z());
        desc.properties.body_type = body_type;
        desc.properties.mass = 1.0;
        desc.properties.restitution = restitution;
        desc.properties.friction = friction;
        return desc;
    }
};

TEST_F(JoltPhysicsSimulatorTest, sphere_falls_below_y1_after_60_steps) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));

    auto plane_id = sim.add_body(make_static_plane(0.0));
    auto sphere_id = sim.add_body(make_sphere(0.5, Point3(0.0, 5.0, 0.0), BodyType::DYNAMIC));

    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 60; ++i) {
        sim.step(dt);
    }

    auto transform = sim.get_transform(sphere_id);
    EXPECT_LT(transform.position.y(), 1.0)
        << "Sphere starting at y=5 should fall below y=1 after 60 steps at dt=1/60";
}

TEST_F(JoltPhysicsSimulatorTest, sphere_with_restitution_bounces_above_plane) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));

    auto plane_id = sim.add_body(make_static_plane(0.0));
    auto sphere_id = sim.add_body(make_sphere(0.5, Point3(0.0, 3.0, 0.0),
                                               BodyType::DYNAMIC, 0.6));

    const double dt = 1.0 / 60.0;
    bool hit_ground = false;
    double lowest_y = 3.0;
    bool bounced = false;

    for (int i = 0; i < 300; ++i) {
        sim.step(dt);
        auto transform = sim.get_transform(sphere_id);
        double y = transform.position.y();

        if (y < lowest_y) {
            lowest_y = y;
        }
        if (lowest_y < 1.0) {
            hit_ground = true;
        }
        if (hit_ground && y > lowest_y + 0.1) {
            bounced = true;
            break;
        }
    }

    EXPECT_TRUE(bounced)
        << "Sphere with restitution 0.6 should bounce above plane after contact";
}

TEST_F(JoltPhysicsSimulatorTest, static_plane_remains_at_y0_after_300_steps) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));

    auto plane_id = sim.add_body(make_static_plane(0.0));
    auto sphere_id = sim.add_body(make_sphere(0.5, Point3(0.0, 5.0, 0.0), BodyType::DYNAMIC));

    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 300; ++i) {
        sim.step(dt);
    }

    auto plane_transform = sim.get_transform(plane_id);
    EXPECT_NEAR(plane_transform.position.y(), 0.0, 0.001)
        << "Static plane should remain at y=0 after 300 steps";
}

TEST_F(JoltPhysicsSimulatorTest, new_simulator_has_no_state_from_previous_instance) {
    {
        JoltPhysicsSimulator sim1;
        sim1.set_gravity(Vec3(0.0, -9.81, 0.0));
        sim1.add_body(make_sphere(0.5, Point3(0.0, 10.0, 0.0), BodyType::DYNAMIC));
        sim1.step(1.0 / 60.0);
    }

    JoltPhysicsSimulator sim2;
    sim2.set_gravity(Vec3(0.0, -9.81, 0.0));
    auto sphere_id = sim2.add_body(make_sphere(0.5, Point3(0.0, 5.0, 0.0), BodyType::DYNAMIC));

    auto transform = sim2.get_transform(sphere_id);
    EXPECT_NEAR(transform.position.y(), 5.0, 0.1)
        << "New simulator instance should start fresh with no state from previous instance";
}

TEST_F(JoltPhysicsSimulatorTest, box_from_min_max_creates_body_at_center_with_correct_half_extents) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, 0.0, 0.0));

    // Box defined by min=[2,0,-0.5] max=[3,1.5,0.5]
    // center = [2.5, 0.75, 0], half-extents = [0.5, 0.75, 0.5]
    // dimensions (full extents) = [1.0, 1.5, 1.0]
    auto box_id = sim.add_body(make_box_from_min_max(
        Point3(2.0, 0.0, -0.5), Point3(3.0, 1.5, 0.5), BodyType::DYNAMIC));

    auto transform = sim.get_transform(box_id);
    EXPECT_NEAR(transform.position.x(), 2.5, 0.01)
        << "Box center x should be (2+3)/2 = 2.5";
    EXPECT_NEAR(transform.position.y(), 0.75, 0.01)
        << "Box center y should be (0+1.5)/2 = 0.75";
    EXPECT_NEAR(transform.position.z(), 0.0, 0.01)
        << "Box center z should be (-0.5+0.5)/2 = 0.0";
}

TEST_F(JoltPhysicsSimulatorTest, sphere_collision_pushes_box_from_original_position) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, 0.0, 0.0));

    // Dynamic box at center [5.0, 0.0, 0.0] with full extents [1.0, 1.0, 1.0]
    auto box_desc = make_box_from_min_max(
        Point3(4.5, -0.5, -0.5), Point3(5.5, 0.5, 0.5), BodyType::DYNAMIC, 0.8, 0.3);
    auto box_id = sim.add_body(box_desc);

    // Dynamic sphere at origin with velocity toward the box
    auto sphere_desc = make_sphere(0.5, Point3(0.0, 0.0, 0.0), BodyType::DYNAMIC, 0.8, 0.3);
    sphere_desc.properties.initial_velocity = Vec3(10.0, 0.0, 0.0);
    auto sphere_id = sim.add_body(sphere_desc);

    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 120; ++i) {
        sim.step(dt);
    }

    auto box_transform = sim.get_transform(box_id);
    EXPECT_GT(box_transform.position.x(), 5.5)
        << "Box should be pushed beyond its original position by sphere collision";
}

TEST_F(JoltPhysicsSimulatorTest, sphere_velocity_changes_after_box_collision) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, 0.0, 0.0));

    // Dynamic box at x=5 with full extents [1.0, 1.0, 1.0]
    auto box_desc = make_box_from_min_max(
        Point3(4.5, -0.5, -0.5), Point3(5.5, 0.5, 0.5), BodyType::DYNAMIC, 0.8, 0.3);
    auto box_id = sim.add_body(box_desc);

    // Sphere at origin moving toward box at x velocity = 10
    auto sphere_desc = make_sphere(0.5, Point3(0.0, 0.0, 0.0), BodyType::DYNAMIC, 0.8, 0.3);
    sphere_desc.properties.initial_velocity = Vec3(10.0, 0.0, 0.0);
    auto sphere_id = sim.add_body(sphere_desc);

    // Without collision, after 2 seconds sphere would be at x=20
    // With collision and energy transfer, sphere should travel less distance
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 120; ++i) {
        sim.step(dt);
    }

    auto sphere_transform = sim.get_transform(sphere_id);
    double free_travel_x = 10.0 * 2.0; // velocity * time = 20.0
    EXPECT_LT(sphere_transform.position.x(), free_travel_x)
        << "Sphere should travel less than free-travel distance due to energy transfer to box";
}

TEST_F(JoltPhysicsSimulatorTest, kinematic_body_moves_at_constant_velocity_unaffected_by_gravity) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));

    // Kinematic sphere at y=5 with initial_velocity [0, 0, -3.44]
    auto sphere_desc = make_sphere(0.5, Point3(0.0, 5.0, 0.0), BodyType::KINEMATIC);
    sphere_desc.properties.initial_velocity = Vec3(0.0, 0.0, -3.44);
    auto sphere_id = sim.add_body(sphere_desc);

    const double dt = 1.0 / 60.0;
    const int steps = 60;
    for (int i = 0; i < steps; ++i) {
        sim.step(dt);
    }

    auto transform = sim.get_transform(sphere_id);
    double elapsed = dt * steps; // 1 second
    double expected_z = -3.44 * elapsed; // -3.44

    EXPECT_NEAR(transform.position.z(), expected_z, 0.1)
        << "Kinematic body should move at constant velocity in Z";
    EXPECT_NEAR(transform.position.y(), 5.0, 0.01)
        << "Kinematic body should not be affected by gravity";
}

TEST_F(JoltPhysicsSimulatorTest, kinematic_body_with_zero_velocity_remains_stationary) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));

    // Kinematic sphere at y=5 with zero initial_velocity (default)
    auto sphere_desc = make_sphere(0.5, Point3(0.0, 5.0, 0.0), BodyType::KINEMATIC);
    auto sphere_id = sim.add_body(sphere_desc);

    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 60; ++i) {
        sim.step(dt);
    }

    auto transform = sim.get_transform(sphere_id);
    EXPECT_NEAR(transform.position.x(), 0.0, 0.01)
        << "Kinematic body with zero velocity should not move in X";
    EXPECT_NEAR(transform.position.y(), 5.0, 0.01)
        << "Kinematic body with zero velocity should not move in Y";
    EXPECT_NEAR(transform.position.z(), 0.0, 0.01)
        << "Kinematic body with zero velocity should not move in Z";
}

TEST_F(JoltPhysicsSimulatorTest, wake_body_activates_sleeping_kinematic_body) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));

    // Kinematic sphere with initial_velocity and start_asleep=true
    auto sphere_desc = make_sphere(0.5, Point3(0.0, 5.0, 0.0), BodyType::KINEMATIC);
    sphere_desc.properties.initial_velocity = Vec3(0.0, 0.0, -3.0);
    sphere_desc.properties.start_asleep = true;
    auto sphere_id = sim.add_body(sphere_desc);

    const double dt = 1.0 / 60.0;

    // Step 30 frames while asleep -- body should not move
    for (int i = 0; i < 30; ++i) {
        sim.step(dt);
    }
    auto before_wake = sim.get_transform(sphere_id);
    EXPECT_NEAR(before_wake.position.z(), 0.0, 0.01)
        << "Sleeping kinematic body should not move before wake_body";

    // Wake the body
    sim.wake_body(sphere_id);

    // Step 60 more frames -- body should now move at its stored velocity
    for (int i = 0; i < 60; ++i) {
        sim.step(dt);
    }
    auto after_wake = sim.get_transform(sphere_id);
    double expected_z = -3.0 * (60 * dt); // -3.0 after 1 second
    EXPECT_NEAR(after_wake.position.z(), expected_z, 0.2)
        << "Woken kinematic body should move at its stored initial_velocity";
    EXPECT_NEAR(after_wake.position.y(), 5.0, 0.01)
        << "Kinematic body should not be affected by gravity after wake";
}

TEST_F(JoltPhysicsSimulatorTest, wake_body_on_active_body_does_not_cause_errors) {
    JoltPhysicsSimulator sim;
    sim.set_gravity(Vec3(0.0, 0.0, 0.0));

    // Active kinematic sphere (start_asleep defaults to false)
    auto sphere_desc = make_sphere(0.5, Point3(0.0, 0.0, 0.0), BodyType::KINEMATIC);
    sphere_desc.properties.initial_velocity = Vec3(1.0, 0.0, 0.0);
    auto sphere_id = sim.add_body(sphere_desc);

    sim.step(1.0 / 60.0);

    // Calling wake_body on an already-active body should not throw
    EXPECT_NO_THROW(sim.wake_body(sphere_id));
}

TEST_F(JoltPhysicsSimulatorTest, wake_body_with_invalid_id_throws) {
    JoltPhysicsSimulator sim;

    EXPECT_THROW(sim.wake_body(99), std::runtime_error);
    EXPECT_THROW(sim.wake_body(-1), std::runtime_error);
}
