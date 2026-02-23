#ifndef NWAVE_DOMAIN_MATERIALS_PROCEDURAL_TEXTURE_H
#define NWAVE_DOMAIN_MATERIALS_PROCEDURAL_TEXTURE_H

#include "domain/materials/image_texture.h"
#include <cstdint>
#include <string>

namespace nwave {

enum class ProceduralPattern {
    CAMOUFLAGE,
    CAVE_ART,
    CLOUDS,
    CONCRETE,
    ENTANGLED,
    FORDITE,
    GRID,
    ISOLINES,
    MARBLE,
    NEON,
    PLANET,
    PHOTOSPHERE,
    POLKA_DOTS,
    SATIN,
    SCEPTER_HEAD,
    SCREAM,
    SIMPLEX_NOISE,
    STARS,
    WATER_DROPS,
    ZEBRA
};

ProceduralPattern pattern_from_string(const std::string& name);

class ProceduralTexture : public ImageTexture {
public:
    static std::unique_ptr<ProceduralTexture> generate(
        ProceduralPattern pattern, int width, int height, uint32_t seed);

private:
    ProceduralTexture(std::vector<uint8_t> pixels, int width, int height);

    static void generate_pixels(
        ProceduralPattern pattern, std::vector<uint8_t>& pixels,
        int width, int height, uint32_t seed);
};

} // namespace nwave

#endif // NWAVE_DOMAIN_MATERIALS_PROCEDURAL_TEXTURE_H
