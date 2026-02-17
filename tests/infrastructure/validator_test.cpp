#include <gtest/gtest.h>
#include "infrastructure/validator.h"
#include <algorithm>

using namespace nwave;

// --- Acceptance test: scene validator collects all errors in single pass ---

TEST(SceneValidatorAcceptance, CollectsAllErrorsInSinglePassAndSuggestsMisspelledMaterials) {
    // Scene with multiple problems:
    // - No camera
    // - Object references misspelled material "geen_glass" (should suggest "green_glass")
    // - Object has negative mass
    const std::string yaml = R"(
materials:
  - name: green_glass
    type: dielectric
    ior: 1.5
  - name: red_rubber
    type: lambertian
    albedo: [0.8, 0.1, 0.1]
objects:
  - type: sphere
    name: ball
    center: [0, 1, 0]
    radius: 1.0
    material: geen_glass
    physics:
      mass: -5.0
  - type: sphere
    name: other_ball
    center: [2, 1, 0]
    radius: 0.5
    material: red_rubber
lights:
  - type: point
    position: [0, 10, 0]
    color: [1, 1, 1]
    intensity: 1.0
)";

    SceneValidator validator;
    auto result = validator.validate(yaml);

    // Must not be valid
    EXPECT_FALSE(result.is_valid);

    // Must contain at least 3 errors: missing camera, bad material ref, negative mass
    ASSERT_GE(result.errors.size(), 3u);

    // Check camera-required error
    auto has_camera_error = std::any_of(result.errors.begin(), result.errors.end(),
        [](const ValidationError& e) { return e.message.find("camera") != std::string::npos; });
    EXPECT_TRUE(has_camera_error) << "Expected camera-required error";

    // Check negative mass error with object name and value
    auto has_mass_error = std::any_of(result.errors.begin(), result.errors.end(),
        [](const ValidationError& e) {
            return e.message.find("ball") != std::string::npos &&
                   e.message.find("-5") != std::string::npos &&
                   e.message.find("mass") != std::string::npos;
        });
    EXPECT_TRUE(has_mass_error) << "Expected negative mass error with object name and value";

    // Check misspelled material suggestion
    auto has_suggestion = std::any_of(result.errors.begin(), result.errors.end(),
        [](const ValidationError& e) {
            return e.message.find("geen_glass") != std::string::npos &&
                   e.message.find("green_glass") != std::string::npos;
        });
    EXPECT_TRUE(has_suggestion) << "Expected misspelled material to suggest green_glass";
}

// --- Unit tests through driving port (SceneValidator::validate) ---

class SceneValidatorTest : public ::testing::Test {
protected:
    SceneValidator validator;

    static constexpr const char* valid_scene_yaml = R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: sphere
    name: ball
    center: [0, 0.5, 0]
    radius: 0.5
    material: mat
lights:
  - type: point
    position: [0, 10, 0]
    color: [1, 1, 1]
    intensity: 1.0
camera:
  lookfrom: [0, 2, 5]
  lookat: [0, 0, 0]
  vup: [0, 1, 0]
  vfov: 45
  image_width: 640
)";
};

TEST_F(SceneValidatorTest, ValidScenePassesAllChecks) {
    auto result = validator.validate(valid_scene_yaml);

    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

TEST_F(SceneValidatorTest, ReportsMissingCamera) {
    const std::string yaml = R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: sphere
    name: ball
    center: [0, 0, 0]
    radius: 1.0
    material: mat
lights:
  - type: point
    position: [0, 10, 0]
    color: [1, 1, 1]
    intensity: 1.0
)";

    auto result = validator.validate(yaml);

    EXPECT_FALSE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].message.find("camera"), std::string::npos);
}

TEST_F(SceneValidatorTest, ReportsNegativeMassWithObjectNameAndValue) {
    const std::string yaml = R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: sphere
    name: heavy_ball
    center: [0, 0, 0]
    radius: 1.0
    material: mat
    physics:
      mass: -3.5
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

    auto result = validator.validate(yaml);

    EXPECT_FALSE(result.is_valid);
    ASSERT_GE(result.errors.size(), 1u);
    auto has_mass_error = std::any_of(result.errors.begin(), result.errors.end(),
        [](const ValidationError& e) {
            return e.message.find("heavy_ball") != std::string::npos &&
                   e.message.find("-3.5") != std::string::npos &&
                   e.message.find("mass") != std::string::npos;
        });
    EXPECT_TRUE(has_mass_error);
}

TEST_F(SceneValidatorTest, ReportsMultipleErrorsInSinglePass) {
    // Missing camera AND missing lights
    const std::string yaml = R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: sphere
    name: ball
    center: [0, 0, 0]
    radius: 1.0
    material: mat
)";

    auto result = validator.validate(yaml);

    EXPECT_FALSE(result.is_valid);
    EXPECT_GE(result.errors.size(), 2u) << "Expected at least 2 errors (missing camera, missing lights)";
}

TEST_F(SceneValidatorTest, SuggestsMisspelledMaterialWithinEditDistance2) {
    const std::string yaml = R"(
materials:
  - name: green_glass
    type: dielectric
    ior: 1.5
objects:
  - type: sphere
    name: ball
    center: [0, 0, 0]
    radius: 1.0
    material: geen_glass
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

    auto result = validator.validate(yaml);

    EXPECT_FALSE(result.is_valid);
    ASSERT_GE(result.errors.size(), 1u);
    auto has_suggestion = std::any_of(result.errors.begin(), result.errors.end(),
        [](const ValidationError& e) {
            return e.message.find("geen_glass") != std::string::npos &&
                   e.message.find("green_glass") != std::string::npos;
        });
    EXPECT_TRUE(has_suggestion) << "Expected suggestion for misspelled material";
}

struct MissingStructureCase {
    std::string name;
    std::string yaml;
    std::string expected_error_fragment;
};

class SceneValidatorMissingStructureTest
    : public ::testing::TestWithParam<MissingStructureCase> {
protected:
    SceneValidator validator;
};

TEST_P(SceneValidatorMissingStructureTest, ReportsMissingStructuralElement) {
    auto result = validator.validate(GetParam().yaml);
    EXPECT_FALSE(result.is_valid);
    auto has_error = std::any_of(result.errors.begin(), result.errors.end(),
        [&](const ValidationError& e) {
            return e.message.find(GetParam().expected_error_fragment) != std::string::npos;
        });
    EXPECT_TRUE(has_error) << "Expected error containing: " << GetParam().expected_error_fragment;
}

INSTANTIATE_TEST_SUITE_P(StructureChecks, SceneValidatorMissingStructureTest,
    ::testing::Values(
        MissingStructureCase{"missing_objects", R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
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
)", "object"},
        MissingStructureCase{"missing_lights", R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: sphere
    name: ball
    center: [0, 0, 0]
    radius: 1.0
    material: mat
camera:
  lookfrom: [0, 0, 5]
  lookat: [0, 0, 0]
  vup: [0, 1, 0]
  vfov: 45
  image_width: 640
)", "light"}
    ),
    [](const ::testing::TestParamInfo<MissingStructureCase>& info) {
        return info.param.name;
    }
);

struct NegativeParameterCase {
    std::string name;
    std::string yaml;
    std::string expected_fragment;
};

class SceneValidatorNegativeParamTest
    : public ::testing::TestWithParam<NegativeParameterCase> {
protected:
    SceneValidator validator;
};

TEST_P(SceneValidatorNegativeParamTest, ReportsNegativeParameterValue) {
    auto result = validator.validate(GetParam().yaml);
    EXPECT_FALSE(result.is_valid);
    auto has_error = std::any_of(result.errors.begin(), result.errors.end(),
        [&](const ValidationError& e) {
            return e.message.find(GetParam().expected_fragment) != std::string::npos;
        });
    EXPECT_TRUE(has_error) << "Expected error containing: " << GetParam().expected_fragment;
}

INSTANTIATE_TEST_SUITE_P(ParameterRangeChecks, SceneValidatorNegativeParamTest,
    ::testing::Values(
        NegativeParameterCase{"negative_radius", R"(
materials:
  - name: mat
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
objects:
  - type: sphere
    name: bad_sphere
    center: [0, 0, 0]
    radius: -1.0
    material: mat
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
)", "radius"}
    ),
    [](const ::testing::TestParamInfo<NegativeParameterCase>& info) {
        return info.param.name;
    }
);
