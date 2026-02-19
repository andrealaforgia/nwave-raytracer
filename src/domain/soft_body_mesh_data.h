#ifndef NWAVE_DOMAIN_SOFT_BODY_MESH_DATA_H
#define NWAVE_DOMAIN_SOFT_BODY_MESH_DATA_H

#include "core/vec3.h"
#include <vector>

namespace nwave {

struct SoftBodyMeshData {
    std::vector<Point3> vertices;
    std::vector<int> face_indices;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SOFT_BODY_MESH_DATA_H
