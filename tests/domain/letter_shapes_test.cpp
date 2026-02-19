#include <gtest/gtest.h>
#include "domain/shapes/letter_shapes.h"
#include "domain/shapes/box.h"
#include "domain/physics_properties.h"
#include "core/vec3.h"

using namespace nwave;

TEST(LetterShapesTest, CreateLetterEReturnsAtLeast5Boxes) {
    auto boxes = create_letter_e(nullptr);
    EXPECT_GE(boxes.size(), 5u);
}

TEST(LetterShapesTest, LetterEApproximately2UnitsTallAnd1Point5Wide) {
    auto boxes = create_letter_e(nullptr);

    double min_x = 1e9, min_y = 1e9;
    double max_x = -1e9, max_y = -1e9;
    for (const auto& box : boxes) {
        min_x = std::min(min_x, box.box_min().x());
        min_y = std::min(min_y, box.box_min().y());
        max_x = std::max(max_x, box.box_max().x());
        max_y = std::max(max_y, box.box_max().y());
    }

    double height = max_y - min_y;
    double width = max_x - min_x;

    EXPECT_NEAR(height, 2.0, 0.3);
    EXPECT_NEAR(width, 1.5, 0.3);
}

TEST(LetterShapesTest, EachBoxHasPositiveDimensions) {
    auto boxes = create_letter_e(nullptr);

    for (size_t i = 0; i < boxes.size(); ++i) {
        const auto& box = boxes[i];
        double w = box.box_max().x() - box.box_min().x();
        double h = box.box_max().y() - box.box_min().y();
        double d = box.box_max().z() - box.box_min().z();

        EXPECT_GT(w, 0.0) << "Box " << i << " has non-positive width";
        EXPECT_GT(h, 0.0) << "Box " << i << " has non-positive height";
        EXPECT_GT(d, 0.0) << "Box " << i << " has non-positive depth";
    }
}

TEST(LetterShapesTest, CreateLetterEPhysicsReturnsDynamicStartAsleep) {
    auto boxes = create_letter_e(nullptr);
    auto physics = create_letter_e_physics();

    ASSERT_EQ(physics.size(), boxes.size());

    for (size_t i = 0; i < physics.size(); ++i) {
        EXPECT_EQ(physics[i].body_type, BodyType::DYNAMIC) << "Box " << i;
        EXPECT_TRUE(physics[i].start_asleep) << "Box " << i;
        EXPECT_DOUBLE_EQ(physics[i].mass, 2.0) << "Box " << i;
    }
}
