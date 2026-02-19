// Spike test: Validate Jolt v5.2.0 XPBD soft body API availability.
// This test verifies that the soft body headers compile and the key types
// (SoftBodySharedSettings, SoftBodyCreationSettings, SoftBodyMotionProperties)
// can be instantiated with expected API patterns from Jolt's SoftBodyCreator sample.

#include <gtest/gtest.h>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>

JPH_SUPPRESS_WARNINGS

using namespace JPH;

class JoltSoftBodyApiSpike : public ::testing::Test {
protected:
    static inline bool s_owns_jolt = false;

    static void SetUpTestSuite() {
        JPH::RegisterDefaultAllocator();
        if (!JPH::Factory::sInstance) {
            JPH::Factory::sInstance = new JPH::Factory();
            s_owns_jolt = true;
        }
        JPH::RegisterTypes();
    }

    static void TearDownTestSuite() {
        if (s_owns_jolt) {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
            s_owns_jolt = false;
        }
    }
};

// ---------------------------------------------------------------------------
// Acceptance Criterion 1 & 2: SoftBodySharedSettings can be instantiated,
// vertices and faces added, and edge/volume constraints created.
// This mirrors the pattern from Jolt's SoftBodyCreator::CreateCube.
// ---------------------------------------------------------------------------

TEST_F(JoltSoftBodyApiSpike, SharedSettingsAcceptsVerticesFacesAndConstraints) {
    // Create a minimal 2x2x2 cube (8 vertices) following CreateCube pattern
    const uint grid_size = 2;
    const float spacing = 1.0f;
    const Vec3 offset = Vec3::sReplicate(-0.5f * spacing * (grid_size - 1));

    SoftBodySharedSettings *settings = new SoftBodySharedSettings;

    // Add vertices (3D grid)
    for (uint z = 0; z < grid_size; ++z)
        for (uint y = 0; y < grid_size; ++y)
            for (uint x = 0; x < grid_size; ++x)
            {
                SoftBodySharedSettings::Vertex v;
                (offset + Vec3::sReplicate(spacing) * Vec3(float(x), float(y), float(z)))
                    .StoreFloat3(&v.mPosition);
                v.mInvMass = 1.0f;
                settings->mVertices.push_back(v);
            }

    EXPECT_EQ(settings->mVertices.size(), 8u) << "2x2x2 grid should produce 8 vertices";

    // Vertex index helper (same as CreateCube)
    auto vertex_index = [grid_size](uint inX, uint inY, uint inZ) -> uint {
        return inX + inY * grid_size + inZ * grid_size * grid_size;
    };

    // Add edge constraints along each axis
    for (uint z = 0; z < grid_size; ++z)
        for (uint y = 0; y < grid_size; ++y)
            for (uint x = 0; x < grid_size; ++x)
            {
                SoftBodySharedSettings::Edge e;
                e.mVertex[0] = vertex_index(x, y, z);
                if (x < grid_size - 1)
                {
                    e.mVertex[1] = vertex_index(x + 1, y, z);
                    settings->mEdgeConstraints.push_back(e);
                }
                if (y < grid_size - 1)
                {
                    e.mVertex[1] = vertex_index(x, y + 1, z);
                    settings->mEdgeConstraints.push_back(e);
                }
                if (z < grid_size - 1)
                {
                    e.mVertex[1] = vertex_index(x, y, z + 1);
                    settings->mEdgeConstraints.push_back(e);
                }
            }

    settings->CalculateEdgeLengths();
    EXPECT_EQ(settings->mEdgeConstraints.size(), 12u)
        << "2x2x2 grid should have 12 edge constraints (4 per axis)";

    // Add volume constraints (tetrahedra) for one cube cell
    const int tetra_indices[6][4][3] = {
        { {0, 0, 0}, {0, 1, 1}, {0, 0, 1}, {1, 1, 1} },
        { {0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {1, 1, 1} },
        { {0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1} },
        { {0, 0, 0}, {1, 0, 1}, {1, 0, 0}, {1, 1, 1} },
        { {0, 0, 0}, {1, 1, 0}, {0, 1, 0}, {1, 1, 1} },
        { {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1} }
    };

    for (uint t = 0; t < 6; ++t)
    {
        SoftBodySharedSettings::Volume v;
        for (uint i = 0; i < 4; ++i)
            v.mVertex[i] = vertex_index(
                tetra_indices[t][i][0],
                tetra_indices[t][i][1],
                tetra_indices[t][i][2]);
        settings->mVolumeConstraints.push_back(v);
    }

    settings->CalculateVolumeConstraintVolumes();
    EXPECT_EQ(settings->mVolumeConstraints.size(), 6u)
        << "One cube cell decomposes into 6 tetrahedra";

    // Add faces (two triangles for one face of the cube)
    SoftBodySharedSettings::Face f;
    f.mVertex[0] = vertex_index(0, 0, 0);
    f.mVertex[1] = vertex_index(0, 1, 0);
    f.mVertex[2] = vertex_index(1, 1, 0);
    settings->AddFace(f);

    f.mVertex[0] = vertex_index(0, 0, 0);
    f.mVertex[1] = vertex_index(1, 1, 0);
    f.mVertex[2] = vertex_index(1, 0, 0);
    settings->AddFace(f);

    EXPECT_EQ(settings->mFaces.size(), 2u)
        << "Two triangles should be added for one quad face";

    // Optimize (reorders constraints for parallel execution)
    settings->Optimize();

    // Ref-counted pointer works
    Ref<SoftBodySharedSettings> ref_settings = settings;
    EXPECT_EQ(ref_settings->mVertices.size(), 8u)
        << "Ref<SoftBodySharedSettings> should retain the same data";
}

// ---------------------------------------------------------------------------
// Acceptance Criterion 3: SoftBodyCreationSettings can be created from
// SoftBodySharedSettings with position, rotation, and object layer.
// ---------------------------------------------------------------------------

TEST_F(JoltSoftBodyApiSpike, CreationSettingsCanBeConstructedFromSharedSettings) {
    // Build minimal shared settings with one vertex and one face
    Ref<SoftBodySharedSettings> shared = new SoftBodySharedSettings;

    SoftBodySharedSettings::Vertex v;
    Vec3(0.0f, 0.0f, 0.0f).StoreFloat3(&v.mPosition);
    shared->mVertices.push_back(v);

    Vec3(1.0f, 0.0f, 0.0f).StoreFloat3(&v.mPosition);
    shared->mVertices.push_back(v);

    Vec3(0.0f, 1.0f, 0.0f).StoreFloat3(&v.mPosition);
    shared->mVertices.push_back(v);

    SoftBodySharedSettings::Face f(0, 1, 2);
    shared->AddFace(f);

    shared->Optimize();

    // Create SoftBodyCreationSettings using the 4-arg constructor
    const ObjectLayer object_layer = 1;
    SoftBodyCreationSettings creation_settings(
        shared,
        RVec3(0.0, 5.0, 0.0),
        Quat::sIdentity(),
        object_layer);

    // Verify the creation settings fields are accessible
    EXPECT_EQ(creation_settings.mSettings, shared)
        << "Creation settings should reference the shared settings";
    EXPECT_FLOAT_EQ(float(creation_settings.mPosition.GetY()), 5.0f)
        << "Position Y should be 5.0";
    EXPECT_EQ(creation_settings.mObjectLayer, object_layer)
        << "Object layer should match";

    // Verify configurable simulation parameters are accessible
    creation_settings.mNumIterations = 10;
    creation_settings.mPressure = 2000.0f;
    creation_settings.mRestitution = 0.3f;
    creation_settings.mFriction = 0.5f;
    creation_settings.mLinearDamping = 0.05f;
    creation_settings.mGravityFactor = 1.0f;

    EXPECT_EQ(creation_settings.mNumIterations, 10u);
    EXPECT_FLOAT_EQ(creation_settings.mPressure, 2000.0f);
    EXPECT_FLOAT_EQ(creation_settings.mRestitution, 0.3f);
}

// ---------------------------------------------------------------------------
// Acceptance Criterion 2 (extended): CreateConstraints API works with
// VertexAttributes for automated edge/shear/bend constraint generation.
// This is the pattern used for cloth-like soft bodies.
// ---------------------------------------------------------------------------

TEST_F(JoltSoftBodyApiSpike, CreateConstraintsAutomaticallyGeneratesEdgesFromFaces) {
    Ref<SoftBodySharedSettings> settings = new SoftBodySharedSettings;

    // Create a small 3x3 grid of vertices (cloth-like)
    const uint grid_x = 3, grid_z = 3;
    const float spacing = 1.0f;
    for (uint z = 0; z < grid_z; ++z)
        for (uint x = 0; x < grid_x; ++x)
        {
            SoftBodySharedSettings::Vertex v;
            Vec3(float(x) * spacing, 0.0f, float(z) * spacing).StoreFloat3(&v.mPosition);
            settings->mVertices.push_back(v);
        }

    ASSERT_EQ(settings->mVertices.size(), 9u);

    // Create faces (two triangles per quad cell)
    auto vi = [grid_x](uint x, uint z) -> uint { return x + z * grid_x; };
    for (uint z = 0; z < grid_z - 1; ++z)
        for (uint x = 0; x < grid_x - 1; ++x)
        {
            SoftBodySharedSettings::Face f;
            f.mVertex[0] = vi(x, z);
            f.mVertex[1] = vi(x, z + 1);
            f.mVertex[2] = vi(x + 1, z + 1);
            settings->AddFace(f);

            f.mVertex[1] = vi(x + 1, z + 1);
            f.mVertex[2] = vi(x + 1, z);
            settings->AddFace(f);
        }

    EXPECT_EQ(settings->mFaces.size(), 8u)
        << "3x3 grid should have 4 quads x 2 triangles = 8 faces";

    // Use CreateConstraints to auto-generate edge constraints from faces
    SoftBodySharedSettings::VertexAttributes attributes(1.0e-5f, 1.0e-5f, 1.0e-5f);
    settings->CreateConstraints(&attributes, 1, SoftBodySharedSettings::EBendType::Distance);

    EXPECT_GT(settings->mEdgeConstraints.size(), 0u)
        << "CreateConstraints should generate edge constraints from faces";

    settings->Optimize();
}
