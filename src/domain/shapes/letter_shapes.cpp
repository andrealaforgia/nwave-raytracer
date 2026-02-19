#include "domain/shapes/letter_shapes.h"

namespace nwave {

namespace {
constexpr int letter_e_box_count = 5;
}

std::vector<Box> create_letter_e(const Material* mat) {
    // Letter 'e' built from boxes at origin (0,0,0) = bottom-left.
    // Total height ~2.0, width ~1.5, depth ~0.4.
    //
    // Layout:
    //   +---------+        top bar      y=1.8 to y=2.0
    //   |         |
    //   |     +---+        right-upper  y=1.0 to y=1.8
    //   |     |
    //   +-----+---+        middle bar   y=0.9 to y=1.1
    //   |
    //   |                  left spine   y=0.0 to y=2.0
    //   +---------+        bottom bar   y=0.0 to y=0.2

    constexpr double depth = 0.4;
    constexpr double bar_thickness = 0.2;
    constexpr double spine_width = 0.3;

    std::vector<Box> boxes;
    boxes.reserve(5);

    // 1. Left vertical stroke (spine): x=0..0.3, y=0..2.0
    boxes.emplace_back(
        Point3(0.0, 0.0, 0.0),
        Point3(spine_width, 2.0, depth),
        mat);

    // 2. Top bar: x=0.3..1.5, y=1.8..2.0
    boxes.emplace_back(
        Point3(spine_width, 2.0 - bar_thickness, 0.0),
        Point3(1.5, 2.0, depth),
        mat);

    // 3. Middle bar: x=0.3..1.5, y=0.9..1.1
    boxes.emplace_back(
        Point3(spine_width, 0.9, 0.0),
        Point3(1.5, 0.9 + bar_thickness, depth),
        mat);

    // 4. Bottom bar: x=0.3..1.5, y=0.0..0.2
    boxes.emplace_back(
        Point3(spine_width, 0.0, 0.0),
        Point3(1.5, bar_thickness, depth),
        mat);

    // 5. Right-upper segment: x=1.2..1.5, y=1.1..1.8
    boxes.emplace_back(
        Point3(1.2, 0.9 + bar_thickness, 0.0),
        Point3(1.5, 2.0 - bar_thickness, depth),
        mat);

    return boxes;
}

std::vector<PhysicsProperties> create_letter_e_physics() {
    std::vector<PhysicsProperties> props;
    props.reserve(letter_e_box_count);

    for (int i = 0; i < letter_e_box_count; ++i) {
        PhysicsProperties box_physics;
        box_physics.body_type = BodyType::DYNAMIC;
        box_physics.mass = 2.0;
        box_physics.start_asleep = true;
        props.push_back(box_physics);
    }

    return props;
}

} // namespace nwave
