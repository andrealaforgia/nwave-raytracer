#include <gtest/gtest.h>
#include "infrastructure/yaml_scene_loader.h"
#include "domain/materials/lambertian.h"
#include "domain/materials/metal.h"
#include "domain/materials/dielectric.h"

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
