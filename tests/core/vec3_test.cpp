#include <gtest/gtest.h>
#include "core/vec3.h"
#include "core/math_utils.h"
#include <cmath>

using namespace nwave;

// --- Construction and access ---

TEST(Vec3Test, DefaultConstructionYieldsZeroVector) {
    Vec3 v;
    EXPECT_DOUBLE_EQ(v.x(), 0.0);
    EXPECT_DOUBLE_EQ(v.y(), 0.0);
    EXPECT_DOUBLE_EQ(v.z(), 0.0);
}

TEST(Vec3Test, ConstructionWithValues) {
    Vec3 v(1.0, 2.0, 3.0);
    EXPECT_DOUBLE_EQ(v.x(), 1.0);
    EXPECT_DOUBLE_EQ(v.y(), 2.0);
    EXPECT_DOUBLE_EQ(v.z(), 3.0);
}

TEST(Vec3Test, ColorAccessorsMatchXYZ) {
    Vec3 v(0.1, 0.2, 0.3);
    EXPECT_DOUBLE_EQ(v.r(), v.x());
    EXPECT_DOUBLE_EQ(v.g(), v.y());
    EXPECT_DOUBLE_EQ(v.b(), v.z());
}

TEST(Vec3Test, IndexOperatorAccess) {
    Vec3 v(4.0, 5.0, 6.0);
    EXPECT_DOUBLE_EQ(v[0], 4.0);
    EXPECT_DOUBLE_EQ(v[1], 5.0);
    EXPECT_DOUBLE_EQ(v[2], 6.0);
}

// --- Type aliases ---

TEST(Vec3Test, Point3AndColor3AreVec3Aliases) {
    Point3 p(1.0, 2.0, 3.0);
    Color3 c(0.5, 0.6, 0.7);
    Vec3 v = p;
    EXPECT_DOUBLE_EQ(v.x(), 1.0);
    v = c;
    EXPECT_DOUBLE_EQ(v.x(), 0.5);
}

// --- Arithmetic operators ---

TEST(Vec3Test, Addition) {
    Vec3 a(1.0, 2.0, 3.0);
    Vec3 b(4.0, 5.0, 6.0);
    Vec3 c = a + b;
    EXPECT_DOUBLE_EQ(c.x(), 5.0);
    EXPECT_DOUBLE_EQ(c.y(), 7.0);
    EXPECT_DOUBLE_EQ(c.z(), 9.0);
}

TEST(Vec3Test, Subtraction) {
    Vec3 a(5.0, 7.0, 9.0);
    Vec3 b(1.0, 2.0, 3.0);
    Vec3 c = a - b;
    EXPECT_DOUBLE_EQ(c.x(), 4.0);
    EXPECT_DOUBLE_EQ(c.y(), 5.0);
    EXPECT_DOUBLE_EQ(c.z(), 6.0);
}

TEST(Vec3Test, UnaryNegation) {
    Vec3 v(1.0, -2.0, 3.0);
    Vec3 neg = -v;
    EXPECT_DOUBLE_EQ(neg.x(), -1.0);
    EXPECT_DOUBLE_EQ(neg.y(), 2.0);
    EXPECT_DOUBLE_EQ(neg.z(), -3.0);
}

TEST(Vec3Test, ScalarMultiplication) {
    Vec3 v(1.0, 2.0, 3.0);
    Vec3 r1 = v * 2.0;
    EXPECT_DOUBLE_EQ(r1.x(), 2.0);
    EXPECT_DOUBLE_EQ(r1.y(), 4.0);
    EXPECT_DOUBLE_EQ(r1.z(), 6.0);

    Vec3 r2 = 3.0 * v;
    EXPECT_DOUBLE_EQ(r2.x(), 3.0);
    EXPECT_DOUBLE_EQ(r2.y(), 6.0);
    EXPECT_DOUBLE_EQ(r2.z(), 9.0);
}

TEST(Vec3Test, ComponentWiseMultiplication) {
    Vec3 a(2.0, 3.0, 4.0);
    Vec3 b(5.0, 6.0, 7.0);
    Vec3 c = a * b;
    EXPECT_DOUBLE_EQ(c.x(), 10.0);
    EXPECT_DOUBLE_EQ(c.y(), 18.0);
    EXPECT_DOUBLE_EQ(c.z(), 28.0);
}

TEST(Vec3Test, ScalarDivision) {
    Vec3 v(4.0, 6.0, 8.0);
    Vec3 r = v / 2.0;
    EXPECT_DOUBLE_EQ(r.x(), 2.0);
    EXPECT_DOUBLE_EQ(r.y(), 3.0);
    EXPECT_DOUBLE_EQ(r.z(), 4.0);
}

TEST(Vec3Test, ComponentWiseDivision) {
    Vec3 a(10.0, 12.0, 14.0);
    Vec3 b(2.0, 3.0, 7.0);
    Vec3 c = a / b;
    EXPECT_DOUBLE_EQ(c.x(), 5.0);
    EXPECT_DOUBLE_EQ(c.y(), 4.0);
    EXPECT_DOUBLE_EQ(c.z(), 2.0);
}

// --- Compound assignment operators ---

TEST(Vec3Test, CompoundAssignmentOperators) {
    Vec3 v(1.0, 2.0, 3.0);

    v += Vec3(1.0, 1.0, 1.0);
    EXPECT_DOUBLE_EQ(v.x(), 2.0);
    EXPECT_DOUBLE_EQ(v.y(), 3.0);
    EXPECT_DOUBLE_EQ(v.z(), 4.0);

    v -= Vec3(0.5, 0.5, 0.5);
    EXPECT_DOUBLE_EQ(v.x(), 1.5);
    EXPECT_DOUBLE_EQ(v.y(), 2.5);
    EXPECT_DOUBLE_EQ(v.z(), 3.5);

    v *= 2.0;
    EXPECT_DOUBLE_EQ(v.x(), 3.0);
    EXPECT_DOUBLE_EQ(v.y(), 5.0);
    EXPECT_DOUBLE_EQ(v.z(), 7.0);

    v /= 2.0;
    EXPECT_DOUBLE_EQ(v.x(), 1.5);
    EXPECT_DOUBLE_EQ(v.y(), 2.5);
    EXPECT_DOUBLE_EQ(v.z(), 3.5);
}

// --- Geometric functions ---

TEST(Vec3Test, DotProduct) {
    Vec3 a(1.0, 2.0, 3.0);
    Vec3 b(4.0, 5.0, 6.0);
    EXPECT_DOUBLE_EQ(dot(a, b), 32.0);
}

TEST(Vec3Test, CrossProduct) {
    Vec3 a(1.0, 0.0, 0.0);
    Vec3 b(0.0, 1.0, 0.0);
    Vec3 c = cross(a, b);
    EXPECT_DOUBLE_EQ(c.x(), 0.0);
    EXPECT_DOUBLE_EQ(c.y(), 0.0);
    EXPECT_DOUBLE_EQ(c.z(), 1.0);
}

TEST(Vec3Test, CrossProductAnticommutative) {
    Vec3 a(1.0, 2.0, 3.0);
    Vec3 b(4.0, 5.0, 6.0);
    Vec3 ab = cross(a, b);
    Vec3 ba = cross(b, a);
    EXPECT_DOUBLE_EQ(ab.x(), -ba.x());
    EXPECT_DOUBLE_EQ(ab.y(), -ba.y());
    EXPECT_DOUBLE_EQ(ab.z(), -ba.z());
}

TEST(Vec3Test, LengthAndLengthSquared) {
    Vec3 v(3.0, 4.0, 0.0);
    EXPECT_DOUBLE_EQ(v.length_squared(), 25.0);
    EXPECT_DOUBLE_EQ(v.length(), 5.0);
}

TEST(Vec3Test, Normalize) {
    Vec3 v(3.0, 4.0, 0.0);
    Vec3 n = normalize(v);
    EXPECT_NEAR(n.length(), 1.0, 1e-10);
    EXPECT_DOUBLE_EQ(n.x(), 0.6);
    EXPECT_DOUBLE_EQ(n.y(), 0.8);
    EXPECT_DOUBLE_EQ(n.z(), 0.0);
}

TEST(Vec3Test, NearZero) {
    Vec3 tiny(1e-9, -1e-9, 1e-9);
    EXPECT_TRUE(tiny.near_zero());

    Vec3 not_tiny(1.0, 0.0, 0.0);
    EXPECT_FALSE(not_tiny.near_zero());
}

// --- Random vectors ---

TEST(Vec3Test, RandomInUnitSphereHasLengthLessThanOne) {
    for (int i = 0; i < 100; ++i) {
        Vec3 v = random_in_unit_sphere();
        EXPECT_LT(v.length_squared(), 1.0);
    }
}

TEST(Vec3Test, RandomUnitVectorHasUnitLength) {
    for (int i = 0; i < 100; ++i) {
        Vec3 v = random_unit_vector();
        EXPECT_NEAR(v.length(), 1.0, 1e-10);
    }
}

TEST(Vec3Test, RandomInUnitDiskHasZComponentZeroAndLengthLessThanOne) {
    for (int i = 0; i < 100; ++i) {
        Vec3 v = random_in_unit_disk();
        EXPECT_DOUBLE_EQ(v.z(), 0.0);
        EXPECT_LT(v.x() * v.x() + v.y() * v.y(), 1.0);
    }
}

TEST(Vec3Test, StaticRandomGeneratesInRange) {
    for (int i = 0; i < 100; ++i) {
        Vec3 v = Vec3::random();
        EXPECT_GE(v.x(), 0.0);
        EXPECT_LT(v.x(), 1.0);
        EXPECT_GE(v.y(), 0.0);
        EXPECT_LT(v.y(), 1.0);
        EXPECT_GE(v.z(), 0.0);
        EXPECT_LT(v.z(), 1.0);
    }
}

TEST(Vec3Test, StaticRandomWithRangeGeneratesInRange) {
    for (int i = 0; i < 100; ++i) {
        Vec3 v = Vec3::random(-5.0, 5.0);
        EXPECT_GE(v.x(), -5.0);
        EXPECT_LT(v.x(), 5.0);
        EXPECT_GE(v.y(), -5.0);
        EXPECT_LT(v.y(), 5.0);
        EXPECT_GE(v.z(), -5.0);
        EXPECT_LT(v.z(), 5.0);
    }
}

// --- Math utilities tested through Vec3 usage ---

TEST(MathUtilsTest, PiConstant) {
    EXPECT_NEAR(nwave::pi, 3.14159265358979323846, 1e-15);
}

TEST(MathUtilsTest, InfinityConstant) {
    EXPECT_TRUE(std::isinf(nwave::infinity));
    EXPECT_GT(nwave::infinity, 0.0);
}

TEST(MathUtilsTest, EpsilonConstant) {
    EXPECT_DOUBLE_EQ(nwave::epsilon, 0.001);
}

TEST(MathUtilsTest, DegreesToRadians) {
    EXPECT_NEAR(degrees_to_radians(180.0), nwave::pi, 1e-15);
    EXPECT_NEAR(degrees_to_radians(90.0), nwave::pi / 2.0, 1e-15);
    EXPECT_DOUBLE_EQ(degrees_to_radians(0.0), 0.0);
}

TEST(MathUtilsTest, Clamp) {
    EXPECT_DOUBLE_EQ(clamp(0.5, 0.0, 1.0), 0.5);
    EXPECT_DOUBLE_EQ(clamp(-1.0, 0.0, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(clamp(2.0, 0.0, 1.0), 1.0);
}

TEST(MathUtilsTest, RandomDoubleInZeroToOne) {
    for (int i = 0; i < 100; ++i) {
        double r = random_double();
        EXPECT_GE(r, 0.0);
        EXPECT_LT(r, 1.0);
    }
}

TEST(MathUtilsTest, RandomDoubleInRange) {
    for (int i = 0; i < 100; ++i) {
        double r = random_double(-10.0, 10.0);
        EXPECT_GE(r, -10.0);
        EXPECT_LT(r, 10.0);
    }
}
