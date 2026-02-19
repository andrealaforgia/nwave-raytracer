#include <gtest/gtest.h>
#include "infrastructure/yaml_scene_loader.h"
#include "domain/soft_body_desc.h"
#include "domain/materials/lambertian.h"
#include <fstream>
#include <string>

using namespace nwave;

// Test Budget: 5 behaviors x 2 = 10 max unit tests. Using 5 tests.

class YamlSoftBodyParsingTest : public ::testing::Test {
protected:
    YamlSceneLoader loader;

    static constexpr const char* scene_suffix = R"(
lights:
  - type: point
    position: [0, 10, 0]
    color: [1, 1, 1]
    intensity: 1.0
camera:
  lookfrom: [0, 0, 5]
  lookat: [0, 0, 0]
  vup: [0, 1, 0]
  vfov: 45
  image_width: 640
)";

    std::string make_scene(const std::string& objects_yaml) {
        return std::string(R"(
materials:
  - name: jelly
    type: lambertian
    albedo: [0.9, 0.3, 0.5]
objects:
)") + objects_yaml + scene_suffix;
    }
};

TEST_F(YamlSoftBodyParsingTest, ParsesSoftBodyCubeWithAllFieldsSpecified) {
    auto result = loader.load(make_scene(R"(
  - type: soft_body_cube
    material: jelly
    position: [1, 5, -2]
    grid_resolution: 8
    grid_spacing: 0.3
    pressure: 3000.0
    restitution: 0.5
    friction: 0.4
    damping: 0.2
    solver_iterations: 10
    edge_compliance: 0.001
    volume_compliance: 0.01
)"));

    ASSERT_EQ(result.soft_body_descs.size(), 1u);
    const auto& desc = result.soft_body_descs[0];
    EXPECT_EQ(desc.grid_resolution, 8);
    EXPECT_DOUBLE_EQ(desc.grid_spacing, 0.3);
    EXPECT_DOUBLE_EQ(desc.position.x(), 1.0);
    EXPECT_DOUBLE_EQ(desc.position.y(), 5.0);
    EXPECT_DOUBLE_EQ(desc.position.z(), -2.0);
    EXPECT_DOUBLE_EQ(desc.pressure, 3000.0);
    EXPECT_DOUBLE_EQ(desc.restitution, 0.5);
    EXPECT_DOUBLE_EQ(desc.friction, 0.4);
    EXPECT_DOUBLE_EQ(desc.damping, 0.2);
    EXPECT_EQ(desc.solver_iterations, 10);
    EXPECT_DOUBLE_EQ(desc.edge_compliance, 0.001);
    EXPECT_DOUBLE_EQ(desc.volume_compliance, 0.01);
    EXPECT_NE(desc.material, nullptr);
}

TEST_F(YamlSoftBodyParsingTest, UsesDefaultsWhenOptionalFieldsOmitted) {
    auto result = loader.load(make_scene(R"(
  - type: soft_body_cube
    material: jelly
    position: [0, 3, 0]
)"));

    ASSERT_EQ(result.soft_body_descs.size(), 1u);
    const auto& desc = result.soft_body_descs[0];
    EXPECT_EQ(desc.grid_resolution, 5);
    EXPECT_DOUBLE_EQ(desc.grid_spacing, 0.5);
    EXPECT_DOUBLE_EQ(desc.pressure, 2000.0);
    EXPECT_DOUBLE_EQ(desc.restitution, 0.3);
    EXPECT_DOUBLE_EQ(desc.friction, 0.2);
    EXPECT_DOUBLE_EQ(desc.damping, 0.1);
    EXPECT_EQ(desc.solver_iterations, 5);
    EXPECT_DOUBLE_EQ(desc.edge_compliance, 1e-4);
    EXPECT_DOUBLE_EQ(desc.volume_compliance, 0.0);
}

TEST_F(YamlSoftBodyParsingTest, SoftBodyCubeDoesNotAddShapeOrPhysicsToScene) {
    auto result = loader.load(make_scene(R"(
  - type: soft_body_cube
    material: jelly
    position: [0, 5, 0]
)"));

    EXPECT_EQ(result.scene.shapes().size(), 0u);
    EXPECT_EQ(result.shape_physics.size(), 0u);
    EXPECT_EQ(result.soft_body_descs.size(), 1u);
}

TEST_F(YamlSoftBodyParsingTest, MixedSceneCarriesBothShapesAndSoftBodies) {
    auto result = loader.load(make_scene(R"(
  - type: sphere
    center: [0, 0.5, 0]
    radius: 0.5
    material: jelly
  - type: soft_body_cube
    material: jelly
    position: [0, 5, 0]
  - type: sphere
    center: [2, 0.5, 0]
    radius: 0.3
    material: jelly
)"));

    EXPECT_EQ(result.scene.shapes().size(), 2u);
    EXPECT_EQ(result.shape_physics.size(), 2u);
    EXPECT_EQ(result.soft_body_descs.size(), 1u);
}

static std::string project_root() {
    std::string file_path = __FILE__;
    // __FILE__ is .../tests/infrastructure/yaml_soft_body_parsing_test.cpp
    // Go up 2 directories to reach the project root
    auto pos = file_path.rfind('/');
    if (pos != std::string::npos) pos = file_path.rfind('/', pos - 1);
    if (pos != std::string::npos) pos = file_path.rfind('/', pos - 1);
    return (pos != std::string::npos) ? file_path.substr(0, pos + 1) : "";
}

class YamlSoftBodyExistingScenesTest : public ::testing::TestWithParam<std::string> {
protected:
    YamlSceneLoader loader;
};

TEST_P(YamlSoftBodyExistingScenesTest, ExistingSceneParsesWithEmptySoftBodyDescs) {
    std::string scene_path = project_root() + GetParam();
    std::ifstream file(scene_path);
    ASSERT_TRUE(file.is_open()) << "Cannot open scene file: " << scene_path;
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    auto result = loader.load(content);

    EXPECT_TRUE(result.soft_body_descs.empty());
    EXPECT_FALSE(result.scene.shapes().empty());
}

INSTANTIATE_TEST_SUITE_P(
    ExistingScenes,
    YamlSoftBodyExistingScenesTest,
    ::testing::Values(
        "scenes/simple.yaml",
        "scenes/gpu_showcase.yaml"
    )
);

TEST(YamlSoftBodyBowlingScene, BowlingSceneContainsSoftBody) {
    std::string scene_path = project_root() + "scenes/nwave_bowling.yaml";
    std::ifstream file(scene_path);
    ASSERT_TRUE(file.is_open()) << "Cannot open scene file: " << scene_path;
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    YamlSceneLoader loader;
    auto result = loader.load(content);

    ASSERT_EQ(result.soft_body_descs.size(), 1u);
    EXPECT_FALSE(result.scene.shapes().empty());
}
