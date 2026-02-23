#include <gtest/gtest.h>
#include "infrastructure/gpu/scene_flattener.h"
#include "domain/scene.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/box.h"
#include "domain/shapes/cylinder.h"
#include "domain/shapes/triangle.h"
#include "domain/shapes/transformed_shape.h"
#include "domain/shapes/triangle_mesh.h"
#include "domain/materials/lambertian.h"
#include "domain/materials/metal.h"
#include "domain/materials/dielectric.h"
#include "domain/materials/emissive.h"
#include "domain/lights/point_light.h"
#include "domain/lights/directional_light.h"
#include "core/matrix4x4.h"
#include <cmath>
#include <chrono>

using namespace nwave;

class SceneFlattenerTest : public ::testing::Test {
protected:
    SceneFlattener flattener;
};

TEST_F(SceneFlattenerTest, EmptySceneProducesEmptyArrays) {
    Scene scene;

    auto result = flattener.flatten(scene);

    EXPECT_TRUE(result.shapes.empty());
    EXPECT_TRUE(result.materials.empty());
}

TEST_F(SceneFlattenerTest, SingleSphereFlattensCorrectly) {
    Lambertian mat(Color3(0.8, 0.2, 0.1));
    Scene scene;
    scene.add_shape(std::make_shared<Sphere>(Point3(1, 2, 3), 0.5, &mat));

    auto result = flattener.flatten(scene);

    ASSERT_EQ(result.shapes.size(), 1u);
    ASSERT_EQ(result.materials.size(), 1u);

    const auto& gpu_shape = result.shapes[0];
    EXPECT_EQ(gpu_shape.shape_type, static_cast<uint32_t>(GPUShapeType::SPHERE));
    EXPECT_EQ(gpu_shape.material_index, 0u);
    EXPECT_EQ(gpu_shape.has_transform, 0u);
    EXPECT_FLOAT_EQ(gpu_shape.params[0], 1.0f);  // center.x
    EXPECT_FLOAT_EQ(gpu_shape.params[1], 2.0f);  // center.y
    EXPECT_FLOAT_EQ(gpu_shape.params[2], 3.0f);  // center.z
    EXPECT_FLOAT_EQ(gpu_shape.params[3], 0.5f);  // radius

    const auto& gpu_mat = result.materials[0];
    EXPECT_EQ(gpu_mat.material_type, static_cast<uint32_t>(GPUMaterialType::LAMBERTIAN));
    EXPECT_FLOAT_EQ(gpu_mat.albedo[0], 0.8f);
    EXPECT_FLOAT_EQ(gpu_mat.albedo[1], 0.2f);
    EXPECT_FLOAT_EQ(gpu_mat.albedo[2], 0.1f);
}

TEST_F(SceneFlattenerTest, SharedMaterialDeduplication) {
    Lambertian shared_mat(Color3(0.5, 0.5, 0.5));
    Scene scene;
    scene.add_shape(std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, &shared_mat));
    scene.add_shape(std::make_shared<Sphere>(Point3(2, 0, 0), 1.0, &shared_mat));
    scene.add_shape(std::make_shared<Sphere>(Point3(4, 0, 0), 1.0, &shared_mat));

    auto result = flattener.flatten(scene);

    ASSERT_EQ(result.shapes.size(), 3u);
    ASSERT_EQ(result.materials.size(), 1u);
    EXPECT_EQ(result.shapes[0].material_index, 0u);
    EXPECT_EQ(result.shapes[1].material_index, 0u);
    EXPECT_EQ(result.shapes[2].material_index, 0u);
}

TEST_F(SceneFlattenerTest, AllFiveShapeTypesFlattened) {
    Lambertian mat_lamb(Color3(0.8, 0.2, 0.1));
    Metal mat_metal(Color3(0.9, 0.9, 0.9), 0.1);
    Dielectric mat_glass(1.5, Color3(1.0, 1.0, 0.9));
    Emissive mat_emissive(Color3(1.0, 1.0, 1.0), 5.0);

    Scene scene;
    scene.add_shape(std::make_shared<Sphere>(Point3(1, 2, 3), 0.5, &mat_lamb));
    scene.add_shape(std::make_shared<Plane>(Point3(0, 0, 0), Vec3(0, 1, 0), &mat_metal));
    scene.add_shape(std::make_shared<Box>(Point3(-1, -1, -1), Point3(1, 1, 1), &mat_glass));
    scene.add_shape(std::make_shared<Cylinder>(Point3(0, 0, 0), 0.5, 2.0, &mat_emissive));
    scene.add_shape(std::make_shared<Triangle>(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0), &mat_lamb));

    auto result = flattener.flatten(scene);

    ASSERT_EQ(result.shapes.size(), 5u);
    // 4 unique materials (mat_lamb shared by sphere and triangle)
    ASSERT_EQ(result.materials.size(), 4u);

    // Verify shape types
    EXPECT_EQ(result.shapes[0].shape_type, static_cast<uint32_t>(GPUShapeType::SPHERE));
    EXPECT_EQ(result.shapes[1].shape_type, static_cast<uint32_t>(GPUShapeType::PLANE));
    EXPECT_EQ(result.shapes[2].shape_type, static_cast<uint32_t>(GPUShapeType::BOX));
    EXPECT_EQ(result.shapes[3].shape_type, static_cast<uint32_t>(GPUShapeType::CYLINDER));
    EXPECT_EQ(result.shapes[4].shape_type, static_cast<uint32_t>(GPUShapeType::TRIANGLE));

    // Verify plane params: point=(0,0,0), normal=(0,1,0)
    EXPECT_FLOAT_EQ(result.shapes[1].params[0], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[1].params[1], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[1].params[2], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[1].params[4], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[1].params[5], 1.0f);
    EXPECT_FLOAT_EQ(result.shapes[1].params[6], 0.0f);

    // Verify box params: bmin=(-1,-1,-1), bmax=(1,1,1)
    EXPECT_FLOAT_EQ(result.shapes[2].params[0], -1.0f);
    EXPECT_FLOAT_EQ(result.shapes[2].params[1], -1.0f);
    EXPECT_FLOAT_EQ(result.shapes[2].params[2], -1.0f);
    EXPECT_FLOAT_EQ(result.shapes[2].params[4], 1.0f);
    EXPECT_FLOAT_EQ(result.shapes[2].params[5], 1.0f);
    EXPECT_FLOAT_EQ(result.shapes[2].params[6], 1.0f);

    // Verify cylinder params: center=(0,0,0), radius=0.5, axis=(0,1,0), half_height=1.0
    EXPECT_FLOAT_EQ(result.shapes[3].params[0], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[3].params[1], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[3].params[2], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[3].params[3], 0.5f);
    EXPECT_FLOAT_EQ(result.shapes[3].params[4], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[3].params[5], 1.0f);
    EXPECT_FLOAT_EQ(result.shapes[3].params[6], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[3].params[7], 1.0f);  // half_height = height/2

    // Verify triangle params: v0=(0,0,0), v1=(1,0,0), v2=(0,1,0)
    EXPECT_FLOAT_EQ(result.shapes[4].params[0], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[4].params[1], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[4].params[2], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[4].params[4], 1.0f);
    EXPECT_FLOAT_EQ(result.shapes[4].params[5], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[4].params[6], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[4].params[8], 0.0f);
    EXPECT_FLOAT_EQ(result.shapes[4].params[9], 1.0f);
    EXPECT_FLOAT_EQ(result.shapes[4].params[10], 0.0f);

    // Verify material types
    EXPECT_EQ(result.materials[0].material_type, static_cast<uint32_t>(GPUMaterialType::LAMBERTIAN));
    EXPECT_EQ(result.materials[1].material_type, static_cast<uint32_t>(GPUMaterialType::METAL));
    EXPECT_EQ(result.materials[2].material_type, static_cast<uint32_t>(GPUMaterialType::DIELECTRIC));
    EXPECT_EQ(result.materials[3].material_type, static_cast<uint32_t>(GPUMaterialType::EMISSIVE));

    // Verify metal material params
    EXPECT_FLOAT_EQ(result.materials[1].albedo[0], 0.9f);
    EXPECT_FLOAT_EQ(result.materials[1].param1, 0.1f);

    // Verify dielectric material params
    EXPECT_FLOAT_EQ(result.materials[2].param1, 1.5f);
    EXPECT_FLOAT_EQ(result.materials[2].tint[0], 1.0f);
    EXPECT_FLOAT_EQ(result.materials[2].tint[1], 1.0f);
    EXPECT_FLOAT_EQ(result.materials[2].tint[2], 0.9f);

    // Verify emissive material: color in albedo, intensity in param1
    EXPECT_FLOAT_EQ(result.materials[3].albedo[0], 1.0f);
    EXPECT_FLOAT_EQ(result.materials[3].albedo[1], 1.0f);
    EXPECT_FLOAT_EQ(result.materials[3].albedo[2], 1.0f);
    EXPECT_FLOAT_EQ(result.materials[3].param1, 5.0f);

    // Sphere and Triangle share mat_lamb: same material_index
    EXPECT_EQ(result.shapes[0].material_index, result.shapes[4].material_index);
}

TEST_F(SceneFlattenerTest, TransformedShapeHasInverseMatrix) {
    Lambertian mat(Color3(0.5, 0.5, 0.5));
    auto inner_box = std::make_shared<Box>(Point3(-1, -1, -1), Point3(1, 1, 1), &mat);

    // Create a 45-degree rotation around Y axis via translation
    // Use a simple translation transform for deterministic testing
    Matrix4x4 transform = Matrix4x4::translation(3.0, 5.0, 7.0);
    auto transformed = std::make_shared<TransformedShape>(inner_box, transform);

    Scene scene;
    scene.add_shape(transformed);

    auto result = flattener.flatten(scene);

    ASSERT_EQ(result.shapes.size(), 1u);
    const auto& gpu_shape = result.shapes[0];

    EXPECT_EQ(gpu_shape.shape_type, static_cast<uint32_t>(GPUShapeType::BOX));
    EXPECT_EQ(gpu_shape.has_transform, 1u);

    // The inner box params should still be present
    EXPECT_FLOAT_EQ(gpu_shape.params[0], -1.0f);
    EXPECT_FLOAT_EQ(gpu_shape.params[1], -1.0f);
    EXPECT_FLOAT_EQ(gpu_shape.params[2], -1.0f);
    EXPECT_FLOAT_EQ(gpu_shape.params[4], 1.0f);
    EXPECT_FLOAT_EQ(gpu_shape.params[5], 1.0f);
    EXPECT_FLOAT_EQ(gpu_shape.params[6], 1.0f);

    // Verify inverse transform stored in column-major order (matching Metal float4x4)
    // Inverse of translation(3,5,7) is translation(-3,-5,-7)
    Matrix4x4 expected_inverse = Matrix4x4::translation(-3.0, -5.0, -7.0);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            EXPECT_NEAR(gpu_shape.inverse_transform[c * 4 + r],
                       static_cast<float>(expected_inverse.m[r][c]),
                       1e-5f)
                << "Mismatch at inverse_transform row=" << r << " col=" << c;
        }
    }
}

TEST_F(SceneFlattenerTest, PointLightFlattensCorrectly) {
    Scene scene;
    scene.add_light(std::make_shared<PointLight>(
        Point3(1.0, 2.0, 3.0), Color3(1.0, 1.0, 1.0), 0.8));

    auto result = flattener.flatten(scene);

    ASSERT_EQ(result.lights.size(), 1u);
    const auto& gpu_light = result.lights[0];
    EXPECT_EQ(gpu_light.light_type, static_cast<uint32_t>(GPULightType::POINT));
    EXPECT_FLOAT_EQ(gpu_light.position[0], 1.0f);
    EXPECT_FLOAT_EQ(gpu_light.position[1], 2.0f);
    EXPECT_FLOAT_EQ(gpu_light.position[2], 3.0f);
    EXPECT_FLOAT_EQ(gpu_light.color[0], 1.0f);
    EXPECT_FLOAT_EQ(gpu_light.color[1], 1.0f);
    EXPECT_FLOAT_EQ(gpu_light.color[2], 1.0f);
    EXPECT_FLOAT_EQ(gpu_light.intensity, 0.8f);
}

TEST_F(SceneFlattenerTest, DirectionalLightFlattensCorrectly) {
    Scene scene;
    scene.add_light(std::make_shared<DirectionalLight>(
        Vec3(0.0, -1.0, 0.0), Color3(0.9, 0.8, 0.7), 1.5));

    auto result = flattener.flatten(scene);

    ASSERT_EQ(result.lights.size(), 1u);
    const auto& gpu_light = result.lights[0];
    EXPECT_EQ(gpu_light.light_type, static_cast<uint32_t>(GPULightType::DIRECTIONAL));
    // Direction is negated by the flattener so the shader receives toward-light
    EXPECT_FLOAT_EQ(gpu_light.position[0], 0.0f);
    EXPECT_FLOAT_EQ(gpu_light.position[1], 1.0f);
    EXPECT_FLOAT_EQ(gpu_light.position[2], 0.0f);
    EXPECT_FLOAT_EQ(gpu_light.color[0], 0.9f);
    EXPECT_FLOAT_EQ(gpu_light.color[1], 0.8f);
    EXPECT_FLOAT_EQ(gpu_light.color[2], 0.7f);
    EXPECT_FLOAT_EQ(gpu_light.intensity, 1.5f);
}

TEST_F(SceneFlattenerTest, NullMaterialDefaultsToIndex0) {
    Scene scene;
    // Sphere with nullptr material
    scene.add_shape(std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, nullptr));

    auto result = flattener.flatten(scene);

    ASSERT_EQ(result.shapes.size(), 1u);
    ASSERT_GE(result.materials.size(), 1u);
    EXPECT_EQ(result.shapes[0].material_index, 0u);
}

TEST_F(SceneFlattenerTest, TriangleMeshIsSkippedDuringFlattening) {
    Lambertian mat(Color3(0.5, 0.5, 0.5));
    std::vector<Point3> vertices = {
        Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0)};
    std::vector<Vec3> normals;
    std::vector<int> vertex_indices = {0, 1, 2};
    std::vector<int> normal_indices;

    Scene scene;
    scene.add_shape(std::make_shared<TriangleMesh>(
        vertices, normals, vertex_indices, normal_indices, &mat));
    // Also add a valid sphere to verify mesh is skipped but sphere is kept
    scene.add_shape(std::make_shared<Sphere>(Point3(0, 0, 0), 1.0, &mat));

    auto result = flattener.flatten(scene);

    // Only the sphere should be present, mesh skipped
    ASSERT_EQ(result.shapes.size(), 1u);
    EXPECT_EQ(result.shapes[0].shape_type,
              static_cast<uint32_t>(GPUShapeType::SPHERE));
}

TEST_F(SceneFlattenerTest, LargeSceneFlattensUnder10ms) {
    Lambertian mat(Color3(0.5, 0.5, 0.5));
    Scene scene;
    for (int i = 0; i < 1000; ++i) {
        scene.add_shape(std::make_shared<Sphere>(
            Point3(i * 1.0, 0, 0), 0.5, &mat));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto result = flattener.flatten(scene);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    ASSERT_EQ(result.shapes.size(), 1000u);
    EXPECT_LT(elapsed_ms, 10) << "Flattening 1000 shapes took " << elapsed_ms << "ms";
}
