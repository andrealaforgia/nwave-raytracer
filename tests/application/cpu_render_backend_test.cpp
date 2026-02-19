#include <gtest/gtest.h>
#include "application/cpu_render_backend.h"
#include "application/renderer.h"
#include "application/render_backend.h"
#include "domain/camera.h"
#include "domain/scene.h"
#include "core/vec3.h"
#include <memory>
#include <sstream>
#include <iostream>

using namespace nwave;

class CpuRenderBackendTest : public ::testing::Test {
protected:
    Camera camera{Point3(0, 0, -2), Point3(0, 0, 0), Vec3(0, 1, 0),
                  90.0, 16.0 / 9.0, 16};
    Scene scene;
    RenderSettings settings;

    CpuRenderBackendTest() {
        settings.samples_per_pixel = 1;
        settings.max_depth = 2;
        settings.num_threads = 1;
    }
};

TEST_F(CpuRenderBackendTest, ProducesByteIdenticalOutputToDirectRenderer) {
    Renderer renderer;
    renderer.set_quiet(true);
    std::vector<Color3> expected = renderer.render(camera, scene, settings);

    CpuRenderBackend backend;
    backend.set_quiet(true);
    std::vector<Color3> actual = backend.render(camera, scene, settings);

    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_DOUBLE_EQ(actual[i].r(), expected[i].r()) << "Mismatch at pixel " << i << " (red)";
        EXPECT_DOUBLE_EQ(actual[i].g(), expected[i].g()) << "Mismatch at pixel " << i << " (green)";
        EXPECT_DOUBLE_EQ(actual[i].b(), expected[i].b()) << "Mismatch at pixel " << i << " (blue)";
    }
}

TEST_F(CpuRenderBackendTest, IsCallableThroughRenderBackendInterface) {
    auto backend = std::make_unique<CpuRenderBackend>();
    backend->set_quiet(true);
    RenderBackend* base = backend.get();

    std::vector<Color3> pixels = base->render(camera, scene, settings);

    int expected_count = camera.image_width() * camera.image_height();
    ASSERT_EQ(static_cast<int>(pixels.size()), expected_count);
}

TEST_F(CpuRenderBackendTest, SetQuietTrueSuppressesProgressOutput) {
    // Use multiple threads to trigger the "Using N threads" output
    RenderSettings multi_thread_settings = settings;
    multi_thread_settings.num_threads = 2;

    // Capture stdout
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    CpuRenderBackend backend;
    backend.set_quiet(true);
    backend.render(camera, scene, multi_thread_settings);

    std::cout.rdbuf(old);

    EXPECT_TRUE(buffer.str().empty()) << "Expected no output, got: " << buffer.str();
}
