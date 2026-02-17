#include <gtest/gtest.h>
#include "infrastructure/yaml_scene_loader.h"
#include "domain/materials/lambertian.h"
#include "domain/materials/metal.h"
#include "domain/materials/dielectric.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/lights/point_light.h"
#include "domain/camera.h"

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
