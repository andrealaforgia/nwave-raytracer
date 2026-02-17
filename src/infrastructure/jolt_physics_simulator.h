#ifndef NWAVE_INFRASTRUCTURE_JOLT_PHYSICS_SIMULATOR_H
#define NWAVE_INFRASTRUCTURE_JOLT_PHYSICS_SIMULATOR_H

#include "application/physics_simulator.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include <memory>
#include <vector>

namespace nwave {

class JoltPhysicsSimulator : public PhysicsSimulator {
public:
    JoltPhysicsSimulator();
    ~JoltPhysicsSimulator() override;

    JoltPhysicsSimulator(const JoltPhysicsSimulator&) = delete;
    JoltPhysicsSimulator& operator=(const JoltPhysicsSimulator&) = delete;

    int add_body(const PhysicsBodyDesc& desc) override;
    void step(double dt) override;
    PhysicsTransform get_transform(int body_id) const override;
    void set_gravity(const Vec3& gravity) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_JOLT_PHYSICS_SIMULATOR_H
