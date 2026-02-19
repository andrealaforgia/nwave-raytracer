#include <gtest/gtest.h>
#include "core/matrix4x4.h"
#include "core/quaternion.h"
#include "core/math_utils.h"
#include <cmath>

using namespace nwave;

// --- AC1: identity() leaves points unchanged under transform_point ---

TEST(Matrix4x4Test, IdentityLeavesPointUnchanged) {
    Matrix4x4 I = Matrix4x4::identity();
    Point3 p(3.0, -7.0, 42.0);
    Point3 result = I.transform_point(p);

    EXPECT_NEAR(result.x(), 3.0, 1e-10);
    EXPECT_NEAR(result.y(), -7.0, 1e-10);
    EXPECT_NEAR(result.z(), 42.0, 1e-10);
}

// --- AC2: translation(1,2,3) shifts point by (1,2,3) ---

TEST(Matrix4x4Test, TranslationShiftsPointByOffset) {
    Matrix4x4 T = Matrix4x4::translation(1.0, 2.0, 3.0);
    Point3 p(10.0, 20.0, 30.0);
    Point3 result = T.transform_point(p);

    EXPECT_NEAR(result.x(), 11.0, 1e-10);
    EXPECT_NEAR(result.y(), 22.0, 1e-10);
    EXPECT_NEAR(result.z(), 33.0, 1e-10);
}

// --- AC3: from_translation_rotation with 90-deg Y rotation produces correct matrix ---

TEST(Matrix4x4Test, FromTranslationRotation90DegYRotationTransformsCorrectly) {
    Vec3 pos(5.0, 0.0, 0.0);
    Quaternion rot = Quaternion::from_axis_angle(Vec3(0, 1, 0), degrees_to_radians(90.0));
    Matrix4x4 M = Matrix4x4::from_translation_rotation(pos, rot);

    // Transforming origin should give translation: (5, 0, 0)
    Point3 origin(0.0, 0.0, 0.0);
    Point3 translated = M.transform_point(origin);
    EXPECT_NEAR(translated.x(), 5.0, 1e-10);
    EXPECT_NEAR(translated.y(), 0.0, 1e-10);
    EXPECT_NEAR(translated.z(), 0.0, 1e-10);

    // Transforming (1, 0, 0): 90-deg Y rotation maps (1,0,0) -> (0,0,-1), then translate by (5,0,0)
    Point3 px(1.0, 0.0, 0.0);
    Point3 rotated = M.transform_point(px);
    EXPECT_NEAR(rotated.x(), 5.0, 1e-10);
    EXPECT_NEAR(rotated.y(), 0.0, 1e-10);
    EXPECT_NEAR(rotated.z(), -1.0, 1e-10);
}

// --- AC4: M * M.inverse() approximates identity within 1e-10 ---

TEST(Matrix4x4Test, MatrixTimesInverseApproximatesIdentity) {
    Vec3 pos(3.0, -1.0, 7.0);
    Quaternion rot = Quaternion::from_axis_angle(normalize(Vec3(1, 1, 0)), degrees_to_radians(37.0));
    Matrix4x4 M = Matrix4x4::from_translation_rotation(pos, rot);
    Matrix4x4 I = M * M.inverse();
    Matrix4x4 eye = Matrix4x4::identity();

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            EXPECT_NEAR(I.m[r][c], eye.m[r][c], 1e-10)
                << "Mismatch at (" << r << "," << c << ")";
        }
    }
}

// --- AC5: transform_normal uses inverse-transpose (verified with non-uniform scale) ---

TEST(Matrix4x4Test, TransformNormalUsesInverseTransposeUnderNonUniformScale) {
    // Construct a non-uniform scale matrix: scale x by 2, y by 1, z by 1
    // Under non-uniform scale, normals must be transformed by inverse-transpose
    // to remain perpendicular to the surface.
    Matrix4x4 scale = Matrix4x4::identity();
    scale.m[0][0] = 2.0;  // scale x by 2
    scale.m[1][1] = 1.0;
    scale.m[2][2] = 1.0;

    // A surface normal pointing at 45 degrees between x and y: (1, 1, 0) normalized
    Vec3 normal = normalize(Vec3(1.0, 1.0, 0.0));

    // The corresponding surface tangent is (-1, 1, 0) normalized
    Vec3 tangent = normalize(Vec3(-1.0, 1.0, 0.0));

    // Transform both
    Vec3 transformed_normal = scale.transform_normal(normal);
    Vec3 transformed_tangent = scale.transform_vector(tangent);

    // The transformed normal should be perpendicular to the transformed tangent
    double dot_product = dot(transformed_normal, transformed_tangent);
    EXPECT_NEAR(dot_product, 0.0, 1e-10);

    // Without inverse-transpose (naive transform_vector), the normal would NOT be perpendicular
    // Verify the naive approach would fail:
    Vec3 naive_normal = scale.transform_vector(normal);
    double naive_dot = dot(naive_normal, transformed_tangent);
    EXPECT_GT(std::fabs(naive_dot), 0.1)
        << "Naive transform should break perpendicularity under non-uniform scale";
}

// --- Additional: transform_vector ignores translation ---

TEST(Matrix4x4Test, TransformVectorIgnoresTranslation) {
    Matrix4x4 T = Matrix4x4::translation(100.0, 200.0, 300.0);
    Vec3 v(1.0, 0.0, 0.0);
    Vec3 result = T.transform_vector(v);

    EXPECT_NEAR(result.x(), 1.0, 1e-10);
    EXPECT_NEAR(result.y(), 0.0, 1e-10);
    EXPECT_NEAR(result.z(), 0.0, 1e-10);
}

// --- Additional: transpose swaps rows and columns ---

TEST(Matrix4x4Test, TransposeSwapsRowsAndColumns) {
    Matrix4x4 M = Matrix4x4::identity();
    M.m[0][3] = 5.0;  // row 0, col 3
    M.m[3][0] = 9.0;  // row 3, col 0

    Matrix4x4 Mt = M.transpose();
    EXPECT_NEAR(Mt.m[3][0], 5.0, 1e-10);  // was row 0, col 3
    EXPECT_NEAR(Mt.m[0][3], 9.0, 1e-10);  // was row 3, col 0
}

// --- Additional: matrix multiplication ---

TEST(Matrix4x4Test, MultiplyTwoTranslationsAddOffsets) {
    Matrix4x4 T1 = Matrix4x4::translation(1.0, 0.0, 0.0);
    Matrix4x4 T2 = Matrix4x4::translation(0.0, 2.0, 0.0);
    Matrix4x4 combined = T1 * T2;

    Point3 p(0.0, 0.0, 0.0);
    Point3 result = combined.transform_point(p);

    EXPECT_NEAR(result.x(), 1.0, 1e-10);
    EXPECT_NEAR(result.y(), 2.0, 1e-10);
    EXPECT_NEAR(result.z(), 0.0, 1e-10);
}
