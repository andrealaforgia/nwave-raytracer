#include <gtest/gtest.h>
#include "core/quaternion.h"
#include "core/math_utils.h"
#include <cmath>

using namespace nwave;

// --- AC1: identity() returns (0, 0, 0, 1) ---

TEST(QuaternionTest, IdentityReturnsZeroXYZAndOneW) {
    Quaternion q = Quaternion::identity();
    EXPECT_DOUBLE_EQ(q.x(), 0.0);
    EXPECT_DOUBLE_EQ(q.y(), 0.0);
    EXPECT_DOUBLE_EQ(q.z(), 0.0);
    EXPECT_DOUBLE_EQ(q.w(), 1.0);
}

// --- AC2: from_axis_angle(Y, 90deg) produces correct quaternion ---

TEST(QuaternionTest, FromAxisAngleYAxis90DegreesProducesCorrectComponents) {
    double angle = degrees_to_radians(90.0);
    Quaternion q = Quaternion::from_axis_angle(Vec3(0, 1, 0), angle);

    double expected_s = std::sin(angle / 2.0);
    double expected_c = std::cos(angle / 2.0);

    EXPECT_NEAR(q.x(), 0.0, 1e-10);
    EXPECT_NEAR(q.y(), expected_s, 1e-10);
    EXPECT_NEAR(q.z(), 0.0, 1e-10);
    EXPECT_NEAR(q.w(), expected_c, 1e-10);
}

// --- AC3: conjugate negates xyz, preserves w ---

TEST(QuaternionTest, ConjugateNegatesXYZAndPreservesW) {
    Quaternion q = Quaternion::from_axis_angle(Vec3(1, 0, 0), degrees_to_radians(45.0));
    Quaternion c = q.conjugate();

    EXPECT_DOUBLE_EQ(c.x(), -q.x());
    EXPECT_DOUBLE_EQ(c.y(), -q.y());
    EXPECT_DOUBLE_EQ(c.z(), -q.z());
    EXPECT_DOUBLE_EQ(c.w(), q.w());
}

// --- AC4: normalized() returns unit-length quaternion ---

TEST(QuaternionTest, NormalizedReturnsUnitLengthQuaternion) {
    // Construct a non-unit quaternion manually
    Quaternion q(2.0, 3.0, 4.0, 5.0);
    Quaternion n = q.normalized();

    double length = std::sqrt(n.x() * n.x() + n.y() * n.y() + n.z() * n.z() + n.w() * n.w());
    EXPECT_NEAR(length, 1.0, 1e-10);
}

// --- AC5: Hamilton product of two 90-degree rotations equals 180-degree rotation ---

TEST(QuaternionTest, HamiltonProductOfTwo90DegreeRotationsEquals180Degrees) {
    double angle_90 = degrees_to_radians(90.0);
    Quaternion q90 = Quaternion::from_axis_angle(Vec3(0, 1, 0), angle_90);
    Quaternion q180 = q90 * q90;

    // A 180-degree rotation about Y: (0, sin(90), 0, cos(90)) = (0, 1, 0, 0)
    EXPECT_NEAR(q180.x(), 0.0, 1e-10);
    EXPECT_NEAR(q180.y(), 1.0, 1e-10);
    EXPECT_NEAR(q180.z(), 0.0, 1e-10);
    EXPECT_NEAR(q180.w(), 0.0, 1e-10);
}

// --- to_matrix: rotation matrix from quaternion ---

TEST(QuaternionTest, ToMatrixIdentityQuaternionProducesIdentityRotation) {
    Quaternion q = Quaternion::identity();
    auto m = q.to_matrix();

    // 3x3 identity rotation
    EXPECT_NEAR(m[0], 1.0, 1e-10);
    EXPECT_NEAR(m[1], 0.0, 1e-10);
    EXPECT_NEAR(m[2], 0.0, 1e-10);
    EXPECT_NEAR(m[3], 0.0, 1e-10);
    EXPECT_NEAR(m[4], 1.0, 1e-10);
    EXPECT_NEAR(m[5], 0.0, 1e-10);
    EXPECT_NEAR(m[6], 0.0, 1e-10);
    EXPECT_NEAR(m[7], 0.0, 1e-10);
    EXPECT_NEAR(m[8], 1.0, 1e-10);
}

TEST(QuaternionTest, ToMatrix90DegreeYRotationTransformsXToNegZ) {
    // Rotating X-axis by 90 degrees about Y should give -Z
    Quaternion q = Quaternion::from_axis_angle(Vec3(0, 1, 0), degrees_to_radians(90.0));
    auto m = q.to_matrix();

    // Column-major interpretation for rotating point (1,0,0):
    // new_x = m[0]*1 + m[1]*0 + m[2]*0 = m[0]
    // new_y = m[3]*1 + m[4]*0 + m[5]*0 = m[3]
    // new_z = m[6]*1 + m[7]*0 + m[8]*0 = m[6]
    // 90 deg Y rotation of (1,0,0) -> (0, 0, -1)
    // Using row-major: result = (m[0], m[3], m[6]) for input (1,0,0)
    // Row-major layout: row0=(m[0],m[1],m[2]), row1=(m[3],m[4],m[5]), row2=(m[6],m[7],m[8])
    // For right-handed rotation about Y by 90 deg:
    // (1,0,0) -> (cos90, 0, -sin90) = (0, 0, -1)
    // So m[0]=0, m[3]=0, m[6]=-1
    EXPECT_NEAR(m[0], 0.0, 1e-10);   // row0 col0
    EXPECT_NEAR(m[3], 0.0, 1e-10);   // row1 col0
    EXPECT_NEAR(m[6], -1.0, 1e-10);  // row2 col0
}

// --- slerp: spherical linear interpolation ---

TEST(QuaternionTest, SlerpAtZeroReturnsFirstQuaternion) {
    Quaternion a = Quaternion::identity();
    Quaternion b = Quaternion::from_axis_angle(Vec3(0, 1, 0), degrees_to_radians(90.0));
    Quaternion result = Quaternion::slerp(a, b, 0.0);

    EXPECT_NEAR(result.x(), a.x(), 1e-10);
    EXPECT_NEAR(result.y(), a.y(), 1e-10);
    EXPECT_NEAR(result.z(), a.z(), 1e-10);
    EXPECT_NEAR(result.w(), a.w(), 1e-10);
}

TEST(QuaternionTest, SlerpAtOneReturnsSecondQuaternion) {
    Quaternion a = Quaternion::identity();
    Quaternion b = Quaternion::from_axis_angle(Vec3(0, 1, 0), degrees_to_radians(90.0));
    Quaternion result = Quaternion::slerp(a, b, 1.0);

    EXPECT_NEAR(result.x(), b.x(), 1e-10);
    EXPECT_NEAR(result.y(), b.y(), 1e-10);
    EXPECT_NEAR(result.z(), b.z(), 1e-10);
    EXPECT_NEAR(result.w(), b.w(), 1e-10);
}

TEST(QuaternionTest, SlerpAtHalfProduces45DegreeRotation) {
    Quaternion a = Quaternion::identity();
    Quaternion b = Quaternion::from_axis_angle(Vec3(0, 1, 0), degrees_to_radians(90.0));
    Quaternion result = Quaternion::slerp(a, b, 0.5);

    // Halfway between 0 and 90 degrees about Y is 45 degrees
    Quaternion expected = Quaternion::from_axis_angle(Vec3(0, 1, 0), degrees_to_radians(45.0));
    EXPECT_NEAR(result.x(), expected.x(), 1e-10);
    EXPECT_NEAR(result.y(), expected.y(), 1e-10);
    EXPECT_NEAR(result.z(), expected.z(), 1e-10);
    EXPECT_NEAR(result.w(), expected.w(), 1e-10);
}
