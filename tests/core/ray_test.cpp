#include <gtest/gtest.h>
#include "core/ray.h"

using namespace nwave;

TEST(RayTest, ConstructionStoresOriginAndDirection) {
    Point3 origin(1.0, 2.0, 3.0);
    Vec3 direction(4.0, 5.0, 6.0);
    Ray r(origin, direction);

    EXPECT_DOUBLE_EQ(r.origin().x(), 1.0);
    EXPECT_DOUBLE_EQ(r.origin().y(), 2.0);
    EXPECT_DOUBLE_EQ(r.origin().z(), 3.0);
    EXPECT_DOUBLE_EQ(r.direction().x(), 4.0);
    EXPECT_DOUBLE_EQ(r.direction().y(), 5.0);
    EXPECT_DOUBLE_EQ(r.direction().z(), 6.0);
}

struct RayAtParam {
    double t;
    double expected_x;
    double expected_y;
    double expected_z;
};

class RayAtTest : public ::testing::TestWithParam<RayAtParam> {};

TEST_P(RayAtTest, AtReturnsCorrectPoint) {
    Point3 origin(2.0, 3.0, 4.0);
    Vec3 direction(1.0, 0.0, 0.0);
    Ray r(origin, direction);

    auto [t, ex, ey, ez] = GetParam();
    Point3 p = r.at(t);
    EXPECT_DOUBLE_EQ(p.x(), ex);
    EXPECT_DOUBLE_EQ(p.y(), ey);
    EXPECT_DOUBLE_EQ(p.z(), ez);
}

INSTANTIATE_TEST_SUITE_P(
    RayAtCases,
    RayAtTest,
    ::testing::Values(
        RayAtParam{0.0, 2.0, 3.0, 4.0},   // t=0 returns origin
        RayAtParam{1.0, 3.0, 3.0, 4.0},    // t=1 positive
        RayAtParam{-1.0, 1.0, 3.0, 4.0},   // t=-1 negative
        RayAtParam{2.5, 4.5, 3.0, 4.0}     // t=2.5 fractional
    )
);
