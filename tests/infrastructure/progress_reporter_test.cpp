#include <gtest/gtest.h>
#include "infrastructure/progress_reporter.h"
#include "application/animation_renderer.h"
#include "application/physics_simulator.h"
#include "domain/animation_config.h"
#include "domain/scene.h"
#include "domain/camera.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/materials/lambertian.h"
#include "domain/physics_properties.h"
#include <sstream>
#include <stdexcept>
#include <string>
#include <regex>
#include <thread>
#include <chrono>
#include <vector>

namespace nwave {
namespace {

// --- Fake PhysicsSimulator for integration test ---

class FakePhysicsForProgress : public PhysicsSimulator {
public:
    int add_body(const PhysicsBodyDesc& desc) override {
        transforms_[next_id_] = PhysicsTransform{desc.position, desc.rotation};
        return next_id_++;
    }
    void step(double /*dt*/) override {}
    PhysicsTransform get_transform(int body_id) const override {
        auto it = transforms_.find(body_id);
        if (it != transforms_.end()) return it->second;
        return PhysicsTransform{};
    }
    void set_gravity(const Vec3& /*gravity*/) override {}
    void wake_all() override {}

    int add_soft_body(const SoftBodyDesc& /*desc*/) override {
        throw std::runtime_error("not implemented");
    }

    bool is_soft_body(int /*body_id*/) const override {
        return false;
    }

    SoftBodyMeshData get_soft_body_mesh(int /*body_id*/) const override {
        throw std::runtime_error("not implemented");
    }
private:
    int next_id_ = 0;
    std::map<int, PhysicsTransform> transforms_;
};

// ==========================================================
// ACCEPTANCE TEST: Progress reporter output format and ETA
// ==========================================================

TEST(ProgressReporterAcceptance, ReportsFrameCounterPercentageEtaAndDoneMessage) {
    // Given: a StreamProgressReporter writing to a stringstream
    std::ostringstream output;
    StreamProgressReporter reporter(output);

    // When: we simulate a 5-frame render with timing
    reporter.start(5);

    for (int i = 1; i <= 5; ++i) {
        // Simulate work (small sleep to produce measurable ETA)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        reporter.frame_complete(i);
    }

    reporter.finish();

    std::string result = output.str();

    // AC1: Frame counter shows "Frame N/total" after each frame
    EXPECT_NE(result.find("Frame 1/5"), std::string::npos) << "Missing 'Frame 1/5' in output:\n" << result;
    EXPECT_NE(result.find("Frame 3/5"), std::string::npos) << "Missing 'Frame 3/5' in output:\n" << result;
    EXPECT_NE(result.find("Frame 5/5"), std::string::npos) << "Missing 'Frame 5/5' in output:\n" << result;

    // AC2: Percentage displayed
    EXPECT_NE(result.find("20%"), std::string::npos) << "Missing '20%' in output:\n" << result;
    EXPECT_NE(result.find("60%"), std::string::npos) << "Missing '60%' in output:\n" << result;
    EXPECT_NE(result.find("100%"), std::string::npos) << "Missing '100%' in output:\n" << result;

    // AC3: ETA decreases as render progresses
    // Extract ETA values from output lines. ETA should appear for frames 1-4 (not last).
    // We look for "ETA:" followed by a number
    std::regex eta_regex("ETA:\\s*([0-9.]+)s");
    std::sregex_iterator it(result.begin(), result.end(), eta_regex);
    std::sregex_iterator end;
    std::vector<double> etas;
    while (it != end) {
        etas.push_back(std::stod((*it)[1].str()));
        ++it;
    }
    // ETA should be present for at least frames 1-4
    ASSERT_GE(static_cast<int>(etas.size()), 2) << "Expected at least 2 ETA values in output:\n" << result;
    // ETA should generally decrease (or stay same) as progress advances
    // With uniform timing, later ETAs should be <= earlier ETAs
    for (size_t i = 1; i < etas.size(); ++i) {
        EXPECT_LE(etas[i], etas[i - 1] + 0.1)
            << "ETA should decrease (or stay roughly same): ETA[" << i-1 << "]=" << etas[i-1]
            << " ETA[" << i << "]=" << etas[i];
    }

    // AC4: Completion line shows "Done" with total elapsed time
    EXPECT_NE(result.find("Done"), std::string::npos) << "Missing 'Done' in output:\n" << result;
    // Should contain elapsed time in seconds
    std::regex done_regex("Done.*[0-9.]+s");
    EXPECT_TRUE(std::regex_search(result, done_regex))
        << "Completion line should show elapsed time:\n" << result;
}

// ==========================================================
// ACCEPTANCE TEST: AnimationRenderer integrates with ProgressReporter
// ==========================================================

TEST(ProgressReporterAcceptance, AnimationRendererCallsProgressReporter) {
    // Given: AnimationRenderer with a ProgressReporter
    AnimationConfig config{0.3, 0.01, 10.0, "frames/"};
    int total_frames = config.total_frames(); // 3 frames

    Scene scene;
    auto mat = std::make_shared<Lambertian>(Vec3(0.8, 0.2, 0.2));
    auto ground = std::make_shared<Plane>(Point3(0, -1, 0), Vec3(0, 1, 0), mat.get());
    scene.add_shape(ground);

    std::vector<PhysicsProperties> physics_props;
    physics_props.push_back(PhysicsProperties{BodyType::STATIC, 0.0, Vec3(0,0,0), 0.5, 0.3});

    Camera camera(Point3(0, 0, 5), Point3(0, 0, 0), Vec3(0, 1, 0), 60.0, 1.0, 100);
    auto physics = std::make_unique<FakePhysicsForProgress>();

    auto write_cb = [](const std::string&, const Scene&, const Camera&, int, int) {};

    std::ostringstream output;
    auto reporter = std::make_shared<StreamProgressReporter>(output);

    AnimationRenderer renderer(config, scene, physics_props,
                               std::move(physics), camera, write_cb, reporter.get());

    int frame_count = renderer.render();

    EXPECT_EQ(frame_count, total_frames);

    std::string result = output.str();

    // Should see Frame 1/3 through 3/3
    EXPECT_NE(result.find("Frame 1/3"), std::string::npos) << "Output:\n" << result;
    EXPECT_NE(result.find("Frame 3/3"), std::string::npos) << "Output:\n" << result;

    // Should see Done
    EXPECT_NE(result.find("Done"), std::string::npos) << "Output:\n" << result;
}

} // namespace
} // namespace nwave
