#include <gtest/gtest.h>
#include "domain/soft_body_desc.h"
#include "domain/soft_body_mesh_data.h"
#include "domain/physics_properties.h"

using namespace nwave;

TEST(SoftBodyDescTest, DefaultConstructorInitializesExpectedValues) {
    SoftBodyDesc desc;
    EXPECT_EQ(desc.grid_resolution, 5);
    EXPECT_DOUBLE_EQ(desc.grid_spacing, 0.5);
    EXPECT_DOUBLE_EQ(desc.position.x(), 0.0);
    EXPECT_DOUBLE_EQ(desc.position.y(), 0.0);
    EXPECT_DOUBLE_EQ(desc.position.z(), 0.0);
    EXPECT_DOUBLE_EQ(desc.pressure, 2000.0);
    EXPECT_DOUBLE_EQ(desc.restitution, 0.3);
    EXPECT_DOUBLE_EQ(desc.friction, 0.2);
    EXPECT_DOUBLE_EQ(desc.damping, 0.1);
    EXPECT_EQ(desc.solver_iterations, 5);
    EXPECT_DOUBLE_EQ(desc.edge_compliance, 1e-4);
    EXPECT_DOUBLE_EQ(desc.volume_compliance, 0.0);
}

TEST(SoftBodyMeshDataTest, HoldsVerticesAndFaceIndices) {
    SoftBodyMeshData mesh;
    mesh.vertices.push_back(Point3(1.0, 2.0, 3.0));
    mesh.vertices.push_back(Point3(4.0, 5.0, 6.0));
    mesh.vertices.push_back(Point3(7.0, 8.0, 9.0));
    mesh.face_indices = {0, 1, 2};

    EXPECT_EQ(mesh.vertices.size(), 3u);
    EXPECT_DOUBLE_EQ(mesh.vertices[0].x(), 1.0);
    EXPECT_DOUBLE_EQ(mesh.vertices[1].y(), 5.0);
    EXPECT_DOUBLE_EQ(mesh.vertices[2].z(), 9.0);
    EXPECT_EQ(mesh.face_indices.size(), 3u);
    EXPECT_EQ(mesh.face_indices[0], 0);
    EXPECT_EQ(mesh.face_indices[2], 2);
}

TEST(BodyTypeTest, SoftEnumValueExists) {
    BodyType soft = BodyType::SOFT;
    EXPECT_NE(static_cast<int>(soft), static_cast<int>(BodyType::STATIC));
    EXPECT_NE(static_cast<int>(soft), static_cast<int>(BodyType::DYNAMIC));
    EXPECT_NE(static_cast<int>(soft), static_cast<int>(BodyType::KINEMATIC));
}
