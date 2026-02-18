#include <gtest/gtest.h>
#include "infrastructure/gpu/bvh_flattener.h"
#include "core/gpu_types.h"
#include <cstring>
#include <cmath>

using namespace nwave;

namespace {

GPUShape make_sphere(float cx, float cy, float cz, float radius) {
    GPUShape shape{};
    std::memset(&shape, 0, sizeof(GPUShape));
    shape.shape_type = static_cast<uint32_t>(GPUShapeType::SPHERE);
    shape.material_index = 0;
    shape.has_transform = 0;
    shape.params[0] = cx;
    shape.params[1] = cy;
    shape.params[2] = cz;
    shape.params[3] = radius;
    return shape;
}

GPUShape make_box(float bmin_x, float bmin_y, float bmin_z,
                  float bmax_x, float bmax_y, float bmax_z) {
    GPUShape shape{};
    std::memset(&shape, 0, sizeof(GPUShape));
    shape.shape_type = static_cast<uint32_t>(GPUShapeType::BOX);
    shape.material_index = 0;
    shape.has_transform = 0;
    shape.params[0] = bmin_x;
    shape.params[1] = bmin_y;
    shape.params[2] = bmin_z;
    shape.params[4] = bmax_x;
    shape.params[5] = bmax_y;
    shape.params[6] = bmax_z;
    return shape;
}

GPUShape make_triangle(float v0x, float v0y, float v0z,
                       float v1x, float v1y, float v1z,
                       float v2x, float v2y, float v2z) {
    GPUShape shape{};
    std::memset(&shape, 0, sizeof(GPUShape));
    shape.shape_type = static_cast<uint32_t>(GPUShapeType::TRIANGLE);
    shape.material_index = 0;
    shape.has_transform = 0;
    shape.params[0] = v0x;
    shape.params[1] = v0y;
    shape.params[2] = v0z;
    shape.params[4] = v1x;
    shape.params[5] = v1y;
    shape.params[6] = v1z;
    shape.params[8] = v2x;
    shape.params[9] = v2y;
    shape.params[10] = v2z;
    return shape;
}

} // anonymous namespace

class BVHFlattenerTest : public ::testing::Test {
protected:
    BVHFlattener flattener;
};

// AC1: Given 10 GPUShapes with known AABBs, build_and_flatten returns non-empty vector
TEST_F(BVHFlattenerTest, TenShapesProduceNonEmptyBVH) {
    std::vector<GPUShape> shapes;
    for (int i = 0; i < 10; ++i) {
        shapes.push_back(make_sphere(
            static_cast<float>(i * 3), 0.0f, 0.0f, 1.0f));
    }

    auto result = flattener.build_and_flatten(shapes);

    EXPECT_FALSE(result.nodes.empty());
    EXPECT_EQ(result.shape_order.size(), 10u);
}

// AC2: Interior nodes have count=0 and offset pointing to valid second child index
TEST_F(BVHFlattenerTest, InteriorNodesHaveZeroCountAndValidSecondChildOffset) {
    std::vector<GPUShape> shapes;
    for (int i = 0; i < 10; ++i) {
        shapes.push_back(make_sphere(
            static_cast<float>(i * 3), 0.0f, 0.0f, 1.0f));
    }

    auto result = flattener.build_and_flatten(shapes);

    bool found_interior = false;
    for (size_t i = 0; i < result.nodes.size(); ++i) {
        const auto& node = result.nodes[i];
        if (node.count == 0) {
            found_interior = true;
            EXPECT_GT(node.offset, static_cast<uint32_t>(i))
                << "Interior node " << i << " second child offset must be > current index";
            EXPECT_LT(node.offset, static_cast<uint32_t>(result.nodes.size()))
                << "Interior node " << i << " second child offset must be within bounds";
        }
    }
    EXPECT_TRUE(found_interior) << "Expected at least one interior node for 10 shapes";
}

// AC3: Leaf nodes have count>0 and offset pointing to valid shape indices
TEST_F(BVHFlattenerTest, LeafNodesHavePositiveCountAndValidShapeIndices) {
    std::vector<GPUShape> shapes;
    for (int i = 0; i < 10; ++i) {
        shapes.push_back(make_sphere(
            static_cast<float>(i * 3), 0.0f, 0.0f, 1.0f));
    }

    auto result = flattener.build_and_flatten(shapes);

    uint32_t total_prims = 0;
    bool found_leaf = false;
    for (size_t i = 0; i < result.nodes.size(); ++i) {
        const auto& node = result.nodes[i];
        if (node.count > 0) {
            found_leaf = true;
            EXPECT_LE(node.offset + node.count,
                      static_cast<uint32_t>(result.shape_order.size()))
                << "Leaf node " << i << " primitive range out of bounds";
            // Each shape index in shape_order must be valid
            for (uint32_t j = node.offset; j < node.offset + node.count; ++j) {
                EXPECT_LT(result.shape_order[j], 10u)
                    << "Shape index at position " << j << " out of range";
            }
            total_prims += node.count;
        }
    }
    EXPECT_TRUE(found_leaf) << "Expected at least one leaf node";
    EXPECT_EQ(total_prims, 10u) << "All shapes must be accounted for in leaf nodes";
}

// AC4: Given 0 shapes, build_and_flatten returns empty vector without error
TEST_F(BVHFlattenerTest, ZeroShapesProduceEmptyResult) {
    std::vector<GPUShape> shapes;

    auto result = flattener.build_and_flatten(shapes);

    EXPECT_TRUE(result.nodes.empty());
    EXPECT_TRUE(result.shape_order.empty());
}

// AC5: Given 1 shape, build_and_flatten returns single leaf node
TEST_F(BVHFlattenerTest, SingleShapeProducesSingleLeafNode) {
    std::vector<GPUShape> shapes;
    shapes.push_back(make_sphere(1.0f, 2.0f, 3.0f, 0.5f));

    auto result = flattener.build_and_flatten(shapes);

    ASSERT_EQ(result.nodes.size(), 1u);
    EXPECT_GT(result.nodes[0].count, 0u) << "Single node must be a leaf";
    EXPECT_EQ(result.nodes[0].offset, 0u) << "Leaf must point to first primitive";
    EXPECT_EQ(result.nodes[0].count, 1u);
    ASSERT_EQ(result.shape_order.size(), 1u);
    EXPECT_EQ(result.shape_order[0], 0u);

    // Verify AABB matches the sphere bounds
    EXPECT_FLOAT_EQ(result.nodes[0].aabb_min[0], 0.5f);  // 1.0 - 0.5
    EXPECT_FLOAT_EQ(result.nodes[0].aabb_min[1], 1.5f);  // 2.0 - 0.5
    EXPECT_FLOAT_EQ(result.nodes[0].aabb_min[2], 2.5f);  // 3.0 - 0.5
    EXPECT_FLOAT_EQ(result.nodes[0].aabb_max[0], 1.5f);  // 1.0 + 0.5
    EXPECT_FLOAT_EQ(result.nodes[0].aabb_max[1], 2.5f);  // 2.0 + 0.5
    EXPECT_FLOAT_EQ(result.nodes[0].aabb_max[2], 3.5f);  // 3.0 + 0.5
}
