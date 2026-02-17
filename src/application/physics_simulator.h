#ifndef NWAVE_APPLICATION_PHYSICS_SIMULATOR_H
#define NWAVE_APPLICATION_PHYSICS_SIMULATOR_H

#include "core/vec3.h"
#include "core/quaternion.h"
#include "domain/physics_properties.h"

namespace nwave {

enum class PhysicsShapeType {
    SPHERE,
    BOX,
    PLANE,
    CYLINDER
};

struct PhysicsTransform {
    Point3 position;
    Quaternion rotation;
};

struct PhysicsBodyDesc {
    PhysicsShapeType shape_type{PhysicsShapeType::BOX};
    Vec3 dimensions{1.0, 1.0, 1.0};
    Point3 position{0.0, 0.0, 0.0};
    Quaternion rotation;
    PhysicsProperties properties;
};

class PhysicsSimulator {
public:
    virtual ~PhysicsSimulator() = default;

    virtual int add_body(const PhysicsBodyDesc& desc) = 0;
    virtual void step(double dt) = 0;
    virtual PhysicsTransform get_transform(int body_id) const = 0;
    virtual void set_gravity(const Vec3& gravity) = 0;
};

} // namespace nwave

#endif // NWAVE_APPLICATION_PHYSICS_SIMULATOR_H
