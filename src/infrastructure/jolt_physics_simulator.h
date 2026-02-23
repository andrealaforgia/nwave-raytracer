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
    void wake_all() override;
    void wake_body(int body_id) override;
    void set_linear_velocity(int body_id, const Vec3& velocity) override;
    void set_angular_velocity(int body_id, const Vec3& angular_velocity) override;
    void set_motion_type(int body_id, BodyType type) override;

    int add_soft_body(const SoftBodyDesc& desc) override;
    bool is_soft_body(int body_id) const override;
    SoftBodyMeshData get_soft_body_mesh(int body_id) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_JOLT_PHYSICS_SIMULATOR_H
