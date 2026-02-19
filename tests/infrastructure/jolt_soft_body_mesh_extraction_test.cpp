#include <gtest/gtest.h>
#include "infrastructure/jolt_physics_simulator.h"
#include "domain/soft_body_desc.h"
#include "domain/soft_body_mesh_data.h"

using namespace nwave;

class JoltSoftBodyMeshExtractionTest : public ::testing::Test {
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
};

// AC1: get_soft_body_mesh() returns vertices for a 5x5x5 cube (count > 0)
TEST_F(JoltSoftBodyMeshExtractionTest, ReturnsVerticesForFiveByFiveCube) {
    SoftBodyDesc desc = make_default_desc();
    int body_id = sim.add_soft_body(desc);

    SoftBodyMeshData mesh = sim.get_soft_body_mesh(body_id);

    EXPECT_EQ(mesh.vertices.size(), 125u)
        << "5x5x5 soft body should have 125 vertices";
}

// AC2: Vertices are in world space (transformed by body center of mass)
TEST_F(JoltSoftBodyMeshExtractionTest, VerticesAreInWorldSpace) {
    Point3 world_position(5.0, 20.0, -3.0);
    SoftBodyDesc desc = make_default_desc(world_position);
    int body_id = sim.add_soft_body(desc);

    SoftBodyMeshData mesh = sim.get_soft_body_mesh(body_id);
    ASSERT_FALSE(mesh.vertices.empty());

    // Compute centroid of all vertices
    double cx = 0.0, cy = 0.0, cz = 0.0;
    for (const auto& v : mesh.vertices) {
        cx += v.x();
        cy += v.y();
        cz += v.z();
    }
    cx /= static_cast<double>(mesh.vertices.size());
    cy /= static_cast<double>(mesh.vertices.size());
    cz /= static_cast<double>(mesh.vertices.size());

    // Centroid should be near the body's world position
    EXPECT_NEAR(cx, world_position.x(), 2.0)
        << "Average vertex x should be near world position x";
    EXPECT_NEAR(cy, world_position.y(), 2.0)
        << "Average vertex y should be near world position y";
    EXPECT_NEAR(cz, world_position.z(), 2.0)
        << "Average vertex z should be near world position z";
}

// AC3: Face indices form valid triangles (3 indices per face, all in vertex range)
TEST_F(JoltSoftBodyMeshExtractionTest, FaceIndicesFormValidTriangles) {
    SoftBodyDesc desc = make_default_desc();
    int body_id = sim.add_soft_body(desc);

    SoftBodyMeshData mesh = sim.get_soft_body_mesh(body_id);

    // Face indices must be a multiple of 3 (triangle faces)
    ASSERT_GT(mesh.face_indices.size(), 0u)
        << "Should have face indices";
    EXPECT_EQ(mesh.face_indices.size() % 3, 0u)
        << "Face indices count must be a multiple of 3";

    // All indices must be within vertex range
    int vertex_count = static_cast<int>(mesh.vertices.size());
    for (size_t i = 0; i < mesh.face_indices.size(); ++i) {
        EXPECT_GE(mesh.face_indices[i], 0)
            << "Face index " << i << " should be non-negative";
        EXPECT_LT(mesh.face_indices[i], vertex_count)
            << "Face index " << i << " should be less than vertex count " << vertex_count;
    }
}

// AC4: After 10 physics steps with gravity, vertex positions differ from initial
TEST_F(JoltSoftBodyMeshExtractionTest, VerticesChangeAfterPhysicsSteps) {
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));
    SoftBodyDesc desc = make_default_desc(Point3(0.0, 10.0, 0.0));
    int body_id = sim.add_soft_body(desc);

    SoftBodyMeshData initial_mesh = sim.get_soft_body_mesh(body_id);
    ASSERT_FALSE(initial_mesh.vertices.empty());

    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 10; ++i) {
        sim.step(dt);
    }

    SoftBodyMeshData after_mesh = sim.get_soft_body_mesh(body_id);
    ASSERT_EQ(after_mesh.vertices.size(), initial_mesh.vertices.size());

    // At least some vertices should have moved
    bool any_moved = false;
    for (size_t i = 0; i < initial_mesh.vertices.size(); ++i) {
        double dx = after_mesh.vertices[i].x() - initial_mesh.vertices[i].x();
        double dy = after_mesh.vertices[i].y() - initial_mesh.vertices[i].y();
        double dz = after_mesh.vertices[i].z() - initial_mesh.vertices[i].z();
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > 0.001) {
            any_moved = true;
            break;
        }
    }
    EXPECT_TRUE(any_moved)
        << "After 10 physics steps with gravity, at least some vertices should have moved";
}

// AC5: Face indices are consistent across physics steps (topology doesn't change)
TEST_F(JoltSoftBodyMeshExtractionTest, FaceIndicesConsistentAcrossPhysicsSteps) {
    sim.set_gravity(Vec3(0.0, -9.81, 0.0));
    SoftBodyDesc desc = make_default_desc(Point3(0.0, 10.0, 0.0));
    int body_id = sim.add_soft_body(desc);

    SoftBodyMeshData initial_mesh = sim.get_soft_body_mesh(body_id);

    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 10; ++i) {
        sim.step(dt);
    }

    SoftBodyMeshData after_mesh = sim.get_soft_body_mesh(body_id);

    ASSERT_EQ(after_mesh.face_indices.size(), initial_mesh.face_indices.size())
        << "Face index count should not change across physics steps";
    for (size_t i = 0; i < initial_mesh.face_indices.size(); ++i) {
        EXPECT_EQ(after_mesh.face_indices[i], initial_mesh.face_indices[i])
            << "Face index " << i << " should be consistent across physics steps";
    }
}

// Error handling: throws for invalid body ID
TEST_F(JoltSoftBodyMeshExtractionTest, ThrowsForInvalidBodyId) {
    EXPECT_THROW(sim.get_soft_body_mesh(-1), std::runtime_error);
    EXPECT_THROW(sim.get_soft_body_mesh(999), std::runtime_error);
}

// Error handling: throws for rigid body (not a soft body)
TEST_F(JoltSoftBodyMeshExtractionTest, ThrowsForRigidBody) {
    PhysicsBodyDesc rigid_desc;
    rigid_desc.shape_type = PhysicsShapeType::SPHERE;
    rigid_desc.dimensions = Vec3(0.5, 0.5, 0.5);
    rigid_desc.position = Point3(0.0, 5.0, 0.0);
    rigid_desc.properties.body_type = BodyType::DYNAMIC;
    int rigid_id = sim.add_body(rigid_desc);

    EXPECT_THROW(sim.get_soft_body_mesh(rigid_id), std::runtime_error);
}
