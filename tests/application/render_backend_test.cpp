#include <gtest/gtest.h>
#include "application/render_backend.h"
#include "domain/camera.h"
#include "domain/scene.h"
#include "core/vec3.h"
#include <vector>

using namespace nwave;

// A concrete implementation that returns a fixed pixel buffer
class StubRenderBackend : public RenderBackend {
public:
    std::vector<Color3> render(const Camera& camera, const Scene& /*scene*/,
                               const RenderSettings& /*settings*/) override {
        int pixel_count = camera.image_width() * camera.image_height();
        return std::vector<Color3>(pixel_count, Color3(0.1, 0.2, 0.3));
    }
};

// A class that does NOT implement render() -- should not compile if instantiated
class IncompleteBackend : public RenderBackend {
    // Intentionally missing render() override
};

TEST(RenderBackendTest, ConcreteImplementationIsCallableThroughInterface) {
    StubRenderBackend stub;
    RenderBackend& backend = stub;

    Camera camera(Point3(0, 0, 0), Point3(0, 0, -1), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 80);
    Scene scene;
    RenderSettings settings;

    std::vector<Color3> pixels = backend.render(camera, scene, settings);

    int expected_count = camera.image_width() * camera.image_height();
    ASSERT_EQ(static_cast<int>(pixels.size()), expected_count);
    EXPECT_NEAR(pixels[0].r(), 0.1, 1e-9);
    EXPECT_NEAR(pixels[0].g(), 0.2, 1e-9);
    EXPECT_NEAR(pixels[0].b(), 0.3, 1e-9);
}

TEST(RenderBackendTest, InterfaceIsPureVirtual) {
    // IncompleteBackend cannot be instantiated because render() is pure virtual.
    // This is a compile-time guarantee. We verify it with a static_assert.
    EXPECT_TRUE(std::is_abstract<RenderBackend>::value);
    EXPECT_TRUE(std::is_abstract<IncompleteBackend>::value);
}
