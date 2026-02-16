#include <gtest/gtest.h>
#include "domain/materials/lambertian.h"
#include "domain/hit_record.h"
#include "core/ray.h"
#include "core/vec3.h"
#include <cmath>

using namespace nwave;

// Helper: create a HitRecord at a known point with an upward normal
static HitRecord make_hit_record(const Point3& point, const Vec3& normal) {
    HitRecord rec;
    rec.point = point;
    rec.normal = normal;
    rec.front_face = true;
    rec.t = 1.0;
    return rec;
}

TEST(LambertianTest, ScatterAlwaysReturnsTrue) {
    Lambertian mat(Color3(0.5, 0.5, 0.5));
    Ray ray_in(Point3(0, 0, 0), Vec3(0, -1, 0));
    HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, 1, 0));

    Color3 attenuation;
    Ray scattered;
    bool result = mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_TRUE(result);
}

TEST(LambertianTest, AttenuationEqualsAlbedo) {
    Color3 albedo(0.8, 0.2, 0.4);
    Lambertian mat(albedo);
    Ray ray_in(Point3(0, 0, 0), Vec3(0, -1, 0));
    HitRecord rec = make_hit_record(Point3(0, 0, 0), Vec3(0, 1, 0));

    Color3 attenuation;
    Ray scattered;
    mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_DOUBLE_EQ(attenuation.r(), 0.8);
    EXPECT_DOUBLE_EQ(attenuation.g(), 0.2);
    EXPECT_DOUBLE_EQ(attenuation.b(), 0.4);
}

TEST(LambertianTest, ScatteredRayOriginIsHitPoint) {
    Lambertian mat(Color3(0.5, 0.5, 0.5));
    Point3 hit_point(1, 2, 3);
    Ray ray_in(Point3(0, 5, 0), Vec3(0, -1, 0));
    HitRecord rec = make_hit_record(hit_point, Vec3(0, 1, 0));

    Color3 attenuation;
    Ray scattered;
    mat.scatter(ray_in, rec, attenuation, scattered);

    EXPECT_DOUBLE_EQ(scattered.origin().x(), 1.0);
    EXPECT_DOUBLE_EQ(scattered.origin().y(), 2.0);
    EXPECT_DOUBLE_EQ(scattered.origin().z(), 3.0);
}

TEST(LambertianTest, ScatteredDirectionIsInNormalHemisphere) {
    // Stochastic: run multiple trials and check the scatter direction
    // has non-negative dot with normal (or very close to zero)
    Lambertian mat(Color3(0.5, 0.5, 0.5));
    Vec3 normal(0, 1, 0);
    HitRecord rec = make_hit_record(Point3(0, 0, 0), normal);

    for (int i = 0; i < 100; ++i) {
        Ray ray_in(Point3(0, 5, 0), Vec3(0, -1, 0));
        Color3 attenuation;
        Ray scattered;
        mat.scatter(ray_in, rec, attenuation, scattered);

        double d = dot(scattered.direction(), normal);
        // The direction should be in the hemisphere of the normal.
        // near_zero case means direction = normal, which has dot = 1.
        // random_unit_vector + normal can theoretically produce a near-zero
        // vector, but after near_zero fix it becomes normal.
        // The dot should be >= 0 (or very slightly negative due to float precision).
        EXPECT_GE(d, -1e-7) << "Trial " << i << ": scatter direction was in wrong hemisphere";
    }
}

TEST(LambertianTest, AlbedoAccessorReturnsConstructorValue) {
    Color3 albedo(0.3, 0.6, 0.9);
    Lambertian mat(albedo);

    EXPECT_DOUBLE_EQ(mat.albedo().r(), 0.3);
    EXPECT_DOUBLE_EQ(mat.albedo().g(), 0.6);
    EXPECT_DOUBLE_EQ(mat.albedo().b(), 0.9);
}
