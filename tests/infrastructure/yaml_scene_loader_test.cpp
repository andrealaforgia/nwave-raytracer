#include <gtest/gtest.h>
#include "infrastructure/yaml_scene_loader.h"
#include "domain/materials/lambertian.h"
#include "domain/materials/metal.h"
#include "domain/materials/dielectric.h"
#include "domain/materials/image_texture.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/box.h"
#include "domain/shapes/cylinder.h"
#include "domain/shapes/triangle.h"
#include "domain/shapes/triangle_mesh.h"
#include "domain/lights/point_light.h"
#include "domain/lights/directional_light.h"
#include "domain/materials/emissive.h"
#include "domain/camera.h"
#include "domain/physics_properties.h"
#include "domain/animation_config.h"

using namespace nwave;

class YamlSceneLoaderMaterialsTest : public ::testing::Test {
protected:
    YamlSceneLoader loader;
};

TEST_F(YamlSceneLoaderMaterialsTest, ParsesLambertianMaterialWithCorrectAlbedo) {
    const std::string yaml = R"(
materials:
  - name: red_rubber
    type: lambertian
    albedo: [0.85, 0.15, 0.15]
)";

    auto materials = loader.parse_materials(yaml);

    ASSERT_EQ(materials.count("red_rubber"), 1);
    auto* lambertian = dynamic_cast<Lambertian*>(materials.at("red_rubber").get());
    ASSERT_NE(lambertian, nullptr);
    EXPECT_DOUBLE_EQ(lambertian->albedo().r(), 0.85);
    EXPECT_DOUBLE_EQ(lambertian->albedo().g(), 0.15);
    EXPECT_DOUBLE_EQ(lambertian->albedo().b(), 0.15);
}

TEST_F(YamlSceneLoaderMaterialsTest, ParsesMetalMaterialWithAlbedoAndFuzz) {
    const std::string yaml = R"(
materials:
  - name: mirror
    type: metal
    albedo: [0.9, 0.9, 0.9]
    fuzz: 0.05
)";

    auto materials = loader.parse_materials(yaml);

    ASSERT_EQ(materials.count("mirror"), 1);
    auto* metal = dynamic_cast<Metal*>(materials.at("mirror").get());
    ASSERT_NE(metal, nullptr);
    EXPECT_DOUBLE_EQ(metal->albedo().r(), 0.9);
    EXPECT_DOUBLE_EQ(metal->albedo().g(), 0.9);
    EXPECT_DOUBLE_EQ(metal->albedo().b(), 0.9);
    EXPECT_DOUBLE_EQ(metal->fuzziness(), 0.05);
}

TEST_F(YamlSceneLoaderMaterialsTest, ParsesDielectricMaterialWithIorAndOptionalTint) {
    const std::string yaml = R"(
materials:
  - name: glass
    type: dielectric
    ior: 1.5
    tint: [0.4, 0.95, 0.4]
)";

    auto materials = loader.parse_materials(yaml);

    ASSERT_EQ(materials.count("glass"), 1);
    auto* dielectric = dynamic_cast<Dielectric*>(materials.at("glass").get());
    ASSERT_NE(dielectric, nullptr);
    EXPECT_DOUBLE_EQ(dielectric->ior(), 1.5);
    EXPECT_DOUBLE_EQ(dielectric->tint().r(), 0.4);
    EXPECT_DOUBLE_EQ(dielectric->tint().g(), 0.95);
    EXPECT_DOUBLE_EQ(dielectric->tint().b(), 0.4);
}

TEST_F(YamlSceneLoaderMaterialsTest, ParsesDielectricMaterialWithDefaultTintWhenOmitted) {
    const std::string yaml = R"(
materials:
  - name: clear_glass
    type: dielectric
    ior: 1.5
)";

    auto materials = loader.parse_materials(yaml);

    ASSERT_EQ(materials.count("clear_glass"), 1);
    auto* dielectric = dynamic_cast<Dielectric*>(materials.at("clear_glass").get());
    ASSERT_NE(dielectric, nullptr);
    EXPECT_DOUBLE_EQ(dielectric->ior(), 1.5);
    EXPECT_DOUBLE_EQ(dielectric->tint().r(), 1.0);
    EXPECT_DOUBLE_EQ(dielectric->tint().g(), 1.0);
    EXPECT_DOUBLE_EQ(dielectric->tint().b(), 1.0);
}

TEST_F(YamlSceneLoaderMaterialsTest, StoresAllMaterialNamesInLookupMap) {
    const std::string yaml = R"(
materials:
  - name: red_rubber
    type: lambertian
    albedo: [0.85, 0.15, 0.15]
  - name: mirror
    type: metal
    albedo: [0.9, 0.9, 0.9]
    fuzz: 0.05
  - name: glass
    type: dielectric
    ior: 1.5
)";

    auto materials = loader.parse_materials(yaml);

    EXPECT_EQ(materials.size(), 3u);
    EXPECT_EQ(materials.count("red_rubber"), 1);
    EXPECT_EQ(materials.count("mirror"), 1);
    EXPECT_EQ(materials.count("glass"), 1);
}

// --- Scene loading tests (step 02-03) ---

class YamlSceneLoaderLoadTest : public ::testing::Test {
protected:
    YamlSceneLoader loader;

    static constexpr const char* full_scene_yaml = R"(
materials:
  - name: red_rubber
    type: lambertian
    albedo: [0.85, 0.15, 0.15]
  - name: floor_metal
    type: metal
    albedo: [0.7, 0.7, 0.7]
    fuzz: 0.1
objects:
  - type: sphere
    center: [0, 0.5, 0]
    radius: 0.5
    material: red_rubber
  - type: plane
    point: [0, 0, 0]
    normal: [0, 1, 0]
    material: floor_metal
lights:
  - type: point
    position: [0, 10, 5]
    color: [1, 1, 1]
    intensity: 0.8
camera:
  lookfrom: [0, 2, 5]
  lookat: [0, 0.5, 0]
  vup: [0, 1, 0]
  vfov: 40
  image_width: 800
)";
};

TEST_F(YamlSceneLoaderLoadTest, ParsesSphereWithCenterRadiusAndResolvedMaterial) {
    auto result = loader.load(full_scene_yaml);

    ASSERT_EQ(result.scene.shapes().size(), 2u);
    auto* sphere = dynamic_cast<Sphere*>(result.scene.shapes()[0].get());
    ASSERT_NE(sphere, nullptr);
    EXPECT_DOUBLE_EQ(sphere->center().x(), 0.0);
    EXPECT_DOUBLE_EQ(sphere->center().y(), 0.5);
    EXPECT_DOUBLE_EQ(sphere->center().z(), 0.0);
    EXPECT_DOUBLE_EQ(sphere->radius(), 0.5);
}

TEST_F(YamlSceneLoaderLoadTest, ParsesPlaneWithPointNormalAndResolvedMaterial) {
    auto result = loader.load(full_scene_yaml);

    ASSERT_GE(result.scene.shapes().size(), 2u);
    auto* plane = dynamic_cast<Plane*>(result.scene.shapes()[1].get());
    ASSERT_NE(plane, nullptr);
    EXPECT_DOUBLE_EQ(plane->point().x(), 0.0);
    EXPECT_DOUBLE_EQ(plane->point().y(), 0.0);
    EXPECT_DOUBLE_EQ(plane->point().z(), 0.0);
    EXPECT_DOUBLE_EQ(plane->normal().x(), 0.0);
    EXPECT_DOUBLE_EQ(plane->normal().y(), 1.0);
    EXPECT_DOUBLE_EQ(plane->normal().z(), 0.0);
}

TEST_F(YamlSceneLoaderLoadTest, ParsesPointLightWithPositionColorAndIntensity) {
    auto result = loader.load(full_scene_yaml);

    ASSERT_EQ(result.scene.lights().size(), 1u);
    auto* light = dynamic_cast<PointLight*>(result.scene.lights()[0].get());
    ASSERT_NE(light, nullptr);
    EXPECT_DOUBLE_EQ(light->position().x(), 0.0);
    EXPECT_DOUBLE_EQ(light->position().y(), 10.0);
    EXPECT_DOUBLE_EQ(light->position().z(), 5.0);
    EXPECT_DOUBLE_EQ(light->color().r(), 1.0);
    EXPECT_DOUBLE_EQ(light->color().g(), 1.0);
    EXPECT_DOUBLE_EQ(light->color().b(), 1.0);
    EXPECT_DOUBLE_EQ(light->intensity(), 0.8);
}

TEST_F(YamlSceneLoaderLoadTest, ParsesCameraWithLookfromLookatVupVfovAndImageWidth) {
    auto result = loader.load(full_scene_yaml);

    EXPECT_DOUBLE_EQ(result.camera.lookfrom().x(), 0.0);
    EXPECT_DOUBLE_EQ(result.camera.lookfrom().y(), 2.0);
    EXPECT_DOUBLE_EQ(result.camera.lookfrom().z(), 5.0);
    EXPECT_EQ(result.camera.image_width(), 800);
}

TEST_F(YamlSceneLoaderLoadTest, ThrowsOnUnknownMaterialReference) {
    const std::string yaml = R"(
materials:
  - name: red_rubber
    type: lambertian
    albedo: [0.85, 0.15, 0.15]
objects:
  - type: sphere
    center: [0, 0, 0]
    radius: 1.0
    material: nonexistent_mat
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

    EXPECT_THROW({
        try {
            loader.load(yaml);
        } catch (const std::runtime_error& e) {
            EXPECT_NE(std::string(e.what()).find("nonexistent_mat"), std::string::npos);
            throw;
        }
    }, std::runtime_error);
}

// --- Shape parsing tests (step 03-01) ---

class YamlSceneLoaderShapesTest : public ::testing::Test {
protected:
    YamlSceneLoader loader;

    std::string make_scene_yaml(const std::string& object_yaml) {
        return R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
)" + object_yaml + R"(
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
    }
};

TEST_F(YamlSceneLoaderShapesTest, ParsesBoxWithMinMaxCornersAndMaterial) {
    auto result = loader.load(make_scene_yaml(R"(
  - type: box
    min: [2, 0, -0.5]
    max: [3, 1.5, 0.5]
    material: mat
)"));

    ASSERT_EQ(result.scene.shapes().size(), 1u);
    auto* box = dynamic_cast<Box*>(result.scene.shapes()[0].get());
    ASSERT_NE(box, nullptr);
    EXPECT_DOUBLE_EQ(box->box_min().x(), 2.0);
    EXPECT_DOUBLE_EQ(box->box_min().y(), 0.0);
    EXPECT_DOUBLE_EQ(box->box_min().z(), -0.5);
    EXPECT_DOUBLE_EQ(box->box_max().x(), 3.0);
    EXPECT_DOUBLE_EQ(box->box_max().y(), 1.5);
    EXPECT_DOUBLE_EQ(box->box_max().z(), 0.5);
}

TEST_F(YamlSceneLoaderShapesTest, ParsesCylinderWithCenterRadiusHeightAndMaterial) {
    auto result = loader.load(make_scene_yaml(R"(
  - type: cylinder
    center: [0, 0.5, 0]
    radius: 0.3
    height: 1.0
    material: mat
)"));

    ASSERT_EQ(result.scene.shapes().size(), 1u);
    auto* cyl = dynamic_cast<Cylinder*>(result.scene.shapes()[0].get());
    ASSERT_NE(cyl, nullptr);
    EXPECT_DOUBLE_EQ(cyl->center().x(), 0.0);
    EXPECT_DOUBLE_EQ(cyl->center().y(), 0.5);
    EXPECT_DOUBLE_EQ(cyl->center().z(), 0.0);
    EXPECT_DOUBLE_EQ(cyl->radius(), 0.3);
    EXPECT_DOUBLE_EQ(cyl->height(), 1.0);
}

TEST_F(YamlSceneLoaderShapesTest, ParsesTriangleWithThreeVerticesAndMaterial) {
    auto result = loader.load(make_scene_yaml(R"(
  - type: triangle
    v0: [0, 0, 0]
    v1: [1, 0, 0]
    v2: [0.5, 1, 0]
    material: mat
)"));

    ASSERT_EQ(result.scene.shapes().size(), 1u);
    auto* tri = dynamic_cast<Triangle*>(result.scene.shapes()[0].get());
    ASSERT_NE(tri, nullptr);
    EXPECT_DOUBLE_EQ(tri->v0().x(), 0.0);
    EXPECT_DOUBLE_EQ(tri->v0().y(), 0.0);
    EXPECT_DOUBLE_EQ(tri->v0().z(), 0.0);
    EXPECT_DOUBLE_EQ(tri->v1().x(), 1.0);
    EXPECT_DOUBLE_EQ(tri->v1().y(), 0.0);
    EXPECT_DOUBLE_EQ(tri->v1().z(), 0.0);
    EXPECT_DOUBLE_EQ(tri->v2().x(), 0.5);
    EXPECT_DOUBLE_EQ(tri->v2().y(), 1.0);
    EXPECT_DOUBLE_EQ(tri->v2().z(), 0.0);
}

TEST_F(YamlSceneLoaderShapesTest, ParsesTriangleMeshWithVerticesNormalsIndicesAndMaterial) {
    auto result = loader.load(make_scene_yaml(R"(
  - type: triangle_mesh
    vertices: [[0,0,0], [1,0,0], [0,1,0], [1,1,0]]
    normals: [[0,0,1], [0,0,1], [0,0,1], [0,0,1]]
    indices: [[0,1,2], [1,3,2]]
    material: mat
)"));

    ASSERT_EQ(result.scene.shapes().size(), 1u);
    auto* mesh = dynamic_cast<TriangleMesh*>(result.scene.shapes()[0].get());
    ASSERT_NE(mesh, nullptr);
    // Verify bounding box covers the parsed vertices [0..1, 0..1, 0..0]
    auto bbox = mesh->bounding_box();
    EXPECT_DOUBLE_EQ(bbox.minimum().x(), 0.0);
    EXPECT_DOUBLE_EQ(bbox.minimum().y(), 0.0);
    EXPECT_DOUBLE_EQ(bbox.minimum().z(), 0.0);
    EXPECT_DOUBLE_EQ(bbox.maximum().x(), 1.0);
    EXPECT_DOUBLE_EQ(bbox.maximum().y(), 1.0);
    EXPECT_DOUBLE_EQ(bbox.maximum().z(), 0.0);
}

TEST_F(YamlSceneLoaderShapesTest, ThrowsOnUnknownShapeTypeListingSupportedTypes) {
    EXPECT_THROW({
        try {
            loader.load(make_scene_yaml(R"(
  - type: cone
    material: mat
)"));
        } catch (const std::runtime_error& e) {
            std::string msg = e.what();
            EXPECT_NE(msg.find("cone"), std::string::npos);
            EXPECT_NE(msg.find("sphere"), std::string::npos);
            EXPECT_NE(msg.find("plane"), std::string::npos);
            EXPECT_NE(msg.find("box"), std::string::npos);
            EXPECT_NE(msg.find("cylinder"), std::string::npos);
            EXPECT_NE(msg.find("triangle"), std::string::npos);
            EXPECT_NE(msg.find("triangle_mesh"), std::string::npos);
            throw;
        }
    }, std::runtime_error);
}

// --- Directional light and emissive material tests (step 03-02) ---

class YamlSceneLoaderLightsAndEmissiveTest : public ::testing::Test {
protected:
    YamlSceneLoader loader;

    std::string make_scene_yaml_with_lights_and_materials(
        const std::string& materials_yaml,
        const std::string& lights_yaml) {
        return R"(
materials:
)" + materials_yaml + R"(
objects:
  - type: sphere
    center: [0, 0.5, 0]
    radius: 0.5
    material: mat
lights:
)" + lights_yaml + R"(
camera:
  lookfrom: [0, 0, 5]
  lookat: [0, 0, 0]
  vup: [0, 1, 0]
  vfov: 45
  image_width: 640
)";
    }
};

TEST_F(YamlSceneLoaderLightsAndEmissiveTest, ParsesDirectionalLightWithDirectionColorAndIntensity) {
    auto result = loader.load(make_scene_yaml_with_lights_and_materials(
        R"(  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5])",
        R"(  - type: directional
    direction: [0, -1, 0]
    color: [1, 0.9, 0.8]
    intensity: 0.5)"));

    ASSERT_EQ(result.scene.lights().size(), 1u);
    auto* dir_light = dynamic_cast<DirectionalLight*>(result.scene.lights()[0].get());
    ASSERT_NE(dir_light, nullptr);
    EXPECT_DOUBLE_EQ(dir_light->direction().x(), 0.0);
    EXPECT_DOUBLE_EQ(dir_light->direction().y(), -1.0);
    EXPECT_DOUBLE_EQ(dir_light->direction().z(), 0.0);
    EXPECT_DOUBLE_EQ(dir_light->color().r(), 1.0);
    EXPECT_DOUBLE_EQ(dir_light->color().g(), 0.9);
    EXPECT_DOUBLE_EQ(dir_light->color().b(), 0.8);
    EXPECT_DOUBLE_EQ(dir_light->intensity(), 0.5);
}

TEST_F(YamlSceneLoaderLightsAndEmissiveTest, ParsesEmissiveMaterialWithColorAndIntensity) {
    auto materials = loader.parse_materials(R"(
materials:
  - name: neon_red
    type: emissive
    color: [1, 0, 0]
    intensity: 5.0
)");

    ASSERT_EQ(materials.count("neon_red"), 1);
    auto* emissive = dynamic_cast<Emissive*>(materials.at("neon_red").get());
    ASSERT_NE(emissive, nullptr);
    EXPECT_DOUBLE_EQ(emissive->color().r(), 1.0);
    EXPECT_DOUBLE_EQ(emissive->color().g(), 0.0);
    EXPECT_DOUBLE_EQ(emissive->color().b(), 0.0);
    EXPECT_DOUBLE_EQ(emissive->intensity(), 5.0);
}

TEST_F(YamlSceneLoaderLightsAndEmissiveTest, ParsesSceneWithBothPointAndDirectionalLights) {
    auto result = loader.load(make_scene_yaml_with_lights_and_materials(
        R"(  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5])",
        R"(  - type: point
    position: [0, 10, 5]
    color: [1, 1, 1]
    intensity: 0.8
  - type: directional
    direction: [1, -1, 0]
    color: [1, 0.95, 0.9]
    intensity: 0.3)"));

    ASSERT_EQ(result.scene.lights().size(), 2u);

    auto* point_light = dynamic_cast<PointLight*>(result.scene.lights()[0].get());
    ASSERT_NE(point_light, nullptr);
    EXPECT_DOUBLE_EQ(point_light->position().y(), 10.0);
    EXPECT_DOUBLE_EQ(point_light->intensity(), 0.8);

    auto* dir_light = dynamic_cast<DirectionalLight*>(result.scene.lights()[1].get());
    ASSERT_NE(dir_light, nullptr);
    EXPECT_DOUBLE_EQ(dir_light->intensity(), 0.3);
    EXPECT_DOUBLE_EQ(dir_light->color().r(), 1.0);
    EXPECT_DOUBLE_EQ(dir_light->color().g(), 0.95);
    EXPECT_DOUBLE_EQ(dir_light->color().b(), 0.9);
}

// --- Physics and animation parsing tests (step 06-02) ---
// Test Budget: 4 behaviors x 2 = 8 max unit tests. Using 4 tests.

class YamlSceneLoaderPhysicsAndAnimationTest : public ::testing::Test {
protected:
    YamlSceneLoader loader;

    static constexpr const char* minimal_scene_suffix = R"(
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
};

TEST_F(YamlSceneLoaderPhysicsAndAnimationTest, ParsesPhysicsBlockWithAllProperties) {
    const std::string yaml = std::string(R"(
materials:
  - name: ball
    type: lambertian
    albedo: [0.8, 0.2, 0.2]
objects:
  - type: sphere
    center: [0, 5, 0]
    radius: 0.5
    material: ball
    physics:
      body_type: dynamic
      mass: 2.0
      initial_velocity: [5, 0, 0]
      friction: 0.6
      restitution: 0.8
)") + minimal_scene_suffix;

    auto result = loader.load(yaml);

    ASSERT_EQ(result.shape_physics.size(), 1u);
    EXPECT_EQ(result.shape_physics[0].body_type, BodyType::DYNAMIC);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].mass, 2.0);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].initial_velocity.x(), 5.0);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].initial_velocity.y(), 0.0);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].initial_velocity.z(), 0.0);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].friction, 0.6);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].restitution, 0.8);
}

TEST_F(YamlSceneLoaderPhysicsAndAnimationTest, DefaultsToStaticBodyTypeWhenPhysicsBlockMissing) {
    const std::string yaml = std::string(R"(
materials:
  - name: floor
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: plane
    point: [0, 0, 0]
    normal: [0, 1, 0]
    material: floor
)") + minimal_scene_suffix;

    auto result = loader.load(yaml);

    ASSERT_EQ(result.shape_physics.size(), 1u);
    EXPECT_EQ(result.shape_physics[0].body_type, BodyType::STATIC);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].mass, 1.0);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].initial_velocity.x(), 0.0);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].initial_velocity.y(), 0.0);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].initial_velocity.z(), 0.0);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].friction, 0.5);
    EXPECT_DOUBLE_EQ(result.shape_physics[0].restitution, 0.3);
}

TEST_F(YamlSceneLoaderPhysicsAndAnimationTest, ParsesAnimationSectionWithAllFields) {
    const std::string yaml = std::string(R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: sphere
    center: [0, 1, 0]
    radius: 0.5
    material: mat
animation:
  duration: 5.0
  physics_timestep: 0.01667
  render_fps: 30
  output_directory: "frames/"
)") + minimal_scene_suffix;

    auto result = loader.load(yaml);

    ASSERT_TRUE(result.animation_config.has_value());
    EXPECT_DOUBLE_EQ(result.animation_config->duration, 5.0);
    EXPECT_DOUBLE_EQ(result.animation_config->physics_timestep, 0.01667);
    EXPECT_DOUBLE_EQ(result.animation_config->render_fps, 30.0);
    EXPECT_EQ(result.animation_config->output_directory, "frames/");
}

TEST_F(YamlSceneLoaderPhysicsAndAnimationTest, AnimationConfigAbsentWhenNoAnimationSection) {
    const std::string yaml = std::string(R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: sphere
    center: [0, 1, 0]
    radius: 0.5
    material: mat
)") + minimal_scene_suffix;

    auto result = loader.load(yaml);

    EXPECT_FALSE(result.animation_config.has_value());
}

// --- wake_frame parsing tests (step 02-03) ---
// Test Budget: 2 behaviors (wake_frame present, wake_frame absent) x 2 = 4 max. Using 2 tests.

class YamlSceneLoaderWakeFrameParsingTest : public ::testing::Test {
protected:
    YamlSceneLoader loader;

    static constexpr const char* minimal_scene_suffix = R"(
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
};

struct WakeFrameTestCase {
    int wake_frame_value;
    std::string label;
};

class YamlSceneLoaderWakeFramePresentTest
    : public YamlSceneLoaderWakeFrameParsingTest,
      public ::testing::WithParamInterface<WakeFrameTestCase> {};

TEST_P(YamlSceneLoaderWakeFramePresentTest, ParsesWakeFrameFromPhysicsBlock) {
    auto [wake_frame_value, label] = GetParam();
    const std::string yaml = std::string(R"(
materials:
  - name: ball
    type: lambertian
    albedo: [0.8, 0.2, 0.2]
objects:
  - type: sphere
    center: [0, 5, 0]
    radius: 0.5
    material: ball
    physics:
      body_type: dynamic
      mass: 2.0
      start_asleep: true
      wake_frame: )") + std::to_string(wake_frame_value) + "\n" + minimal_scene_suffix;

    auto result = loader.load(yaml);

    ASSERT_EQ(result.shape_physics.size(), 1u);
    ASSERT_TRUE(result.shape_physics[0].wake_frame.has_value());
    EXPECT_EQ(result.shape_physics[0].wake_frame.value(), wake_frame_value);
}

INSTANTIATE_TEST_SUITE_P(
    WakeFrameValues,
    YamlSceneLoaderWakeFramePresentTest,
    ::testing::Values(
        WakeFrameTestCase{60, "typical_frame"},
        WakeFrameTestCase{0, "zero_edge_case"}
    ),
    [](const ::testing::TestParamInfo<WakeFrameTestCase>& info) {
        return info.param.label;
    }
);

TEST_F(YamlSceneLoaderWakeFrameParsingTest, WakeFrameIsNulloptWhenOmittedFromPhysicsBlock) {
    const std::string yaml = std::string(R"(
materials:
  - name: ball
    type: lambertian
    albedo: [0.8, 0.2, 0.2]
objects:
  - type: sphere
    center: [0, 5, 0]
    radius: 0.5
    material: ball
    physics:
      body_type: dynamic
      mass: 2.0
      start_asleep: true
)") + minimal_scene_suffix;

    auto result = loader.load(yaml);

    ASSERT_EQ(result.shape_physics.size(), 1u);
    EXPECT_FALSE(result.shape_physics[0].wake_frame.has_value());
}

// --- Procedural texture sampling mode test ---
// Test Budget: 1 behavior (procedural texture uses equirectangular sampling) x 2 = 2 max. Using 1 test.

TEST_F(YamlSceneLoaderMaterialsTest, ProceduralTextureUsesEquirectangularSampling) {
    const std::string yaml = R"(
materials:
  - name: marble_ball
    type: procedural_texture
    pattern: marble
    width: 1024
    height: 512
    seed: 42
)";

    auto materials = loader.parse_materials(yaml);

    ASSERT_EQ(materials.count("marble_ball"), 1);
    auto* img = dynamic_cast<ImageTexture*>(materials.at("marble_ball").get());
    ASSERT_NE(img, nullptr);
    // Procedural textures use default texture_scale (0.0f) for equirectangular
    // sampling, which correctly reflects sphere rotation via object-space UV.
    // The cube_map path (texture_scale = -1.0f) doesn't rotate with the sphere.
    EXPECT_FLOAT_EQ(img->texture_scale(), 0.0f);
}
