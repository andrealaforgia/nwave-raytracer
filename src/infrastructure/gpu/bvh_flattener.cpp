#include "infrastructure/gpu/bvh_flattener.h"
#include <algorithm>
#include <limits>
#include <cstring>
#include <cmath>

namespace nwave {

namespace {

static constexpr uint32_t MAX_LEAF_SIZE = 4;
static constexpr float PLANE_BOUND = 1e4f;

struct AABB {
    float min[3];
    float max[3];
};

struct BVHBuildNode {
    AABB bounds;
    BVHBuildNode* left = nullptr;
    BVHBuildNode* right = nullptr;
    uint32_t first_prim = 0;
    uint32_t prim_count = 0;

    bool is_leaf() const { return prim_count > 0; }
};

AABB compute_shape_aabb(const GPUShape& shape) {
    AABB aabb;
    auto type = static_cast<GPUShapeType>(shape.shape_type);

    switch (type) {
        case GPUShapeType::SPHERE: {
            float cx = shape.params[0];
            float cy = shape.params[1];
            float cz = shape.params[2];
            float r  = shape.params[3];
            aabb.min[0] = cx - r;
            aabb.min[1] = cy - r;
            aabb.min[2] = cz - r;
            aabb.max[0] = cx + r;
            aabb.max[1] = cy + r;
            aabb.max[2] = cz + r;
            break;
        }
        case GPUShapeType::PLANE: {
            aabb.min[0] = -PLANE_BOUND;
            aabb.min[1] = -PLANE_BOUND;
            aabb.min[2] = -PLANE_BOUND;
            aabb.max[0] = PLANE_BOUND;
            aabb.max[1] = PLANE_BOUND;
            aabb.max[2] = PLANE_BOUND;
            break;
        }
        case GPUShapeType::BOX: {
            aabb.min[0] = shape.params[0];
            aabb.min[1] = shape.params[1];
            aabb.min[2] = shape.params[2];
            aabb.max[0] = shape.params[4];
            aabb.max[1] = shape.params[5];
            aabb.max[2] = shape.params[6];
            break;
        }
        case GPUShapeType::CYLINDER: {
            float cx = shape.params[0];
            float cy = shape.params[1];
            float cz = shape.params[2];
            float r  = shape.params[3];
            float hh = shape.params[7]; // half_height
            // Axis is params[4-6], assumed Y-aligned for conservative AABB
            aabb.min[0] = cx - r;
            aabb.min[1] = cy - hh;
            aabb.min[2] = cz - r;
            aabb.max[0] = cx + r;
            aabb.max[1] = cy + hh;
            aabb.max[2] = cz + r;
            break;
        }
        case GPUShapeType::TRIANGLE: {
            float v0x = shape.params[0], v0y = shape.params[1], v0z = shape.params[2];
            float v1x = shape.params[4], v1y = shape.params[5], v1z = shape.params[6];
            float v2x = shape.params[8], v2y = shape.params[9], v2z = shape.params[10];
            aabb.min[0] = std::fmin(v0x, std::fmin(v1x, v2x));
            aabb.min[1] = std::fmin(v0y, std::fmin(v1y, v2y));
            aabb.min[2] = std::fmin(v0z, std::fmin(v1z, v2z));
            aabb.max[0] = std::fmax(v0x, std::fmax(v1x, v2x));
            aabb.max[1] = std::fmax(v0y, std::fmax(v1y, v2y));
            aabb.max[2] = std::fmax(v0z, std::fmax(v1z, v2z));
            break;
        }
        default: {
            // Unknown shape type: use large bounds
            aabb.min[0] = -PLANE_BOUND;
            aabb.min[1] = -PLANE_BOUND;
            aabb.min[2] = -PLANE_BOUND;
            aabb.max[0] = PLANE_BOUND;
            aabb.max[1] = PLANE_BOUND;
            aabb.max[2] = PLANE_BOUND;
            break;
        }
    }
    return aabb;
}

AABB union_aabb(const AABB& a, const AABB& b) {
    AABB result;
    for (int i = 0; i < 3; ++i) {
        result.min[i] = std::fmin(a.min[i], b.min[i]);
        result.max[i] = std::fmax(a.max[i], b.max[i]);
    }
    return result;
}

AABB compute_bounds(const std::vector<AABB>& aabbs,
                    const std::vector<uint32_t>& indices,
                    uint32_t start, uint32_t end) {
    AABB bounds;
    bounds.min[0] = bounds.min[1] = bounds.min[2] = std::numeric_limits<float>::max();
    bounds.max[0] = bounds.max[1] = bounds.max[2] = -std::numeric_limits<float>::max();
    for (uint32_t i = start; i < end; ++i) {
        bounds = union_aabb(bounds, aabbs[indices[i]]);
    }
    return bounds;
}

int longest_axis(const AABB& bounds) {
    float dx = bounds.max[0] - bounds.min[0];
    float dy = bounds.max[1] - bounds.min[1];
    float dz = bounds.max[2] - bounds.min[2];
    if (dx >= dy && dx >= dz) return 0;
    if (dy >= dz) return 1;
    return 2;
}

// Arena allocator for build nodes to avoid individual allocations
struct NodeArena {
    std::vector<BVHBuildNode> storage;
    size_t next = 0;

    explicit NodeArena(size_t capacity) : storage(capacity) {}

    BVHBuildNode* alloc() {
        return &storage[next++];
    }
};

BVHBuildNode* build_recursive(
    const std::vector<AABB>& aabbs,
    std::vector<uint32_t>& indices,
    uint32_t start, uint32_t end,
    NodeArena& arena)
{
    BVHBuildNode* node = arena.alloc();
    node->bounds = compute_bounds(aabbs, indices, start, end);

    uint32_t count = end - start;
    if (count <= MAX_LEAF_SIZE) {
        node->first_prim = start;
        node->prim_count = count;
        return node;
    }

    int axis = longest_axis(node->bounds);
    uint32_t mid = start + count / 2;

    std::nth_element(
        indices.begin() + start,
        indices.begin() + mid,
        indices.begin() + end,
        [&](uint32_t a, uint32_t b) {
            float ca = (aabbs[a].min[axis] + aabbs[a].max[axis]) * 0.5f;
            float cb = (aabbs[b].min[axis] + aabbs[b].max[axis]) * 0.5f;
            return ca < cb;
        });

    node->left  = build_recursive(aabbs, indices, start, mid, arena);
    node->right = build_recursive(aabbs, indices, mid, end, arena);
    return node;
}

uint32_t flatten_recursive(
    const BVHBuildNode* node,
    std::vector<LinearBVHNode>& flat_nodes)
{
    uint32_t my_index = static_cast<uint32_t>(flat_nodes.size());
    flat_nodes.emplace_back();
    LinearBVHNode& flat = flat_nodes[my_index];

    flat.aabb_min[0] = node->bounds.min[0];
    flat.aabb_min[1] = node->bounds.min[1];
    flat.aabb_min[2] = node->bounds.min[2];
    flat.aabb_max[0] = node->bounds.max[0];
    flat.aabb_max[1] = node->bounds.max[1];
    flat.aabb_max[2] = node->bounds.max[2];

    if (node->is_leaf()) {
        flat.offset = node->first_prim;
        flat.count  = node->prim_count;
    } else {
        flat.count = 0;
        // Left child immediately follows in DFS order
        flatten_recursive(node->left, flat_nodes);
        // Store second child offset
        flat_nodes[my_index].offset = static_cast<uint32_t>(flat_nodes.size());
        flatten_recursive(node->right, flat_nodes);
    }

    return my_index;
}

} // anonymous namespace

BVHFlattener::Result BVHFlattener::build_and_flatten(
    const std::vector<GPUShape>& shapes) const
{
    Result result;

    if (shapes.empty()) {
        return result;
    }

    uint32_t n = static_cast<uint32_t>(shapes.size());

    // Compute AABBs for all shapes
    std::vector<AABB> aabbs(n);
    for (uint32_t i = 0; i < n; ++i) {
        aabbs[i] = compute_shape_aabb(shapes[i]);
    }

    // Initialize index array
    std::vector<uint32_t> indices(n);
    for (uint32_t i = 0; i < n; ++i) {
        indices[i] = i;
    }

    // Build tree (max nodes = 2*n - 1 for a full binary tree)
    NodeArena arena(2 * n);
    BVHBuildNode* root = build_recursive(aabbs, indices, 0, n, arena);

    // Flatten into linear array
    flatten_recursive(root, result.nodes);

    // The indices array has been reordered during build -- this is the shape_order
    result.shape_order = std::move(indices);

    return result;
}

} // namespace nwave
