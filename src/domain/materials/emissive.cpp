#include "domain/materials/emissive.h"

namespace nwave {

Emissive::Emissive(const Color3& color, double intensity)
    : color_(color), intensity_(intensity) {}

bool Emissive::scatter(const Ray& /*ray_in*/, const HitRecord& /*rec*/,
                       Color3& /*attenuation*/, Ray& /*scattered*/) const {
    return false; // Emissive materials don't scatter; they only emit light
}

Color3 Emissive::emitted() const {
    return intensity_ * color_;
}

} // namespace nwave
