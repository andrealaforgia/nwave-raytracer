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
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Body/BodyLock.h>

#include <mutex>
#include <set>
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
    std::set<int> soft_body_ids;

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

    bool is_valid_body_id(int body_id) const {
        return body_id >= 0 && body_id < static_cast<int>(body_ids.size());
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
    case BodyType::SOFT:      return {JPH::EMotionType::Dynamic, LAYER_DYNAMIC};
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
    if (!impl_->is_valid_body_id(body_id)) {
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

int JoltPhysicsSimulator::add_soft_body(const SoftBodyDesc& desc) {
    const int grid_size = desc.grid_resolution;
    const float spacing = static_cast<float>(desc.grid_spacing);

    // Vertex index helper: x + y*grid_size + z*grid_size*grid_size
    auto vertex_index = [grid_size](int x, int y, int z) -> uint32_t {
        return static_cast<uint32_t>(x + y * grid_size + z * grid_size * grid_size);
    };

    // 1. Create shared settings with grid_size^3 vertex grid
    JPH::Ref<JPH::SoftBodySharedSettings> shared = new JPH::SoftBodySharedSettings;

    // Center the grid around origin so the body's center of mass aligns with desc.position
    const float half_extent = spacing * static_cast<float>(grid_size - 1) * 0.5f;

    for (int z = 0; z < grid_size; ++z) {
        for (int y = 0; y < grid_size; ++y) {
            for (int x = 0; x < grid_size; ++x) {
                JPH::SoftBodySharedSettings::Vertex v;
                JPH::Vec3 pos(
                    static_cast<float>(x) * spacing - half_extent,
                    static_cast<float>(y) * spacing - half_extent,
                    static_cast<float>(z) * spacing - half_extent);
                pos.StoreFloat3(&v.mPosition);
                v.mInvMass = 1.0f;
                shared->mVertices.push_back(v);
            }
        }
    }

    // 2. Add edge constraints along each axis (x, y, z neighbors)
    const float edge_compliance = static_cast<float>(desc.edge_compliance);
    for (int z = 0; z < grid_size; ++z) {
        for (int y = 0; y < grid_size; ++y) {
            for (int x = 0; x < grid_size; ++x) {
                uint32_t idx = vertex_index(x, y, z);
                if (x < grid_size - 1) {
                    shared->mEdgeConstraints.push_back(
                        JPH::SoftBodySharedSettings::Edge(idx, vertex_index(x + 1, y, z), edge_compliance));
                }
                if (y < grid_size - 1) {
                    shared->mEdgeConstraints.push_back(
                        JPH::SoftBodySharedSettings::Edge(idx, vertex_index(x, y + 1, z), edge_compliance));
                }
                if (z < grid_size - 1) {
                    shared->mEdgeConstraints.push_back(
                        JPH::SoftBodySharedSettings::Edge(idx, vertex_index(x, y, z + 1), edge_compliance));
                }
            }
        }
    }
    shared->CalculateEdgeLengths();

    // 3. Add tetrahedral volume constraints (6 tetrahedra per cube cell)
    // Each cube cell is decomposed into 6 tetrahedra using a fixed pattern.
    const int tetra_offsets[6][4][3] = {
        { {0,0,0}, {0,1,1}, {0,0,1}, {1,1,1} },
        { {0,0,0}, {0,1,0}, {0,1,1}, {1,1,1} },
        { {0,0,0}, {0,0,1}, {1,0,1}, {1,1,1} },
        { {0,0,0}, {1,0,1}, {1,0,0}, {1,1,1} },
        { {0,0,0}, {1,1,0}, {0,1,0}, {1,1,1} },
        { {0,0,0}, {1,0,0}, {1,1,0}, {1,1,1} }
    };

    const float volume_compliance = static_cast<float>(desc.volume_compliance);
    for (int z = 0; z < grid_size - 1; ++z) {
        for (int y = 0; y < grid_size - 1; ++y) {
            for (int x = 0; x < grid_size - 1; ++x) {
                for (int t = 0; t < 6; ++t) {
                    uint32_t v0 = vertex_index(x + tetra_offsets[t][0][0],
                                               y + tetra_offsets[t][0][1],
                                               z + tetra_offsets[t][0][2]);
                    uint32_t v1 = vertex_index(x + tetra_offsets[t][1][0],
                                               y + tetra_offsets[t][1][1],
                                               z + tetra_offsets[t][1][2]);
                    uint32_t v2 = vertex_index(x + tetra_offsets[t][2][0],
                                               y + tetra_offsets[t][2][1],
                                               z + tetra_offsets[t][2][2]);
                    uint32_t v3 = vertex_index(x + tetra_offsets[t][3][0],
                                               y + tetra_offsets[t][3][1],
                                               z + tetra_offsets[t][3][2]);
                    shared->mVolumeConstraints.push_back(
                        JPH::SoftBodySharedSettings::Volume(v0, v1, v2, v3, volume_compliance));
                }
            }
        }
    }
    shared->CalculateVolumeConstraintVolumes();

    // 4. Add surface faces (triangulated quads) on all 6 outer faces of the cube
    const int last = grid_size - 1;

    // Adds two triangles forming a quad from four vertex indices.
    // Winding follows Jolt's convention for correct pressure/volume computation.
    // The renderer flips winding when creating GPU triangles for outward-facing normals.
    auto add_quad_face = [&shared](uint32_t v00, uint32_t v10, uint32_t v11, uint32_t v01) {
        shared->AddFace(JPH::SoftBodySharedSettings::Face(v00, v10, v11));
        shared->AddFace(JPH::SoftBodySharedSettings::Face(v00, v11, v01));
    };

    for (int a = 0; a < last; ++a) {
        for (int b = 0; b < last; ++b) {
            // Front (z=0) and back (z=last)
            add_quad_face(vertex_index(b, a, 0), vertex_index(b + 1, a, 0),
                          vertex_index(b + 1, a + 1, 0), vertex_index(b, a + 1, 0));
            add_quad_face(vertex_index(b, a, last), vertex_index(b, a + 1, last),
                          vertex_index(b + 1, a + 1, last), vertex_index(b + 1, a, last));

            // Bottom (y=0) and top (y=last)
            add_quad_face(vertex_index(b, 0, a), vertex_index(b, 0, a + 1),
                          vertex_index(b + 1, 0, a + 1), vertex_index(b + 1, 0, a));
            add_quad_face(vertex_index(b, last, a), vertex_index(b + 1, last, a),
                          vertex_index(b + 1, last, a + 1), vertex_index(b, last, a + 1));

            // Left (x=0) and right (x=last)
            add_quad_face(vertex_index(0, a, b), vertex_index(0, a + 1, b),
                          vertex_index(0, a + 1, b + 1), vertex_index(0, a, b + 1));
            add_quad_face(vertex_index(last, a, b), vertex_index(last, a, b + 1),
                          vertex_index(last, a + 1, b + 1), vertex_index(last, a + 1, b));
        }
    }

    // 5. Optimize for parallel constraint execution
    shared->Optimize();

    // 6. Create SoftBodyCreationSettings
    JPH::RVec3 position(desc.position.x(), desc.position.y(), desc.position.z());
    JPH::SoftBodyCreationSettings creation_settings(
        shared, position, JPH::Quat::sIdentity(), LAYER_DYNAMIC);

    creation_settings.mNumIterations = static_cast<uint32_t>(desc.solver_iterations);
    creation_settings.mPressure = static_cast<float>(desc.pressure);
    creation_settings.mRestitution = static_cast<float>(desc.restitution);
    creation_settings.mFriction = static_cast<float>(desc.friction);
    creation_settings.mLinearDamping = static_cast<float>(desc.damping);
    creation_settings.mGravityFactor = 1.0f;

    // 7. Create and add the soft body to the physics system
    JPH::BodyInterface& body_interface = impl_->physics_system.GetBodyInterface();
    JPH::BodyID body_id = body_interface.CreateAndAddSoftBody(
        creation_settings, JPH::EActivation::Activate);

    if (body_id.IsInvalid()) {
        throw std::runtime_error("Failed to create soft body");
    }

    int index = static_cast<int>(impl_->body_ids.size());
    impl_->body_ids.push_back(body_id);
    impl_->soft_body_ids.insert(index);
    return index;
}

bool JoltPhysicsSimulator::is_soft_body(int body_id) const {
    if (!impl_->is_valid_body_id(body_id)) {
        return false;
    }
    return impl_->soft_body_ids.count(body_id) > 0;
}

SoftBodyMeshData JoltPhysicsSimulator::get_soft_body_mesh(int body_id) const {
    if (!impl_->is_valid_body_id(body_id)) {
        throw std::runtime_error("Invalid body id");
    }
    if (impl_->soft_body_ids.count(body_id) == 0) {
        throw std::runtime_error("Body is not a soft body");
    }

    JPH::BodyID jolt_id = impl_->body_ids[body_id];
    JPH::BodyLockRead lock(impl_->physics_system.GetBodyLockInterface(), jolt_id);
    if (!lock.Succeeded()) {
        throw std::runtime_error("Failed to lock soft body for reading");
    }

    const JPH::Body& body = lock.GetBody();
    const auto* motion_props =
        static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

    // Get the center of mass transform for world-space conversion
    JPH::RMat44 transform = body.GetCenterOfMassTransform();

    // Extract vertices and transform to world space
    // SoftBodyVertex::mPosition is a Vec3 in local (center-of-mass) space
    const auto& jolt_vertices = motion_props->GetVertices();
    SoftBodyMeshData mesh;
    mesh.vertices.reserve(jolt_vertices.size());
    for (const auto& sv : jolt_vertices) {
        JPH::RVec3 world_pos = transform * sv.mPosition;
        mesh.vertices.emplace_back(
            static_cast<double>(world_pos.GetX()),
            static_cast<double>(world_pos.GetY()),
            static_cast<double>(world_pos.GetZ()));
    }

    // Extract face indices from the shared settings
    const JPH::SoftBodySharedSettings* settings = motion_props->GetSettings();
    const auto& faces = settings->mFaces;
    mesh.face_indices.reserve(faces.size() * 3);
    for (const auto& face : faces) {
        mesh.face_indices.push_back(static_cast<int>(face.mVertex[0]));
        mesh.face_indices.push_back(static_cast<int>(face.mVertex[1]));
        mesh.face_indices.push_back(static_cast<int>(face.mVertex[2]));
    }

    return mesh;
}

} // namespace nwave
