#ifndef NWAVE_DOMAIN_PHYSICS_PROPERTIES_H
#define NWAVE_DOMAIN_PHYSICS_PROPERTIES_H

#include "core/vec3.h"

namespace nwave {

enum class BodyType {
    STATIC,
    DYNAMIC,
    KINEMATIC
};

struct PhysicsProperties {
    BodyType body_type{BodyType::STATIC};
    double mass{1.0};
    Vec3 initial_velocity{0.0, 0.0, 0.0};
    double friction{0.5};
    double restitution{0.3};
};

} // namespace nwave

#endif // NWAVE_DOMAIN_PHYSICS_PROPERTIES_H
