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
