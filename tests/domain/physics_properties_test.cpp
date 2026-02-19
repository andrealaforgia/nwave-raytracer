#include <gtest/gtest.h>
#include <optional>
#include "domain/physics_properties.h"
#include "core/vec3.h"

using namespace nwave;

TEST(PhysicsPropertiesTest, DefaultBodyTypeIsStatic) {
    PhysicsProperties props;
    EXPECT_EQ(props.body_type, BodyType::STATIC);
}

TEST(PhysicsPropertiesTest, DefaultScalarProperties) {
    PhysicsProperties props;
    EXPECT_DOUBLE_EQ(props.mass, 1.0);
    EXPECT_DOUBLE_EQ(props.friction, 0.5);
    EXPECT_DOUBLE_EQ(props.restitution, 0.3);
}

TEST(PhysicsPropertiesTest, DefaultInitialVelocityIsZero) {
    PhysicsProperties props;
    EXPECT_DOUBLE_EQ(props.initial_velocity.x(), 0.0);
    EXPECT_DOUBLE_EQ(props.initial_velocity.y(), 0.0);
    EXPECT_DOUBLE_EQ(props.initial_velocity.z(), 0.0);
}

TEST(PhysicsPropertiesTest, DefaultWakeFrameIsNullopt) {
    PhysicsProperties props;
    EXPECT_EQ(props.wake_frame, std::nullopt);
}

TEST(PhysicsPropertiesTest, WakeFrameStoresAssignedValue) {
    PhysicsProperties props;
    props.wake_frame = 60;
    ASSERT_TRUE(props.wake_frame.has_value());
    EXPECT_EQ(props.wake_frame.value(), 60);
}

TEST(BodyTypeTest, EnumHasAllExpectedValues) {
    EXPECT_NE(static_cast<int>(BodyType::STATIC), static_cast<int>(BodyType::DYNAMIC));
    EXPECT_NE(static_cast<int>(BodyType::DYNAMIC), static_cast<int>(BodyType::KINEMATIC));
    EXPECT_NE(static_cast<int>(BodyType::STATIC), static_cast<int>(BodyType::KINEMATIC));
}
