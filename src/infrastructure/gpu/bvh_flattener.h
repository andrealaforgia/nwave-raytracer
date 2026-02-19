#ifndef NWAVE_INFRASTRUCTURE_GPU_BVH_FLATTENER_H
#define NWAVE_INFRASTRUCTURE_GPU_BVH_FLATTENER_H

#include "core/gpu_types.h"
#include <vector>
#include <cstdint>

namespace nwave {

class BVHFlattener {
public:
    struct Result {
        std::vector<LinearBVHNode> nodes;
        std::vector<uint32_t> shape_order;  // reordered shape indices
    };

    Result build_and_flatten(const std::vector<GPUShape>& shapes) const;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_GPU_BVH_FLATTENER_H
