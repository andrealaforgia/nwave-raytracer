#ifndef NWAVE_DOMAIN_SHAPES_LETTER_SHAPES_H
#define NWAVE_DOMAIN_SHAPES_LETTER_SHAPES_H

#include "domain/shapes/box.h"
#include "domain/physics_properties.h"
#include <vector>

namespace nwave {

// Creates boxes forming the letter 'e' at origin (0,0,0) = bottom-left.
// Height ~2.0, width ~1.5, depth ~0.4.
std::vector<Box> create_letter_e(const Material* mat);

// Returns matching PhysicsProperties for each box in create_letter_e().
// Each body is dynamic with start_asleep=true, mass=2.0.
std::vector<PhysicsProperties> create_letter_e_physics();

} // namespace nwave

#endif // NWAVE_DOMAIN_SHAPES_LETTER_SHAPES_H
