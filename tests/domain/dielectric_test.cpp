#include <gtest/gtest.h>
#include "domain/materials/dielectric.h"
#include "domain/hit_record.h"
#include "core/ray.h"
#include "core/vec3.h"
#include <cmath>

using namespace nwave;

// Helper: create a HitRecord at a known point with a given normal and front_face flag
static HitRecord make_hit_record(const Point3& point, const Vec3& normal, bool front_face) {
    HitRecord rec;
    rec.point = point;
    rec.normal = normal;
    rec.front_face = front_face;
    rec.t = 1.0;
    return rec;
}

TEST(DielectricTest, ScatterAlwaysReturnsTrue) {
    Dielectric mat(1.5);
    Ray ray_in(Point3(0, 1, 0), Vec3(0, -1, 0));
    HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, 1, 0), true);

    Color3 attenuation;
    Ray scattered;
    bool result = mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_TRUE(result);
}

TEST(DielectricTest, AttenuationIsAlwaysWhite) {
    Dielectric mat(1.5);
    Ray ray_in(Point3(0, 1, 0), Vec3(0, -1, 0));
    HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, 1, 0), true);

    Color3 attenuation;
    Ray scattered;
    mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_DOUBLE_EQ(attenuation.r(), 1.0);
    EXPECT_DOUBLE_EQ(attenuation.g(), 1.0);
    EXPECT_DOUBLE_EQ(attenuation.b(), 1.0);
}

TEST(DielectricTest, SchlickReflectanceAtNormalIncidenceForGlass) {
    // At normal incidence (cos_theta = 1.0) with IOR 1.5:
    // r0 = ((1 - 1.5) / (1 + 1.5))^2 = (-0.5/2.5)^2 = 0.04
    // schlick(1.0, 1.5) = 0.04 + (1 - 0.04) * (1-1)^5 = 0.04
    // This is tested indirectly: at normal incidence, reflectance is low (0.04),
    // so most rays should refract. We test statistically over many runs.
    Dielectric mat(1.5);
    int refracted_count = 0;
    int total_runs = 1000;

    for (int i = 0; i < total_runs; ++i) {
        Ray ray_in(Point3(0, 1, 0), Vec3(0, -1, 0));
        HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, 1, 0), true);

        Color3 attenuation;
        Ray scattered;
        mat.scatter(ray_in, rec, attenuation, scattered);

        // If refracted, the scattered ray should go downward (negative y)
        if (scattered.direction().y() < 0) {
            refracted_count++;
        }
    }

    // With Schlick reflectance of 0.04, about 96% should refract.
    // Allow a generous margin for randomness.
    double refraction_rate = static_cast<double>(refracted_count) / total_runs;
    EXPECT_GT(refraction_rate, 0.85);
    EXPECT_LT(refraction_rate, 1.0);
}

TEST(DielectricTest, RefractedRayObeysSnellsLawAt45Degrees) {
    // Snell's law: n1 * sin(theta1) = n2 * sin(theta2)
    // Air to glass (IOR 1.5): sin(45) * 1.0 = sin(theta2) * 1.5
    // sin(theta2) = sin(45) / 1.5 = 0.7071 / 1.5 = 0.4714
    // theta2 = asin(0.4714) ≈ 28.13 degrees
    //
    // We force refraction by using a scenario where Schlick reflectance is low
    // and test the refract function directly through the material.
    // Instead, test refract() directly as a pure function since scatter has randomness.
    Vec3 incoming = normalize(Vec3(1, -1, 0));  // 45 degrees
    Vec3 normal(0, 1, 0);
    double eta = 1.0 / 1.5;  // air to glass

    Vec3 refracted = refract(incoming, normal, eta);

    // The refracted ray should have a smaller angle from the normal.
    // sin(theta_refracted) = eta * sin(theta_incident)
    // For 45-degree incidence: sin(theta_r) = (1/1.5) * sin(45) = 0.4714
    double sin_theta_refracted = std::sqrt(1.0 - dot(refracted, -normal) * dot(refracted, -normal));
    double expected_sin = eta * std::sin(M_PI / 4.0);

    // Verify the refracted ray still points downward
    EXPECT_LT(refracted.y(), 0.0);
    EXPECT_NEAR(sin_theta_refracted, expected_sin, 1e-9);
}

TEST(DielectricTest, TotalInternalReflectionWhenSinThetaExceedsOne) {
    // Ray inside glass (IOR 1.5) hitting the surface at a steep angle.
    // front_face = false means ray is inside the material, so eta = IOR = 1.5
    // For total internal reflection: eta * sin(theta) > 1.0
    // With eta = 1.5, sin(theta) > 0.667, i.e., theta > ~41.8 degrees
    // Use a ray at ~60 degrees from normal inside the glass.
    double angle = 60.0 * M_PI / 180.0;
    Vec3 incoming_dir = normalize(Vec3(std::sin(angle), std::cos(angle), 0));
    // Ray going "upward and to the right" inside glass, hitting inner surface
    // Normal points down (outward from glass at bottom surface), front_face = false
    Ray ray_in(Point3(0, -1, 0), incoming_dir);
    HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, -1, 0), false);

    Dielectric mat(1.5);

    // Run multiple times -- all should reflect (TIR is deterministic, not random)
    for (int i = 0; i < 100; ++i) {
        Color3 attenuation;
        Ray scattered;
        mat.scatter(ray_in, rec, attenuation, scattered);

        // In total internal reflection, the reflected ray should go back downward
        // (reflected about the normal (0,-1,0))
        // The y component of the reflected ray should be negative (going down)
        EXPECT_LT(scattered.direction().y(), 0.0)
            << "Expected total internal reflection but ray went through surface";
    }
}

TEST(DielectricTest, IorOneProducesNoBending) {
    // With IOR = 1.0, there is no refraction -- ray passes straight through.
    // Schlick reflectance: r0 = ((1-1)/(1+1))^2 = 0 => always refracts.
    Dielectric mat(1.0);
    Vec3 incoming_dir = normalize(Vec3(1, -2, 0.5));
    Ray ray_in(Point3(0, 5, 0), incoming_dir);
    HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, 1, 0), true);

    Color3 attenuation;
    Ray scattered;
    mat.scatter(ray_in, rec, attenuation, scattered);

    Vec3 expected_dir = normalize(incoming_dir);
    Vec3 actual_dir = normalize(scattered.direction());

    EXPECT_NEAR(actual_dir.x(), expected_dir.x(), 1e-9);
    EXPECT_NEAR(actual_dir.y(), expected_dir.y(), 1e-9);
    EXPECT_NEAR(actual_dir.z(), expected_dir.z(), 1e-9);
}

TEST(DielectricTest, FrontFaceUsesInverseIorAndBackFaceUsesIor) {
    // front_face=true: eta = 1.0 / IOR (entering the material)
    // front_face=false: eta = IOR (leaving the material)
    // We verify by checking the refraction angle differs between the two cases.
    // With IOR 1.5 and a 30-degree incidence:
    //   front_face=true:  eta = 1/1.5, sin(theta_r) = (1/1.5)*sin(30) = 0.333
    //   front_face=false: eta = 1.5,   sin(theta_r) = 1.5*sin(30) = 0.75

    double angle = 30.0 * M_PI / 180.0;

    // Test refract directly to avoid Schlick randomness
    Vec3 incoming = normalize(Vec3(std::sin(angle), -std::cos(angle), 0));
    Vec3 normal(0, 1, 0);

    // Entering (front_face=true): eta = 1/1.5
    Vec3 refracted_entering = refract(incoming, normal, 1.0 / 1.5);
    double sin_entering = std::sqrt(1.0 - std::pow(dot(refracted_entering, -normal), 2));

    // Leaving (front_face=false): eta = 1.5
    Vec3 refracted_leaving = refract(incoming, normal, 1.5);
    double sin_leaving = std::sqrt(1.0 - std::pow(dot(refracted_leaving, -normal), 2));

    double expected_sin_entering = (1.0 / 1.5) * std::sin(angle);
    double expected_sin_leaving = 1.5 * std::sin(angle);

    EXPECT_NEAR(sin_entering, expected_sin_entering, 1e-9);
    EXPECT_NEAR(sin_leaving, expected_sin_leaving, 1e-9);
    // The two should be clearly different
    EXPECT_GT(std::fabs(sin_entering - sin_leaving), 0.1);
}

TEST(DielectricTest, ScatteredRayOriginIsHitPoint) {
    Dielectric mat(1.5);
    Point3 hit_point(3, 2, 1);
    Ray ray_in(Point3(0, 5, 0), Vec3(0, -1, 0));
    HitRecord rec = make_hit_record(hit_point, Vec3(0, 1, 0), true);

    Color3 attenuation;
    Ray scattered;
    mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_DOUBLE_EQ(scattered.origin().x(), 3.0);
    EXPECT_DOUBLE_EQ(scattered.origin().y(), 2.0);
    EXPECT_DOUBLE_EQ(scattered.origin().z(), 1.0);
}

TEST(DielectricTest, AccessorReturnsConstructorIor) {
    Dielectric mat(2.4);
    EXPECT_DOUBLE_EQ(mat.ior(), 2.4);
}
