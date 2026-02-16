#include <gtest/gtest.h>
#include "domain/materials/metal.h"
#include "domain/hit_record.h"
#include "core/ray.h"
#include "core/vec3.h"
#include <cmath>

using namespace nwave;

// Helper: create a HitRecord at a known point with a given normal
static HitRecord make_hit_record(const Point3& point, const Vec3& normal) {
    HitRecord rec;
    rec.point = point;
    rec.normal = normal;
    rec.front_face = true;
    rec.t = 1.0;
    return rec;
}

TEST(MetalTest, MirrorReflectionDirectionForZeroFuzziness) {
    // Ray coming in at 45 degrees from above-left, hitting a surface with upward normal
    // v = (1, -1, 0), n = (0, 1, 0)
    // reflect(v, n) = v - 2*dot(v,n)*n = (1,-1,0) - 2*(-1)*(0,1,0) = (1,1,0)
    Metal mat(Color3(0.8, 0.8, 0.8), 0.0);
    Vec3 incoming_dir(1, -1, 0);
    Ray ray_in(Point3(0, 5, 0), incoming_dir);
    HitRecord rec = make_hit_record(Point3(1, 0, 0), Vec3(0, 1, 0));

    Color3 attenuation;
    Ray scattered;
    bool result = mat.scatter(ray_in, rec, attenuation, scattered);

    ASSERT_TRUE(result);

    // The scatter function normalizes incoming direction first, so:
    // normalize(1,-1,0) = (1/sqrt2, -1/sqrt2, 0)
    // reflect = (1/sqrt2, -1/sqrt2, 0) - 2*(-1/sqrt2)*(0,1,0) = (1/sqrt2, 1/sqrt2, 0)
    Vec3 expected = normalize(Vec3(1, 1, 0));
    Vec3 dir = normalize(scattered.direction());

    EXPECT_NEAR(dir.x(), expected.x(), 1e-9);
    EXPECT_NEAR(dir.y(), expected.y(), 1e-9);
    EXPECT_NEAR(dir.z(), expected.z(), 1e-9);
}

TEST(MetalTest, AttenuationEqualsAlbedo) {
    Color3 albedo(0.9, 0.1, 0.3);
    Metal mat(albedo, 0.0);
    Ray ray_in(Point3(0, 5, 0), Vec3(0, -1, 0));
    HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, 1, 0));

    Color3 attenuation;
    Ray scattered;
    mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_DOUBLE_EQ(attenuation.r(), 0.9);
    EXPECT_DOUBLE_EQ(attenuation.g(), 0.1);
    EXPECT_DOUBLE_EQ(attenuation.b(), 0.3);
}

TEST(MetalTest, ScatteredRayOriginIsHitPoint) {
    Metal mat(Color3(0.8, 0.8, 0.8), 0.0);
    Point3 hit_point(3, 2, 1);
    Ray ray_in(Point3(0, 5, 0), Vec3(0, -1, 0));
    HitRecord rec = make_hit_record(hit_point, Vec3(0, 1, 0));

    Color3 attenuation;
    Ray scattered;
    mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_DOUBLE_EQ(scattered.origin().x(), 3.0);
    EXPECT_DOUBLE_EQ(scattered.origin().y(), 2.0);
    EXPECT_DOUBLE_EQ(scattered.origin().z(), 1.0);
}

using FuzzClampParam = std::tuple<double, double>;

class MetalFuzzinessClampTest : public ::testing::TestWithParam<FuzzClampParam> {};

TEST_P(MetalFuzzinessClampTest, FuzzinessIsClampedToZeroOne) {
    auto [input_fuzz, expected_fuzz] = GetParam();
    Metal mat(Color3(0.8, 0.8, 0.8), input_fuzz);
    EXPECT_DOUBLE_EQ(mat.fuzziness(), expected_fuzz);
}

INSTANTIATE_TEST_SUITE_P(
    FuzzClampCases,
    MetalFuzzinessClampTest,
    ::testing::Values(
        FuzzClampParam{1.5, 1.0},
        FuzzClampParam{-0.1, 0.0},
        FuzzClampParam{0.5, 0.5},
        FuzzClampParam{0.0, 0.0},
        FuzzClampParam{1.0, 1.0}
    )
);

TEST(MetalTest, ScatterReturnsFalseWhenReflectedBelowSurface) {
    // A ray that, after reflection + fuzz, would go below the surface.
    // We construct this by having fuzziness=1.0 and running many trials.
    // But for a deterministic test: use fuzziness=0 with a ray whose
    // reflected direction would be exactly along the surface (dot=0).
    // Actually, with fuzziness=0 and normal incidence, reflected goes straight back up.
    // Better approach: we test the logic by checking the return value when
    // the normal is opposite to the natural reflection direction.
    // Use a HitRecord with a normal pointing DOWN but the hit says front_face=true.
    // Ray going down (0,-1,0), normal (0,-1,0): reflect = (0,-1,0) - 2*(1)*(0,-1,0) = (0,1,0)
    // dot((0,1,0), (0,-1,0)) = -1 < 0, so scatter returns false.
    Metal mat(Color3(0.8, 0.8, 0.8), 0.0);
    Ray ray_in(Point3(0, 5, 0), Vec3(0, -1, 0));
    // Normal points down -- reflected ray goes up but dot with normal is negative
    HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, -1, 0));

    Color3 attenuation;
    Ray scattered;
    bool result = mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_FALSE(result);
}

TEST(MetalTest, AccessorsReturnConstructorValues) {
    Color3 albedo(0.3, 0.6, 0.9);
    double fuzziness = 0.4;
    Metal mat(albedo, fuzziness);

    EXPECT_DOUBLE_EQ(mat.albedo().r(), 0.3);
    EXPECT_DOUBLE_EQ(mat.albedo().g(), 0.6);
    EXPECT_DOUBLE_EQ(mat.albedo().b(), 0.9);
    EXPECT_DOUBLE_EQ(mat.fuzziness(), 0.4);
}
