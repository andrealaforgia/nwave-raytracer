#include "domain/materials/procedural_texture.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

#define STB_PERLIN_IMPLEMENTATION
#include "stb/stb_perlin.h"

namespace nwave {

// --- Utility functions ---

struct Color3f {
    float r, g, b;
};

static float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

static float smoothstep(float edge0, float edge1, float x) {
    float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static Color3f lerp_color(const Color3f& a, const Color3f& b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

static float fract(float x) {
    return x - std::floor(x);
}

// Seeded noise wrapper: offsets coordinates by seed to produce different patterns
static float noise3(float x, float y, float z, uint32_t seed) {
    float s = static_cast<float>(seed & 0xFF);
    return stb_perlin_noise3(x + s * 17.1f, y + s * 31.7f, z + s * 53.3f, 0, 0, 0);
}

static float fbm(float x, float y, float z, uint32_t seed, int octaves = 6,
                 float lacunarity = 2.0f, float gain = 0.5f) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise3(x * frequency, y * frequency, z * frequency, seed);
        frequency *= lacunarity;
        amplitude *= gain;
    }
    return value;
}

static float turbulence(float x, float y, float z, uint32_t seed, int octaves = 6,
                        float lacunarity = 2.0f, float gain = 0.5f) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * std::fabs(noise3(x * frequency, y * frequency, z * frequency, seed));
        frequency *= lacunarity;
        amplitude *= gain;
    }
    return value;
}

static float ridged(float x, float y, float z, uint32_t seed, int octaves = 6,
                    float lacunarity = 2.0f, float gain = 0.5f) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float weight = 1.0f;
    for (int i = 0; i < octaves; i++) {
        float signal = 1.0f - std::fabs(noise3(x * frequency, y * frequency, z * frequency, seed));
        signal *= signal;
        signal *= weight;
        weight = clampf(signal * 2.0f, 0.0f, 1.0f);
        value += signal * amplitude;
        frequency *= lacunarity;
        amplitude *= gain;
    }
    return value;
}

// Simple hash for per-cell random colors
static float hash1(float n) {
    return fract(std::sin(n) * 43758.5453123f);
}

// HSL to RGB conversion
static Color3f hsl_to_rgb(float h, float s, float l) {
    h = h - std::floor(h); // wrap hue to [0,1]
    float c = (1.0f - std::fabs(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = l - c * 0.5f;
    float r, g, b;
    if (h < 1.0f / 6.0f)      { r = c; g = x; b = 0; }
    else if (h < 2.0f / 6.0f) { r = x; g = c; b = 0; }
    else if (h < 3.0f / 6.0f) { r = 0; g = c; b = x; }
    else if (h < 4.0f / 6.0f) { r = 0; g = x; b = c; }
    else if (h < 5.0f / 6.0f) { r = x; g = 0; b = c; }
    else                       { r = c; g = 0; b = x; }
    return {clampf(r + m, 0.0f, 1.0f), clampf(g + m, 0.0f, 1.0f), clampf(b + m, 0.0f, 1.0f)};
}

// --- Pattern generators ---
// Each takes 3D point on unit sphere + seed, returns Color3f in [0,1]

// ========== KEPT PATTERNS (10) ==========

static Color3f generate_camouflage(float x, float y, float z, uint32_t seed) {
    // Domain-warped noise
    float qx = fbm(x * 2.0f, y * 2.0f, z * 2.0f, seed, 4);
    float qy = fbm(x * 2.0f + 5.2f, y * 2.0f + 1.3f, z * 2.0f, seed, 4);
    float qz = fbm(x * 2.0f + 1.7f, y * 2.0f + 9.2f, z * 2.0f, seed, 4);
    float val = fbm(x + 3.0f * qx, y + 3.0f * qy, z + 3.0f * qz, seed, 4);
    val = val * 0.5f + 0.5f;

    const Color3f dark_green = {0.15f, 0.25f, 0.10f};
    const Color3f olive      = {0.35f, 0.40f, 0.18f};
    const Color3f tan_c      = {0.55f, 0.50f, 0.30f};
    const Color3f brown      = {0.30f, 0.20f, 0.10f};

    if (val < 0.25f) return lerp_color(dark_green, olive, val / 0.25f);
    if (val < 0.50f) return lerp_color(olive, tan_c, (val - 0.25f) / 0.25f);
    if (val < 0.75f) return lerp_color(tan_c, brown, (val - 0.50f) / 0.25f);
    return lerp_color(brown, dark_green, (val - 0.75f) / 0.25f);
}

static Color3f generate_clouds(float x, float y, float z, uint32_t seed) {
    float density = turbulence(x * 2.0f, y * 2.0f, z * 2.0f, seed, 6);
    density = smoothstep(0.25f, 0.65f, density);

    const Color3f sky   = {0.40f, 0.60f, 0.90f};
    const Color3f white = {1.00f, 1.00f, 1.00f};
    return lerp_color(sky, white, density);
}

static Color3f generate_concrete(float x, float y, float z, uint32_t seed) {
    float base = fbm(x * 4.0f, y * 4.0f, z * 4.0f, seed, 5) * 0.15f + 0.55f;
    // Darker speckles
    float speck = noise3(x * 30.0f, y * 30.0f, z * 30.0f, seed + 2);
    if (speck > 0.45f) base -= 0.08f;
    base = clampf(base, 0.0f, 1.0f);

    return {base, base * 0.98f, base * 0.95f};
}

static Color3f generate_marble(float x, float y, float z, uint32_t seed) {
    float t = turbulence(x * 3.0f, y * 3.0f, z * 3.0f, seed, 5);
    float vein = std::sin(x * 8.0f + t * 10.0f);
    vein = (vein + 1.0f) * 0.5f;
    vein = std::pow(vein, 0.8f);

    const Color3f dark  = {0.10f, 0.10f, 0.15f};
    const Color3f light = {0.90f, 0.88f, 0.85f};
    return lerp_color(dark, light, vein);
}

static Color3f generate_neon(float x, float y, float z, uint32_t seed) {
    // Domain-warped noise with neon colors
    float qx = fbm(x * 2.0f + 1.3f, y * 2.0f, z * 2.0f + 2.7f, seed, 4);
    float qy = fbm(x * 2.0f + 5.2f, y * 2.0f + 1.3f, z * 2.0f, seed, 4);
    float val = fbm(x * 2.0f + 3.0f * qx, y * 2.0f + 3.0f * qy, z * 2.0f, seed, 4);
    val = val * 0.5f + 0.5f;

    const Color3f pink  = {1.00f, 0.20f, 0.60f};
    const Color3f cyan  = {0.10f, 0.90f, 0.95f};
    const Color3f green = {0.20f, 1.00f, 0.30f};
    const Color3f bg    = {0.05f, 0.02f, 0.08f};

    if (val < 0.30f) return lerp_color(bg, pink, smoothstep(0.20f, 0.30f, val));
    if (val < 0.55f) return lerp_color(pink, cyan, (val - 0.30f) / 0.25f);
    if (val < 0.75f) return lerp_color(cyan, green, (val - 0.55f) / 0.20f);
    return lerp_color(green, bg, (val - 0.75f) / 0.25f);
}

static Color3f generate_planet(float x, float y, float z, uint32_t seed) {
    float elevation = fbm(x * 2.0f, y * 2.0f, z * 2.0f, seed, 6);

    // Biome color bands
    const Color3f deep_ocean  = {0.12f, 0.22f, 0.55f};
    const Color3f shallow     = {0.25f, 0.50f, 0.75f};
    const Color3f beach       = {0.85f, 0.82f, 0.60f};
    const Color3f grass       = {0.30f, 0.60f, 0.20f};
    const Color3f forest      = {0.15f, 0.45f, 0.18f};
    const Color3f rock        = {0.50f, 0.45f, 0.40f};
    const Color3f snow        = {0.92f, 0.93f, 0.95f};

    if (elevation < -0.10f) return lerp_color(deep_ocean, shallow, (elevation + 0.4f) / 0.3f);
    if (elevation <  0.00f) return lerp_color(shallow, beach, (elevation + 0.10f) / 0.10f);
    if (elevation <  0.02f) return beach;
    if (elevation <  0.20f) return lerp_color(grass, forest, (elevation - 0.02f) / 0.18f);
    if (elevation <  0.40f) return lerp_color(forest, rock, (elevation - 0.20f) / 0.20f);
    return lerp_color(rock, snow, clampf((elevation - 0.40f) / 0.30f, 0.0f, 1.0f));
}

static Color3f generate_polka_dots(float x, float y, float z, uint32_t seed) {
    // Grid-based polka dots on sphere surface using 3D grid
    float scale = 6.0f;
    float sx = x * scale;
    float sy = y * scale;
    float sz = z * scale;

    float ix = std::floor(sx);
    float iy = std::floor(sy);
    float iz = std::floor(sz);
    float fx = sx - ix;
    float fy = sy - iy;
    float fz = sz - iz;

    // Distance from cell center
    float dx = fx - 0.5f;
    float dy = fy - 0.5f;
    float dz = fz - 0.5f;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    float radius = 0.30f;
    float dot = 1.0f - smoothstep(radius - 0.05f, radius + 0.05f, dist);

    // Per-cell color from hash
    float h = hash1(ix * 127.1f + iy * 311.7f + iz * 74.7f + static_cast<float>(seed));

    Color3f bg = {0.95f, 0.95f, 0.92f};
    Color3f dot_colors[] = {
        {0.90f, 0.20f, 0.20f}, {0.20f, 0.50f, 0.90f}, {0.20f, 0.80f, 0.30f},
        {0.95f, 0.70f, 0.10f}, {0.80f, 0.20f, 0.80f}, {0.10f, 0.80f, 0.80f}
    };
    int ci = static_cast<int>(h * 6.0f) % 6;
    return lerp_color(bg, dot_colors[ci], dot);
}

static Color3f generate_satin(float x, float y, float z, uint32_t seed) {
    float n = fbm(x * 1.5f, y * 1.5f, z * 1.5f, seed, 4, 2.0f, 0.4f) * 0.5f + 0.5f;
    // Smooth pastel with sheen effect
    float sheen = noise3(x * 5.0f, y * 5.0f, z * 5.0f, seed + 3) * 0.1f;
    n += sheen;
    n = clampf(n, 0.0f, 1.0f);

    const Color3f pastel_a = {0.75f, 0.55f, 0.65f};
    const Color3f pastel_b = {0.85f, 0.75f, 0.80f};
    Color3f c = lerp_color(pastel_a, pastel_b, n);
    // Add subtle highlight
    float highlight = std::pow(clampf(n, 0.0f, 1.0f), 3.0f) * 0.15f;
    c.r = clampf(c.r + highlight, 0.0f, 1.0f);
    c.g = clampf(c.g + highlight, 0.0f, 1.0f);
    c.b = clampf(c.b + highlight, 0.0f, 1.0f);
    return c;
}

static Color3f generate_stars(float x, float y, float z, uint32_t seed) {
    Color3f bg = {0.02f, 0.02f, 0.06f};
    float n = noise3(x * 20.0f, y * 20.0f, z * 20.0f, seed);
    n = clampf(n, 0.0f, 1.0f);

    // Only show bright peaks as stars
    float star = smoothstep(0.42f, 0.50f, n);
    float nebula = fbm(x * 3.0f, y * 3.0f, z * 3.0f, seed + 5, 4) * 0.5f + 0.5f;

    Color3f neb_color = {0.10f * nebula, 0.03f * nebula, 0.15f * nebula};
    Color3f result = {bg.r + neb_color.r + star, bg.g + neb_color.g + star * 0.95f,
                      bg.b + neb_color.b + star * 0.85f};
    result.r = clampf(result.r, 0.0f, 1.0f);
    result.g = clampf(result.g, 0.0f, 1.0f);
    result.b = clampf(result.b, 0.0f, 1.0f);
    return result;
}

static Color3f generate_zebra(float x, float y, float z, uint32_t seed) {
    float n = noise3(x * 2.0f, y * 2.0f, z * 2.0f, seed) * 2.0f;
    float stripe = std::sin(y * 12.0f + n * 4.0f);
    float t = smoothstep(-0.1f, 0.1f, stripe);

    const Color3f black = {0.05f, 0.05f, 0.05f};
    const Color3f white = {0.95f, 0.93f, 0.90f};
    return lerp_color(black, white, t);
}

// ========== NEW PATTERNS (10) ==========

static Color3f generate_cave_art(float x, float y, float z, uint32_t seed) {
    const Color3f background = {0.82f, 0.72f, 0.55f}; // sandstone
    const Color3f foreground = {0.25f, 0.12f, 0.08f};  // dark ochre
    float scale = 3.0f;
    float thickness = 0.5f;

    float k1 = noise3(x * scale, y * scale, z * scale, seed);
    float k2 = noise3(x * scale * 1.5f, y * scale * 1.5f, z * scale * 1.5f, seed + 7);
    k1 = std::sin(3.0f * k1);
    k2 = std::cos(3.0f * k2);
    float sum = k1 + k2;
    float k = thickness - 20.0f * sum * sum;

    if (k1 > k2 || k < 0.0f) k = 0.0f;
    if (k <= 0.0f) {
        // random noise for background texture
        float rn = hash1(x * 123.4f + y * 567.8f + z * 91.2f + static_cast<float>(seed));
        k = rn * 0.08f;
    }
    k = clampf(k, 0.0f, 1.0f);
    return lerp_color(background, foreground, k);
}

static Color3f generate_entangled(float x, float y, float z, uint32_t seed) {
    const Color3f foreground = {0.15f, 0.08f, 0.30f}; // deep purple
    const Color3f background = {0.95f, 0.92f, 0.85f}; // cream
    float base_scale = 2.0f;
    int density = 8;

    float k = -1e10f;
    float scale = base_scale;
    for (int i = 0; i < density; i++) {
        float n = noise3(x * scale, y * scale, z * scale, seed + static_cast<uint32_t>(i));
        float k1 = std::sin(3.0f * static_cast<float>(M_PI) * n);
        k = std::max(k, k1);
        scale *= 1.2f;
    }

    k = 1.0f - std::pow(clampf(k, 0.0f, 1.0f), 5.0f);
    k = clampf(k * 6.0f, 0.0f, 1.0f);
    return lerp_color(foreground, background, k);
}

static Color3f generate_fordite(float x, float y, float z, uint32_t seed) {
    float scale = 3.0f;
    float n1 = noise3(x * scale, y * scale, z * scale, seed) * 0.5f + 0.5f;
    float n2 = noise3(x * scale * 2.0f, y * scale * 2.0f, z * scale * 2.0f, seed + 3) * 0.5f + 0.5f;
    float n3 = noise3(x * scale * 3.0f, y * scale * 3.0f, z * scale * 3.0f, seed + 7) * 0.5f + 0.5f;

    // Nested noise
    float k = noise3(n1 * scale, n2 * scale * 2.0f, n3 * scale * 3.0f, seed + 11) * 0.5f + 0.5f;

    // HSL color from k
    float h = 0.5f * k + 0.3f;
    float s = 1.0f;
    float l = 0.5f + 0.5f * std::sin(4.0f * static_cast<float>(M_PI) * k);
    l = clampf(l, 0.1f, 0.9f);

    Color3f c = hsl_to_rgb(h, s, l);
    // Add base color tint
    c.r = clampf(c.r * 0.85f + 0.15f * 0.9f, 0.0f, 1.0f);
    c.g = clampf(c.g * 0.85f + 0.15f * 0.3f, 0.0f, 1.0f);
    c.b = clampf(c.b * 0.85f + 0.15f * 0.2f, 0.0f, 1.0f);
    return c;
}

static Color3f generate_grid(float x, float y, float z, uint32_t seed) {
    const Color3f background = {0.12f, 0.12f, 0.15f}; // dark gray
    const Color3f foreground = {0.70f, 0.85f, 0.95f};  // light cyan

    // Spherical coordinates
    float theta = std::acos(clampf(y, -1.0f, 1.0f));
    float phi = std::atan2(z, x) + static_cast<float>(M_PI);

    // Grid spacing
    float grid_h = static_cast<float>(M_PI) / 12.0f;  // 12 horizontal lines
    float grid_v = 2.0f * static_cast<float>(M_PI) / 24.0f; // 24 vertical lines

    // Distance to nearest horizontal grid line
    float h_dist = std::fmod(theta, grid_h);
    h_dist = std::min(h_dist, grid_h - h_dist);

    // Distance to nearest vertical grid line
    float v_dist = std::fmod(phi, grid_v);
    v_dist = std::min(v_dist, grid_v - v_dist);

    float angle = std::min(h_dist, v_dist);

    float smooth_min = 0.01f;
    float smooth_max = 0.04f;
    float k = 1.0f - smoothstep(smooth_min, smooth_max, angle);

    // Add subtle noise to grid lines
    float n = noise3(x * 10.0f, y * 10.0f, z * 10.0f, seed) * 0.1f;
    k = clampf(k + n * k, 0.0f, 1.0f);

    return lerp_color(background, foreground, k);
}

static Color3f generate_isolines(float x, float y, float z, uint32_t seed) {
    const Color3f foreground = {0.10f, 0.25f, 0.50f}; // deep blue
    const Color3f background = {0.92f, 0.90f, 0.82f};  // cream
    float scale = 3.0f;
    float density = 12.0f;

    float k = fbm(x * scale, y * scale, z * scale, seed, 5);
    k = 0.5f - 0.5f * std::sin(density * k);
    k = smoothstep(0.4f, 0.6f, k);

    return lerp_color(foreground, background, k);
}

static Color3f generate_photosphere(float x, float y, float z, uint32_t seed) {
    const Color3f background = {0.95f, 0.55f, 0.10f}; // solar orange
    const Color3f foreground = {1.00f, 0.95f, 0.70f};  // bright yellow
    float scale = 3.0f;

    float vx = x, vy = y, vz = z;
    for (int i = 0; i < 5; i++) {
        // Euler rotation using noise-derived angles
        float ax = noise3(vx * scale, vy * scale, vz * scale, seed + static_cast<uint32_t>(i)) * 0.5f;
        float ay = noise3(vy * scale, vz * scale, vx * scale, seed + static_cast<uint32_t>(i) + 10) * 0.5f;

        float ca = std::cos(ax), sa = std::sin(ax);
        float cb = std::cos(ay), sb = std::sin(ay);

        float nx = vx * cb + vz * sb;
        float ny = vy * ca - (vz * cb - vx * sb) * sa;
        float nz = vy * sa + (vz * cb - vx * sb) * ca;

        vx = nx; vy = ny; vz = nz;
        scale *= 1.1f;
    }

    float k = (1.0f + noise3(vx, vy, vz, seed + 50)) * 0.5f;
    k = clampf(k, 0.0f, 1.0f);
    return lerp_color(background, foreground, k);
}

static Color3f generate_scepter_head(float x, float y, float z, uint32_t seed) {
    float scale = 4.0f;
    float sx = x * scale, sy = y * scale, sz = z * scale;

    // Cosine/tangent division pattern
    float cx = std::cos(sx * 2.0f);
    float cy = std::cos(sy * 2.0f);
    float cz = std::cos(sz * 2.0f);

    // Division produces interesting organic patterns
    float denom = std::fabs(cx + cy + cz) + 0.01f;
    float val = (cx * cy + cy * cz + cz * cx) / denom;

    // Noise modulation
    float n = noise3(x * 3.0f, y * 3.0f, z * 3.0f, seed) * 0.3f;
    val += n;

    // Checkerboard in interior
    int check = (static_cast<int>(std::floor(sx)) + static_cast<int>(std::floor(sy)) + static_cast<int>(std::floor(sz))) & 1;
    float checker = check ? 0.7f : 0.3f;

    float dist = std::sqrt(x * x + y * y + z * z);
    float rim = smoothstep(0.85f, 1.0f, dist);

    const Color3f interior_a = {0.85f, 0.75f, 0.20f}; // gold
    const Color3f interior_b = {0.30f, 0.15f, 0.50f};  // purple
    const Color3f rim_color  = {0.90f, 0.30f, 0.15f};  // red-orange

    // HSL lightness modulated by noise
    float l_mod = 0.5f + 0.3f * noise3(x * 5.0f, y * 5.0f, z * 5.0f, seed + 5);

    Color3f interior = lerp_color(interior_a, interior_b, clampf(val * 0.5f + 0.5f, 0.0f, 1.0f));
    interior.r *= (checker * 0.5f + 0.5f) * l_mod * 2.0f;
    interior.g *= (checker * 0.5f + 0.5f) * l_mod * 2.0f;
    interior.b *= (checker * 0.5f + 0.5f) * l_mod * 2.0f;
    interior.r = clampf(interior.r, 0.0f, 1.0f);
    interior.g = clampf(interior.g, 0.0f, 1.0f);
    interior.b = clampf(interior.b, 0.0f, 1.0f);

    return lerp_color(interior, rim_color, rim);
}

static Color3f generate_scream(float x, float y, float z, uint32_t seed) {
    float s = 3.0f;

    // First pass: sin/cos domain warp
    float xx = std::sin(s * x) + std::cos(s * y);
    float yy = std::sin(s * y) + std::cos(s * z);
    float zz = std::sin(s * z) + std::cos(s * x);

    float k = noise3(xx, yy, zz, seed) * 0.5f + 0.5f;
    s = s * k + 1.0f;

    // Second pass
    xx = std::sin(s * x) + std::cos(s * y);
    yy = std::sin(s * y) + std::cos(s * z);
    zz = std::sin(s * z) + std::cos(s * x);

    k = 2.0f * (noise3(xx, yy, zz, seed + 5) * 0.5f + 0.5f);
    k = clampf(k, 0.0f, 1.0f);

    // Color with hue offset
    float hue_offset = std::cos(k - 0.5f) * 0.3f;
    float h = k * 0.8f + hue_offset;
    float sat = 0.85f;
    float lum = 0.45f + 0.2f * k;

    return hsl_to_rgb(h, sat, lum);
}

static Color3f generate_simplex_noise(float x, float y, float z, uint32_t seed) {
    const Color3f foreground = {0.20f, 0.40f, 0.70f}; // blue
    const Color3f background = {0.95f, 0.90f, 0.80f};  // warm cream
    float scale = 3.0f;
    float balance = 1.5f;

    float k = 0.5f - 0.5f * noise3(x * scale, y * scale, z * scale, seed);
    k = clampf(k, 0.0f, 1.0f);
    k = std::pow(k, balance);
    return lerp_color(foreground, background, k);
}

static Color3f generate_water_drops(float x, float y, float z, uint32_t seed) {
    const Color3f background = {0.15f, 0.25f, 0.45f}; // dark water blue
    const Color3f foreground = {0.70f, 0.85f, 0.95f};  // light water
    float scale = 4.0f;
    float density = 0.3f;

    float n = fbm(x * scale, y * scale, z * scale, seed, 4);
    float k = n + density;
    k = clampf(k, 0.0f, 1.0f);
    k = std::sqrt(k);
    // Cosine smoothing
    k = -(std::cos(static_cast<float>(M_PI) * k) - 1.0f) * 0.5f;
    k = clampf(k, 0.0f, 1.0f);

    return lerp_color(background, foreground, k);
}

// --- Dispatcher ---

static Color3f evaluate_pattern(ProceduralPattern pattern, float x, float y, float z, uint32_t seed) {
    switch (pattern) {
        case ProceduralPattern::CAMOUFLAGE:   return generate_camouflage(x, y, z, seed);
        case ProceduralPattern::CAVE_ART:     return generate_cave_art(x, y, z, seed);
        case ProceduralPattern::CLOUDS:       return generate_clouds(x, y, z, seed);
        case ProceduralPattern::CONCRETE:     return generate_concrete(x, y, z, seed);
        case ProceduralPattern::ENTANGLED:    return generate_entangled(x, y, z, seed);
        case ProceduralPattern::FORDITE:      return generate_fordite(x, y, z, seed);
        case ProceduralPattern::GRID:         return generate_grid(x, y, z, seed);
        case ProceduralPattern::ISOLINES:     return generate_isolines(x, y, z, seed);
        case ProceduralPattern::MARBLE:       return generate_marble(x, y, z, seed);
        case ProceduralPattern::NEON:         return generate_neon(x, y, z, seed);
        case ProceduralPattern::PLANET:       return generate_planet(x, y, z, seed);
        case ProceduralPattern::PHOTOSPHERE:  return generate_photosphere(x, y, z, seed);
        case ProceduralPattern::POLKA_DOTS:   return generate_polka_dots(x, y, z, seed);
        case ProceduralPattern::SATIN:        return generate_satin(x, y, z, seed);
        case ProceduralPattern::SCEPTER_HEAD: return generate_scepter_head(x, y, z, seed);
        case ProceduralPattern::SCREAM:       return generate_scream(x, y, z, seed);
        case ProceduralPattern::SIMPLEX_NOISE:return generate_simplex_noise(x, y, z, seed);
        case ProceduralPattern::STARS:        return generate_stars(x, y, z, seed);
        case ProceduralPattern::WATER_DROPS:  return generate_water_drops(x, y, z, seed);
        case ProceduralPattern::ZEBRA:        return generate_zebra(x, y, z, seed);
    }
    return {0.5f, 0.5f, 0.5f}; // fallback gray
}

// --- Public API ---

ProceduralPattern pattern_from_string(const std::string& name) {
    if (name == "camouflage")   return ProceduralPattern::CAMOUFLAGE;
    if (name == "cave_art")     return ProceduralPattern::CAVE_ART;
    if (name == "clouds")       return ProceduralPattern::CLOUDS;
    if (name == "concrete")     return ProceduralPattern::CONCRETE;
    if (name == "entangled")    return ProceduralPattern::ENTANGLED;
    if (name == "fordite")      return ProceduralPattern::FORDITE;
    if (name == "grid")         return ProceduralPattern::GRID;
    if (name == "isolines")     return ProceduralPattern::ISOLINES;
    if (name == "marble")       return ProceduralPattern::MARBLE;
    if (name == "neon_lights")  return ProceduralPattern::NEON;
    if (name == "planet")       return ProceduralPattern::PLANET;
    if (name == "photosphere")  return ProceduralPattern::PHOTOSPHERE;
    if (name == "polka_dots")   return ProceduralPattern::POLKA_DOTS;
    if (name == "satin")        return ProceduralPattern::SATIN;
    if (name == "scepter_head") return ProceduralPattern::SCEPTER_HEAD;
    if (name == "scream")       return ProceduralPattern::SCREAM;
    if (name == "simplex_noise")return ProceduralPattern::SIMPLEX_NOISE;
    if (name == "stars")        return ProceduralPattern::STARS;
    if (name == "water_drops")  return ProceduralPattern::WATER_DROPS;
    if (name == "zebra")        return ProceduralPattern::ZEBRA;
    throw std::runtime_error("Unknown procedural pattern: " + name);
}

ProceduralTexture::ProceduralTexture(std::vector<uint8_t> pixels, int width, int height)
    : ImageTexture(std::move(pixels), width, height) {}

void ProceduralTexture::generate_pixels(
    ProceduralPattern pattern, std::vector<uint8_t>& pixels,
    int width, int height, uint32_t seed)
{
    pixels.resize(static_cast<size_t>(width) * height * 4);

    for (int py = 0; py < height; py++) {
        for (int px = 0; px < width; px++) {
            float u = static_cast<float>(px) / static_cast<float>(width);
            float v = static_cast<float>(py) / static_cast<float>(height);

            // Equirectangular -> 3D point on unit sphere
            float theta = v * static_cast<float>(M_PI);
            float phi = u * 2.0f * static_cast<float>(M_PI);
            float sx = std::cos(phi) * std::sin(theta);
            float sy = std::cos(theta);
            float sz = std::sin(phi) * std::sin(theta);

            Color3f color = evaluate_pattern(pattern, sx, sy, sz, seed);

            int idx = (py * width + px) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(clampf(color.r, 0.0f, 1.0f) * 255.0f);
            pixels[idx + 1] = static_cast<uint8_t>(clampf(color.g, 0.0f, 1.0f) * 255.0f);
            pixels[idx + 2] = static_cast<uint8_t>(clampf(color.b, 0.0f, 1.0f) * 255.0f);
            pixels[idx + 3] = 255;  // fully opaque
        }
    }
}

std::unique_ptr<ProceduralTexture> ProceduralTexture::generate(
    ProceduralPattern pattern, int width, int height, uint32_t seed)
{
    std::vector<uint8_t> pixels;
    generate_pixels(pattern, pixels, width, height, seed);
    auto texture = std::unique_ptr<ProceduralTexture>(
        new ProceduralTexture(std::move(pixels), width, height));
    // Use default texture_scale (0.0f) so procedural equirectangular textures
    // go through the equirectangular sampling path which correctly reflects
    // sphere rotation via object-space UV coordinates.
    return texture;
}

} // namespace nwave
