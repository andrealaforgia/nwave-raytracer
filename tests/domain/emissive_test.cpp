#include <gtest/gtest.h>
#include "domain/materials/emissive.h"
#include "domain/hit_record.h"

using namespace nwave;

TEST(EmissiveTest, EmittedReturnsColorTimesIntensity) {
    Emissive mat(Color3(1.0, 0.0, 0.5), 3.0);
    Color3 emitted = mat.emitted();
    EXPECT_DOUBLE_EQ(emitted.r(), 3.0);
    EXPECT_DOUBLE_EQ(emitted.g(), 0.0);
    EXPECT_DOUBLE_EQ(emitted.b(), 1.5);
}

TEST(EmissiveTest, ScatterReturnsFalse) {
    Emissive mat(Color3(1.0, 1.0, 1.0), 1.0);
    Ray ray(Point3(0, 0, 0), Vec3(0, 0, -1));
    HitRecord rec;
    rec.point = Point3(0, 0, -1);
    rec.normal = Vec3(0, 0, 1);
    Color3 attenuation;
    Ray scattered(Point3(0,0,0), Vec3(0,0,0));
    EXPECT_FALSE(mat.scatter(ray, rec, attenuation, scattered));
}

TEST(EmissiveTest, DefaultIntensityIsOne) {
    Emissive mat(Color3(0.5, 0.5, 0.5));
    Color3 emitted = mat.emitted();
    EXPECT_DOUBLE_EQ(emitted.r(), 0.5);
    EXPECT_DOUBLE_EQ(emitted.g(), 0.5);
    EXPECT_DOUBLE_EQ(emitted.b(), 0.5);
}

TEST(EmissiveTest, AccessorsReturnCorrectValues) {
    Emissive mat(Color3(0.9, 0.2, 1.0), 2.5);
    EXPECT_DOUBLE_EQ(mat.color().r(), 0.9);
    EXPECT_DOUBLE_EQ(mat.intensity(), 2.5);
}
