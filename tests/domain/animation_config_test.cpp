#include <gtest/gtest.h>
#include "domain/animation_config.h"
#include <cmath>

using namespace nwave;

// --- Acceptance criteria (US-SPA-023) ---

TEST(AnimationConfigTest, TotalFramesIsCeilOfDurationTimesRenderFps) {
    AnimationConfig config{5.0, 1.0 / 60.0, 30.0, "frames/"};
    EXPECT_EQ(config.total_frames(), 150);
}

TEST(AnimationConfigTest, RenderDtIsInverseOfRenderFps) {
    AnimationConfig config{5.0, 1.0 / 60.0, 30.0, "frames/"};
    EXPECT_NEAR(config.render_dt(), 1.0 / 30.0, 1e-10);
}

TEST(AnimationConfigTest, StepsPerFrameIsRoundOfRenderDtOverPhysicsTimestep) {
    // render_dt = 1/30 = 0.0333..., physics_timestep = 1/60 = 0.01667
    // steps_per_frame = round(0.0333 / 0.01667) = round(2.0) = 2
    AnimationConfig config{5.0, 1.0 / 60.0, 30.0, "frames/"};
    EXPECT_EQ(config.steps_per_frame(), 2);
}

TEST(AnimationConfigTest, DefaultOutputDirectoryIsFrames) {
    AnimationConfig config{5.0, 1.0 / 60.0, 30.0};
    EXPECT_EQ(config.output_directory, "frames/");
}
