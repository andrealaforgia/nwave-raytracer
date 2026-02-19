#ifndef NWAVE_DOMAIN_SOFT_BODY_DESC_H
#define NWAVE_DOMAIN_SOFT_BODY_DESC_H

#include "core/vec3.h"

namespace nwave {

class Material;

struct SoftBodyDesc {
    int grid_resolution{5};
    double grid_spacing{0.5};
    Point3 position{0.0, 0.0, 0.0};
    double pressure{2000.0};
    double restitution{0.3};
    double friction{0.2};
    double damping{0.1};
    int solver_iterations{5};
    double edge_compliance{1e-4};
    double volume_compliance{0.0};
    const Material* material{nullptr};
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SOFT_BODY_DESC_H
