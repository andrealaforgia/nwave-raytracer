#include "infrastructure/jolt_physics_simulator.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <mutex>
#include <stdexcept>

JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;

namespace {

constexpr JPH::ObjectLayer LAYER_STATIC = 0;
constexpr JPH::ObjectLayer LAYER_DYNAMIC = 1;
constexpr uint NUM_OBJECT_LAYERS = 2;

constexpr JPH::BroadPhaseLayer BP_LAYER_STATIC(0);
constexpr JPH::BroadPhaseLayer BP_LAYER_DYNAMIC(1);
constexpr uint NUM_BP_LAYERS = 2;

class BPLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterface() {
        object_to_broad_phase_[LAYER_STATIC] = BP_LAYER_STATIC;
        object_to_broad_phase_[LAYER_DYNAMIC] = BP_LAYER_DYNAMIC;
    }

    uint GetNumBroadPhaseLayers() const override {
        return NUM_BP_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return object_to_broad_phase_[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
        case (JPH::BroadPhaseLayer::Type)BP_LAYER_STATIC: return "STATIC";
        case (JPH::BroadPhaseLayer::Type)BP_LAYER_DYNAMIC: return "DYNAMIC";
        default: return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer object_to_broad_phase_[NUM_OBJECT_LAYERS];
};

class ObjectVsBPLayerFilter : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        if (inLayer1 == LAYER_STATIC)
            return inLayer2 == BP_LAYER_DYNAMIC;
        return true;
    }
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        if (inLayer1 == LAYER_STATIC)
            return inLayer2 == LAYER_DYNAMIC;
        return true;
    }
};

std::mutex g_jolt_init_mutex;
int g_jolt_ref_count = 0;

void ensure_jolt_initialized() {
    std::lock_guard<std::mutex> lock(g_jolt_init_mutex);
    if (g_jolt_ref_count == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
    ++g_jolt_ref_count;
}

void release_jolt() {
    std::lock_guard<std::mutex> lock(g_jolt_init_mutex);
    --g_jolt_ref_count;
    if (g_jolt_ref_count == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

} // anonymous namespace

namespace nwave {

struct JoltPhysicsSimulator::Impl {
    BPLayerInterface bp_layer_interface;
    ObjectVsBPLayerFilter object_vs_bp_filter;
    ObjectLayerPairFilterImpl object_pair_filter;

    JPH::PhysicsSystem physics_system;
    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> job_system;

    std::vector<JPH::BodyID> body_ids;

    static constexpr int temp_allocator_size = 10 * 1024 * 1024; // 10 MB
    static constexpr int physics_thread_count = 1;
    static constexpr uint max_bodies = 1024;
    static constexpr uint num_body_mutexes = 0;
    static constexpr uint max_body_pairs = 1024;
    static constexpr uint max_contact_constraints = 1024;

    Impl() {
        temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(temp_allocator_size);
        job_system = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, physics_thread_count);

        physics_system.Init(max_bodies, num_body_mutexes, max_body_pairs,
                           max_contact_constraints, bp_layer_interface,
                           object_vs_bp_filter, object_pair_filter);
    }

    ~Impl() {
        JPH::BodyInterface& body_interface = physics_system.GetBodyInterface();
        for (const auto& id : body_ids) {
            body_interface.RemoveBody(id);
            body_interface.DestroyBody(id);
        }
        body_ids.clear();
    }
};

JoltPhysicsSimulator::JoltPhysicsSimulator() {
    ensure_jolt_initialized();
    impl_ = std::make_unique<Impl>();
}

JoltPhysicsSimulator::~JoltPhysicsSimulator() {
    impl_.reset();
    release_jolt();
}

namespace {

JPH::ShapeRefC create_collision_shape(const PhysicsBodyDesc& desc) {
    switch (desc.shape_type) {
    case PhysicsShapeType::SPHERE:
        return new JPH::SphereShape(static_cast<float>(desc.dimensions.x()));
    case PhysicsShapeType::BOX:
        return new JPH::BoxShape(JPH::Vec3(
            static_cast<float>(desc.dimensions.x()),
            static_cast<float>(desc.dimensions.y()),
            static_cast<float>(desc.dimensions.z())));
    case PhysicsShapeType::PLANE: {
        static constexpr float plane_half_extent = 1000.0f;
        JPH::Plane plane(JPH::Vec3(0.0f, 1.0f, 0.0f), 0.0f);
        return new JPH::PlaneShape(plane, nullptr, plane_half_extent);
    }
    case PhysicsShapeType::CYLINDER: {
        float half_height = static_cast<float>(desc.dimensions.y());
        float radius = static_cast<float>(desc.dimensions.x());
        return new JPH::CylinderShape(half_height, radius);
    }
    }
    return nullptr; // unreachable
}

struct MotionConfig {
    JPH::EMotionType motion_type;
    JPH::ObjectLayer layer;
};

MotionConfig map_body_type_to_motion(BodyType body_type) {
    switch (body_type) {
    case BodyType::STATIC:    return {JPH::EMotionType::Static, LAYER_STATIC};
    case BodyType::DYNAMIC:   return {JPH::EMotionType::Dynamic, LAYER_DYNAMIC};
    case BodyType::KINEMATIC: return {JPH::EMotionType::Kinematic, LAYER_DYNAMIC};
    }
    return {JPH::EMotionType::Static, LAYER_STATIC}; // unreachable
}

} // namespace

int JoltPhysicsSimulator::add_body(const PhysicsBodyDesc& desc) {
    JPH::BodyInterface& body_interface = impl_->physics_system.GetBodyInterface();

    JPH::ShapeRefC shape = create_collision_shape(desc);
    auto [motion_type, layer] = map_body_type_to_motion(desc.properties.body_type);

    JPH::RVec3 position(desc.position.x(), desc.position.y(), desc.position.z());
    JPH::Quat rotation(static_cast<float>(desc.rotation.x()),
                        static_cast<float>(desc.rotation.y()),
                        static_cast<float>(desc.rotation.z()),
                        static_cast<float>(desc.rotation.w()));

    JPH::BodyCreationSettings body_settings(shape, position, rotation, motion_type, layer);
    body_settings.mRestitution = static_cast<float>(desc.properties.restitution);
    body_settings.mFriction = static_cast<float>(desc.properties.friction);

    if (desc.properties.body_type == BodyType::DYNAMIC) {
        body_settings.mLinearVelocity = JPH::Vec3(
            static_cast<float>(desc.properties.initial_velocity.x()),
            static_cast<float>(desc.properties.initial_velocity.y()),
            static_cast<float>(desc.properties.initial_velocity.z()));
    }

    // Static bodies and bodies marked start_asleep are not activated.
    // Sleeping dynamic bodies wake on contact, so e.g. W-letter blocks
    // stay in place until the bowling ball hits them.
    JPH::EActivation activation;
    if (desc.properties.body_type == BodyType::STATIC || desc.properties.start_asleep) {
        activation = JPH::EActivation::DontActivate;
    } else {
        activation = JPH::EActivation::Activate;
    }

    JPH::BodyID body_id = body_interface.CreateAndAddBody(body_settings, activation);
    if (body_id.IsInvalid()) {
        throw std::runtime_error("Failed to create physics body");
    }

    int index = static_cast<int>(impl_->body_ids.size());
    impl_->body_ids.push_back(body_id);
    return index;
}

void JoltPhysicsSimulator::step(double dt) {
    impl_->physics_system.Update(static_cast<float>(dt), 1,
                                  impl_->temp_allocator.get(),
                                  impl_->job_system.get());
}

PhysicsTransform JoltPhysicsSimulator::get_transform(int body_id) const {
    if (body_id < 0 || body_id >= static_cast<int>(impl_->body_ids.size())) {
        throw std::runtime_error("Invalid body id");
    }

    const JPH::BodyInterface& body_interface = impl_->physics_system.GetBodyInterface();
    JPH::BodyID jolt_id = impl_->body_ids[body_id];

    JPH::RVec3 pos = body_interface.GetCenterOfMassPosition(jolt_id);
    JPH::Quat rot = body_interface.GetRotation(jolt_id);

    PhysicsTransform transform;
    transform.position = Point3(
        static_cast<double>(pos.GetX()),
        static_cast<double>(pos.GetY()),
        static_cast<double>(pos.GetZ()));
    transform.rotation = Quaternion(
        static_cast<double>(rot.GetX()),
        static_cast<double>(rot.GetY()),
        static_cast<double>(rot.GetZ()),
        static_cast<double>(rot.GetW()));

    return transform;
}

void JoltPhysicsSimulator::set_gravity(const Vec3& gravity) {
    impl_->physics_system.SetGravity(JPH::Vec3(
        static_cast<float>(gravity.x()),
        static_cast<float>(gravity.y()),
        static_cast<float>(gravity.z())));
}

void JoltPhysicsSimulator::wake_all() {
    JPH::BodyInterface& body_interface = impl_->physics_system.GetBodyInterface();
    for (const auto& id : impl_->body_ids) {
        if (!body_interface.IsActive(id)) {
            body_interface.ActivateBody(id);
        }
    }
}

} // namespace nwave
