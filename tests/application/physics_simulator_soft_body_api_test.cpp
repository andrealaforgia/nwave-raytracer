#include <gtest/gtest.h>
#include <stdexcept>
#include "infrastructure/jolt_physics_simulator.h"
#include "domain/soft_body_desc.h"
#include "domain/soft_body_mesh_data.h"

using namespace nwave;

class PhysicsSimulatorSoftBodyApiTest : public ::testing::Test {
protected:
    JoltPhysicsSimulator sim;
};

TEST_F(PhysicsSimulatorSoftBodyApiTest, AddSoftBodyReturnsValidIndex) {
    SoftBodyDesc desc;
    int id = sim.add_soft_body(desc);
    EXPECT_GE(id, 0);
}

TEST_F(PhysicsSimulatorSoftBodyApiTest, IsSoftBodyReturnsFalseForNonexistentId) {
    EXPECT_FALSE(sim.is_soft_body(0));
}

TEST_F(PhysicsSimulatorSoftBodyApiTest, GetSoftBodyMeshThrowsForInvalidId) {
    EXPECT_THROW(sim.get_soft_body_mesh(999), std::runtime_error);
}
