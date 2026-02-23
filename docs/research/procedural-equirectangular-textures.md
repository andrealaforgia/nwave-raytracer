# Procedural Equirectangular Textures for Ray Tracing

**Research Date:** 2026-02-22
**Topic:** Procedural generation of equirectangular texture maps for use in a C++ Metal GPU ray tracer
**Confidence Rating:** High (8+ sources per major section, algorithms well-documented in academic and practitioner literature)

---

## Table of Contents

1. [Equirectangular Projection Mathematics](#1-equirectangular-projection-mathematics)
2. [Procedural Noise Algorithms Survey](#2-procedural-noise-algorithms-survey)
3. [Reference Project Analysis: texture-generator](#3-reference-project-analysis-texture-generator)
4. [Texture-Specific Generation Algorithms](#4-texture-specific-generation-algorithms)
5. [Implementation Strategy for nWave Ray Tracer](#5-implementation-strategy-for-nwave-ray-tracer)
6. [C/C++ Noise Libraries](#6-cc-noise-libraries)
7. [Recommended Approach](#7-recommended-approach)
8. [Source Analysis](#8-source-analysis)
9. [Knowledge Gaps](#9-knowledge-gaps)

---

## 1. Equirectangular Projection Mathematics

### What Is Equirectangular Projection?

Equirectangular projection (also called latitude-longitude or lat/lon mapping) is a cylindrical map projection that maps a sphere's surface onto a rectangle. Meridians and circles of latitude are projected as straight, evenly-spaced lines, resulting in images with a 2:1 aspect ratio (width = 2 x height).

**Sources:** [Wikipedia: Equirectangular projection](https://en.wikipedia.org/wiki/Equirectangular_projection), [WebGPU Unleashed: Equirectangular Rendering](https://shi-yan.github.io/webgpuunleashed/Advanced/equirectangular_rendering.html), [PanoTools Wiki](https://wiki.panotools.org/Equirectangular_Projection)

### UV-to-Spherical Coordinate Mapping

Given normalized texture coordinates (u, v) in [0, 1]:

```
theta = v * pi          // polar angle (0 at north pole, pi at south pole)
phi   = u * 2 * pi      // azimuthal angle (0 to 2*pi around equator)
```

Converting spherical to 3D Cartesian coordinates on a unit sphere:

```
x = cos(phi) * sin(theta)
y = cos(theta)                // or -cos(theta), depending on convention
z = sin(phi) * sin(theta)
```

### Inverse Mapping (3D Point to UV)

Given a point on a unit sphere (or outward normal for a sphere of any radius):

```
theta = acos(-y)                        // or acos(y) depending on Y-up convention
phi   = atan2(-z, x) + pi              // shift to [0, 2*pi]
u     = phi / (2 * pi)
v     = theta / pi                      // or 1 - theta/pi for V-flip
```

This is exactly what the nWave shader already implements in `sphere_uv()`:

```metal
void sphere_uv(float3 outward_normal, thread float& u, thread float& v) {
    float theta = acos(-outward_normal.y);
    float phi = atan2(-outward_normal.z, outward_normal.x) + M_PI_F;
    u = phi / (2.0f * M_PI_F);
    v = 1.0f - theta / M_PI_F;
}
```

### Key Properties

- **Horizontal wrapping**: u=0 and u=1 represent the same meridian; textures must tile horizontally.
- **Pole distortion**: Pixels near v=0 (north pole) and v=1 (south pole) are compressed. A horizontal row of pixels at the pole maps to a single point on the sphere.
- **Area distortion**: Regions near the poles are stretched horizontally. This is inherent to the projection and means procedural patterns should compensate, or 3D noise sampling should be used to avoid visible distortion.
- **Aspect ratio**: Standard equirectangular maps have width:height = 2:1.

**Sources:** [WebGPU Unleashed](https://shi-yan.github.io/webgpuunleashed/Advanced/equirectangular_rendering.html), [Wikipedia: UV mapping](https://en.wikipedia.org/wiki/UV_mapping), [Toni Sagrista: Procedural Planetary Surfaces](https://tonisagrista.com/blog/2021/procedural-planetary-surfaces/)

### Avoiding Seams and Pole Artifacts with 3D Noise

The critical insight for procedural equirectangular textures is: **sample 3D noise using the Cartesian (x,y,z) coordinates on the sphere surface, not the 2D (u,v) coordinates**. This eliminates:

1. The **seam** at u=0/u=1 (the atan2 discontinuity)
2. The **pole pinching** artifact where 2D patterns compress

The texture-generator project and multiple academic sources confirm this approach:

```
// For each pixel (px, py) in the equirectangular image:
u = px / width
v = py / height
theta = v * pi
phi = u * 2 * pi
x = cos(phi) * sin(theta)
y = cos(theta)
z = sin(phi) * sin(theta)
color = evaluate_procedural_texture(x, y, z)
```

**Sources:** [Toni Sagrista](https://tonisagrista.com/blog/2021/procedural-planetary-surfaces/), [TouchDesigner Forum](https://forum.derivative.ca/t/procedural-noise-texture-on-sphere/623132), [Screaming Brain Studios](https://screamingbrainstudios.com/spherical-textures/)

---

## 2. Procedural Noise Algorithms Survey

### 2.1 Perlin Noise (Gradient Noise)

**Inventor:** Ken Perlin (1985, improved in 2002)

**Algorithm:**
1. Define a lattice of integer grid points in N-dimensional space
2. Assign a pseudorandom gradient vector to each lattice point (using a permutation table and hash function)
3. For a query point, find the surrounding lattice cell (2^N corners)
4. Compute dot products between gradient vectors and offset vectors from corners to query point
5. Interpolate using a smooth fade function

**Improved Perlin (2002) changes:**
- Fade function upgraded from `3t^2 - 2t^3` to `6t^5 - 15t^4 + 10t^3` (zero first AND second derivatives at boundaries)
- Reduced gradient set: 12 vectors at cube edge midpoints instead of 256 random gradients
- Eliminates directional artifacts ("splotchy" appearance of original)

**Output range:** [-1, 1] (can be remapped to [0, 1] via `(n + 1) / 2`)

**Sources:** [Wikipedia: Perlin noise](https://en.wikipedia.org/wiki/Perlin_noise), [NVIDIA GPU Gems Ch.5](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-5-implementing-improved-perlin-noise), [PBR Book: Noise](https://pbr-book.org/3ed-2018/Texture/Noise), [Rtouti: Perlin Noise Algorithm](https://rtouti.github.io/graphics/perlin-noise-algorithm)

### 2.2 Simplex Noise

**Inventor:** Ken Perlin (2001)

**Key differences from Perlin noise:**
- Uses simplicial grid (triangles in 2D, tetrahedra in 3D) instead of hypercubes
- Fewer multiplications: O(N^2) complexity vs O(2^N) for classic Perlin
- Better isotropy (less directional bias)
- No visible axis-aligned artifacts

**Caution:** Simplex noise in 3D and above was patented by Perlin (US Patent 6,867,776, expired 2022). Open-source alternatives include OpenSimplex and OpenSimplex2.

**Sources:** [Wikipedia: Perlin noise (Simplex section)](https://en.wikipedia.org/wiki/Perlin_noise), [A Survey of Procedural Noise Functions (Lagae et al.)](https://www.cs.umd.edu/~zwicker/publications/SurveyProceduralNoise-CGF10.pdf), [Toni Sagrista](https://tonisagrista.com/blog/2021/procedural-planetary-surfaces/)

### 2.3 Value Noise

**Algorithm:** Simpler than gradient noise. Assigns random scalar values (not gradient vectors) to lattice points and interpolates between them.

- Cheaper to compute than Perlin noise
- Lower visual quality: more "blobby" and has visible grid alignment
- Useful when computation budget is tight or combined with other techniques

**Sources:** [Toni Sagrista](https://tonisagrista.com/blog/2021/procedural-planetary-surfaces/), [Book of Shaders](https://thebookofshaders.com/11/)

### 2.4 Worley / Voronoi / Cellular Noise

**Inventor:** Steven Worley (1996)

**Algorithm:**
1. Scatter feature points randomly through space (one per cell in a grid, with jittered positions)
2. For a query point, find the distances to the N nearest feature points
3. Output a function of those distances (F1 = nearest, F2 = second nearest, F2-F1, etc.)

**Produces:** Cell-like patterns resembling biological tissue, cracked earth, stone, craters, scales, cobblestones

**Key for moon/crater textures:** F1 distance creates circular depressions naturally. Inverted F1 values create bump-like craters.

**Sources:** [A Survey of Procedural Noise Functions](https://www.cs.umd.edu/~zwicker/publications/SurveyProceduralNoise-CGF10.pdf), [PBR Book: Noise](https://pbr-book.org/3ed-2018/Texture/Noise), [GameIdea: Noise Functions](https://gameidea.org/2023/12/16/noise-functions/)

### 2.5 Fractal Brownian Motion (FBM)

**Not a noise function itself** -- it is a technique for combining multiple octaves of any noise function to create fractal detail.

**Algorithm:**

```cpp
float fbm(float3 p, int octaves, float lacunarity, float persistence) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise3d(p * frequency);
        frequency *= lacunarity;   // typically 2.0
        amplitude *= persistence;  // typically 0.5
    }
    return value;
}
```

**Parameters:**
- **Octaves**: Number of noise layers (4-8 typical; more = finer detail)
- **Lacunarity**: Frequency multiplier per octave (2.0 standard, 1.99 reduces regularity artifacts per PBR Book)
- **Persistence (Gain)**: Amplitude multiplier per octave (0.5 standard)

**Sources:** [Book of Shaders: FBM](https://thebookofshaders.com/13/), [PBR Book: Noise](https://pbr-book.org/3ed-2018/Texture/Noise), [Toni Sagrista](https://tonisagrista.com/blog/2021/procedural-planetary-surfaces/)

### 2.6 Turbulence

A variant of FBM that uses absolute values of signed noise, creating sharp "valley" features:

```cpp
float turbulence(float3 p, int octaves, float lacunarity, float persistence) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * abs(noise3d(p * frequency));
        frequency *= lacunarity;
        amplitude *= persistence;
    }
    return value;
}
```

The `abs()` introduces first-derivative discontinuities that create vein-like and craggy features, essential for marble and mountain terrain.

**Sources:** [PBR Book: Noise](https://pbr-book.org/3ed-2018/Texture/Noise), [Lodev: Texture Generation](https://lodev.org/cgtutor/randomnoise.html), [Scratchapixel: Procedural Patterns](https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/procedural-patterns-noise-part-1/simple-pattern-examples.html)

### 2.7 Ridged Multifractal

Inverts turbulence valleys into sharp ridges:

```cpp
float ridged(float3 p, int octaves, float lacunarity, float persistence) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    float weight = 1.0;
    for (int i = 0; i < octaves; i++) {
        float signal = 1.0 - abs(noise3d(p * frequency));
        signal *= signal;  // sharpen the ridge
        signal *= weight;
        weight = clamp(signal * 2.0, 0.0, 1.0);
        value += signal * amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
    }
    return value;
}
```

Excellent for mountain ridges, rocky terrain, and dramatic landscape features.

**Sources:** [PBR Book: Noise](https://pbr-book.org/3ed-2018/Texture/Noise), [Book of Shaders: FBM](https://thebookofshaders.com/13/)

### 2.8 Domain Warping

Applies FBM to warp the input coordinates of another FBM evaluation, creating complex, organic, cloud-like patterns:

```cpp
float domain_warped(float3 p) {
    float3 q = float3(
        fbm(p + float3(0.0, 0.0, 0.0)),
        fbm(p + float3(5.2, 1.3, 0.0)),
        fbm(p + float3(1.7, 9.2, 0.0))
    );
    return fbm(p + 4.0 * q);
}
```

**Sources:** [Book of Shaders: FBM](https://thebookofshaders.com/13/), [Inigo Quilez (referenced in multiple sources)]

---

## 3. Reference Project Analysis: texture-generator

**Project:** [boytchev/texture-generator](https://github.com/boytchev/texture-generator) (MIT license)
**Live demo:** [https://boytchev.github.io/texture-generator/](https://boytchev.github.io/texture-generator/)
**Version:** v1.7.1 (January 2025)
**Language:** JavaScript (Three.js ecosystem), available as npm package `pet-gen`

### Architecture

The generator follows a clear pipeline:

1. **Pixel iteration**: Loop over all pixels (px, py) in the output image
2. **UV computation**: `u = px / width`, `v = py / height`
3. **Spherical-to-Cartesian conversion**: `vector.setFromSphericalCoords(1, PI * v, 2 * PI * u)`
4. **Pattern evaluation**: `pattern(x, y, z, color, options, u, v, px, py)` -- each pattern receives 3D coordinates
5. **Color output**: Pattern writes to color object, which is drawn to canvas

**Key design decision**: All pattern functions receive 3D (x, y, z) coordinates on a unit sphere. This ensures seamless textures with no seam at the date line and no pole distortion in the pattern itself (though pixels near poles are still compressed in the image).

### Available Texture Types (20 patterns)

Camouflage, Cave art, Clouds, Concrete, Entangled, Fordite, Grid, Isolines, Marble, Neon lights, Planet, Photosphere, Polka dots, Satin, Scepter head, Scream, Simplex noise, Stars, Water drops, Zebra lines

### Noise Implementation

The project wraps Three.js `SimplexNoise` (3D simplex noise) behind a simple API:

```javascript
noise(x, y, z, scale)  // returns simplex.noise3d(scale*x, scale*y, scale*z)
noiseSeed(seed)         // re-seeds the simplex noise generator
```

All texture variety comes from how patterns compose and transform this single 3D simplex noise function.

### Planet Pattern Analysis

The planet texture algorithm from the source code:

1. **Multi-octave noise**: Iterates with `power *= 0.8` and `scale *= 1.5` (non-standard lacunarity/persistence for organic feel)
2. **Adaptive octave count**: `iterations = max(4, 4 + log2(width))` -- more octaves at higher resolution
3. **Elevation-to-biome color mapping**:
   - Deep water (SteelBlue) -> Shallow water (SkyBlue) -> Beach (LemonChiffon) -> Grass -> Forest (SeaGreen) -> Rock -> Snow (Azure)
   - Colors interpolated linearly between adjacent bands
   - Water regions get randomized luminosity shifts for organic variation

### Marble Pattern Analysis

1. **Multi-frequency noise sum**: `noise(x,y,z) + 0.5*noise(x,y,z,2) + 0.1*noise(x,y,z,6)`
2. **Power compression**: `k = 1 - abs(k)^2.5` creates sharp vein boundaries
3. **Threshold smoothing**: Conditional logic with `(k-min)/(max-min))^20` for crisp vein edges
4. **Directional modulation**: Multiply by `noise(z, -x, y, 0.5)` using rotated coordinates for complexity
5. **Color interpolation**: Linear blend between background and foreground marble colors

### Applicability to C++

The patterns are algorithm-based, not library-dependent. They can be directly ported to C++ or Metal shader code. The core dependency is a 3D noise function, which has many C/C++ implementations available.

**Sources:** [texture-generator GitHub](https://github.com/boytchev/texture-generator), [texture-generator demo](https://boytchev.github.io/texture-generator/), source code analysis of `generator.js`, `noise.js`, `patterns/planet.js`, `patterns/marble.js`

---

## 4. Texture-Specific Generation Algorithms

### 4.1 Earth-like Planet

**Technique**: Multi-octave FBM noise mapped through elevation-to-biome color bands.

```cpp
// Pseudocode for planet texture
float3 planet_color(float3 p, int seed) {
    float elevation = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    int octaves = 6;

    for (int i = 0; i < octaves; i++) {
        elevation += amplitude * noise3d(p * frequency, seed);
        amplitude *= 0.5;     // persistence
        frequency *= 2.0;     // lacunarity
    }

    // Map elevation to biome colors
    if (elevation < -0.1) return DEEP_OCEAN;       // dark blue
    if (elevation < 0.0)  return SHALLOW_WATER;     // light blue
    if (elevation < 0.02) return BEACH;             // sandy yellow
    if (elevation < 0.3)  return lerp(GRASS, FOREST, (elevation - 0.02) / 0.28);
    if (elevation < 0.6)  return lerp(FOREST, ROCK, (elevation - 0.3) / 0.3);
    return lerp(ROCK, SNOW, (elevation - 0.6) / 0.4);
}
```

**Advanced: Two-axis color lookup** (used by Space Nerds in Space): Use elevation for one axis and a separate humidity/temperature FBM for the second axis, indexing into a 2D biome lookup table. This produces more realistic biome distribution.

**Sources:** [Space Nerds in Space](https://smcameron.github.io/space-nerds-in-space/earthlike/example/slideshow.html), [Toni Sagrista](https://tonisagrista.com/blog/2021/procedural-planetary-surfaces/), [texture-generator planet.js](https://github.com/boytchev/texture-generator), [Pistol Shrimp Games](https://pistolshrimpgames.com/2024/01/dev-diary-procedural-planet-art/)

### 4.2 Marble / Stone

**Technique**: Sine function with noise-perturbed phase.

```cpp
float3 marble_color(float3 p) {
    float t = turbulence(p, 5, 2.0, 0.5);  // 5-octave turbulence

    // Sine wave with noise perturbation creates veins
    float vein = sin(p.x * 8.0 + t * 10.0);

    // Remap from [-1,1] to [0,1]
    vein = (vein + 1.0) * 0.5;

    // Sharpen veins with power function
    vein = pow(vein, 0.8);

    // Interpolate between marble colors
    float3 dark = float3(0.1, 0.1, 0.15);   // dark vein
    float3 light = float3(0.9, 0.88, 0.85);  // white marble
    return lerp(dark, light, vein);
}
```

**Key formula**: `marble(x,y,z) = sin(f * (x + a * turbulence(x,y,z)))` where:
- `f` = vein frequency (controls spacing)
- `a` = turbulence amplitude (controls how wavy the veins are)

**Sources:** [Lodev: Random Noise Textures](https://lodev.org/cgtutor/randomnoise.html), [Scratchapixel: Procedural Patterns](https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/procedural-patterns-noise-part-1/simple-pattern-examples.html), [PBR Book: Noise](https://pbr-book.org/3ed-2018/Texture/Noise), [WKU Lecture Notes](https://people.wku.edu/qi.li/teaching/446/cg13_texturing.pdf)

### 4.3 Wood Grain

**Technique**: Concentric rings based on cylindrical distance, perturbed by noise.

```cpp
float3 wood_color(float3 p) {
    // Distance from Y axis (cylindrical distance = ring pattern)
    float dist = sqrt(p.x * p.x + p.z * p.z);

    // Add noise perturbation (small amplitude to preserve rings)
    float t = turbulence(p, 4, 2.0, 0.5);
    dist += t * 0.15;

    // Create ring pattern
    float ring = sin(dist * 25.0);  // 25 = number of rings
    ring = (ring + 1.0) * 0.5;

    // Wood colors
    float3 light_wood = float3(0.65, 0.45, 0.25);
    float3 dark_wood  = float3(0.4, 0.25, 0.12);
    return lerp(dark_wood, light_wood, ring);
}
```

**Alternative (Scratchapixel method)**: Multiply noise by a large factor and take fractional part:
```
g = noise(p) * 10.0
wood = g - floor(g)
```
This creates discontinuities that look like grain boundaries.

**Sources:** [Lodev: Random Noise Textures](https://lodev.org/cgtutor/randomnoise.html), [Gorilla Sun: Radial Perlin Noise](https://www.gorillasun.de/blog/radial-perlin-noise-and-generative-tree-rings/), [Scratchapixel](https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/procedural-patterns-noise-part-1/simple-pattern-examples.html)

### 4.4 Moon / Rocky Surface

**Technique**: Combine FBM base terrain with Worley noise craters.

```cpp
float3 moon_color(float3 p) {
    // Base terrain: FBM for general bumpiness
    float terrain = fbm(p, 6, 2.0, 0.5) * 0.3;

    // Craters: inverted Worley F1 distance
    float crater = 1.0 - worley_f1(p * 3.0);
    crater = pow(crater, 3.0);  // sharpen crater edges

    // Combine
    float height = terrain - crater * 0.4;

    // Gray-scale moon surface
    float brightness = 0.4 + height * 0.3;
    brightness = clamp(brightness, 0.15, 0.65);
    return float3(brightness, brightness * 0.98, brightness * 0.95);
}
```

**Sources:** [Survey of Procedural Noise Functions (Lagae)](https://www.cs.umd.edu/~zwicker/publications/SurveyProceduralNoise-CGF10.pdf), [DukeBWard/flatMoonSurfaceSimulation](https://github.com/DukeBWard/flatMoonSurfaceSimulation), [CGVR Bremen: Procedural Asteroids](https://cgvr.cs.uni-bremen.de/research/procedural_asteroid/)

### 4.5 Clouds

**Technique**: Turbulence-based opacity with white/blue color mapping.

```cpp
float3 cloud_color(float3 p, float3 sky_blue) {
    float density = turbulence(p * 2.0, 6, 2.0, 0.5);

    // Threshold for cloud/sky boundary
    density = smoothstep(0.3, 0.7, density);

    float3 white = float3(1.0, 1.0, 1.0);
    return lerp(sky_blue, white, density);
}
```

**Sources:** [Lodev: Random Noise Textures](https://lodev.org/cgtutor/randomnoise.html), [Space Nerds in Space](https://smcameron.github.io/space-nerds-in-space/earthlike/example/slideshow.html)

### 4.6 Abstract / Domain-Warped Patterns

**Technique**: Nested FBM (domain warping) creates complex organic structures.

```cpp
float3 abstract_color(float3 p) {
    float3 q = float3(
        fbm(p + float3(0.0, 0.0, 0.0), 4, 2.0, 0.5),
        fbm(p + float3(5.2, 1.3, 0.0), 4, 2.0, 0.5),
        fbm(p + float3(1.7, 9.2, 0.0), 4, 2.0, 0.5)
    );
    float f = fbm(p + 4.0f * q, 4, 2.0, 0.5);

    // Map to color palette
    float t = clamp(f * 0.5 + 0.5, 0.0, 1.0);
    return color_palette(t);  // user-defined gradient
}
```

**Sources:** [Book of Shaders: FBM](https://thebookofshaders.com/13/), [Inigo Quilez articles]

---

## 5. Implementation Strategy for nWave Ray Tracer

### Current Architecture

The nWave ray tracer has this texture pipeline:

1. **YAML scene loader** parses `image_texture` material type, calls `ImageTexture::load_from_file(path)`
2. **ImageTexture** stores RGBA pixel data in `std::vector<uint8_t>` with width/height
3. **SceneFlattener** packs all texture pixel data into a single `texture_data` byte buffer, recording byte offsets per material
4. **Metal shader** samples textures via `sample_texture()` using UV coordinates and byte offsets

### Two Implementation Approaches

#### Approach A: CPU-Side Pixel Buffer Generation (Recommended)

Generate the procedural texture on the CPU as an RGBA pixel buffer (replacing the file-loading step). The rest of the pipeline remains unchanged.

**Advantages:**
- Zero changes to SceneFlattener, GPUMaterial, or Metal shader
- Same bilinear interpolation and texture sampling code works
- Easy to debug (can write generated textures to files for inspection)
- Supports all existing texture mapping modes (equirectangular, triplanar, cube map, dual-hemisphere)
- Can pre-compute at any desired resolution

**Disadvantages:**
- Memory usage scales with resolution (a 4096x2048 RGBA texture = 32 MB)
- Texture must be regenerated if parameters change
- Fixed resolution (no infinite zoom)

**Implementation sketch:**

```cpp
// New class: ProceduralTexture (or extend ImageTexture)
class ProceduralTexture : public ImageTexture {
public:
    enum class Type { Planet, Marble, Wood, Moon, Clouds, Abstract };

    static std::unique_ptr<ProceduralTexture> generate(
        Type type, int width, int height, uint32_t seed);

private:
    static void generate_pixels(
        Type type, std::vector<uint8_t>& pixels,
        int width, int height, uint32_t seed);
};

// Generation function
void ProceduralTexture::generate_pixels(
    Type type, std::vector<uint8_t>& pixels,
    int width, int height, uint32_t seed)
{
    pixels.resize(width * height * 4);

    for (int py = 0; py < height; py++) {
        for (int px = 0; px < width; px++) {
            // UV coordinates
            float u = float(px) / float(width);
            float v = float(py) / float(height);

            // Convert to 3D point on unit sphere
            float theta = v * M_PI;
            float phi = u * 2.0f * M_PI;
            float x = cosf(phi) * sinf(theta);
            float y = cosf(theta);
            float z = sinf(phi) * sinf(theta);

            // Evaluate procedural pattern
            float3 color = evaluate_pattern(type, {x, y, z}, seed);

            // Write RGBA
            int idx = (py * width + px) * 4;
            pixels[idx + 0] = uint8_t(clamp(color.x, 0.0f, 1.0f) * 255.0f);
            pixels[idx + 1] = uint8_t(clamp(color.y, 0.0f, 1.0f) * 255.0f);
            pixels[idx + 2] = uint8_t(clamp(color.z, 0.0f, 1.0f) * 255.0f);
            pixels[idx + 3] = 255;  // fully opaque
        }
    }
}
```

**YAML integration:**

```yaml
materials:
  earth:
    type: procedural_texture
    pattern: planet
    width: 4096
    height: 2048
    seed: 42
```

#### Approach B: GPU-Side Procedural Evaluation (Metal Shader)

Evaluate procedural patterns directly in the Metal shader using the UV/3D coordinates at intersection time. No pixel buffer needed.

**Advantages:**
- Zero memory overhead for textures
- Infinite resolution (pattern is computed at the exact intersection point)
- Dynamic (can animate with time parameter)

**Disadvantages:**
- Requires implementing noise functions in Metal Shading Language
- Increases shader complexity and compilation time
- Noise evaluation per ray-hit adds to render time (each sample requires multiple hash lookups and interpolations)
- Must modify GPUMaterial struct (add procedural type fields instead of texture offset)
- Must modify shader dispatch, scene flattener, and material handling
- Harder to debug (no visual preview without rendering)

**Implementation sketch (Metal):**

```metal
// In ray_trace.metal -- add procedural texture type
constant uint PROC_PLANET  = 1;
constant uint PROC_MARBLE  = 2;
constant uint PROC_WOOD    = 3;
constant uint PROC_MOON    = 4;

// Hash function for noise
float hash3d(float3 p) {
    p = fract(p * float3(443.8975, 397.2973, 491.1871));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

// 3D value noise
float noise3d(float3 p) {
    float3 i = floor(p);
    float3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);  // Hermite smoothing

    return mix(
        mix(mix(hash3d(i + float3(0,0,0)), hash3d(i + float3(1,0,0)), f.x),
            mix(hash3d(i + float3(0,1,0)), hash3d(i + float3(1,1,0)), f.x), f.y),
        mix(mix(hash3d(i + float3(0,0,1)), hash3d(i + float3(1,0,1)), f.x),
            mix(hash3d(i + float3(0,1,1)), hash3d(i + float3(1,1,1)), f.x), f.y),
        f.z);
}

// FBM
float fbm(float3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise3d(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}
```

### Performance Comparison

| Factor | CPU Pixel Buffer (A) | GPU Shader (B) |
|--------|---------------------|----------------|
| Memory | 32 MB per 4K texture | ~0 |
| Render-time cost | Bilinear sample (fast) | 6-8 octave noise eval per hit (moderate) |
| Setup cost | Generation time (one-time) | None |
| Flexibility | Fixed resolution | Infinite detail |
| Integration effort | Low (extends ImageTexture) | High (shader + pipeline changes) |
| Debug ability | Can save as PNG | Render-only |

**Sources:** [NVIDIA GPU Gems Ch.5](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-5-implementing-improved-perlin-noise), [NVIDIA GPU Gems 2 Ch.26](https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-26-implementing-improved-perlin-noise), [NoiseKit (Metal noise)](https://github.com/rudedogg/NoiseKit), [Comprehensive GPU Noise Article](https://peerdh.com/blogs/programming-insights/a-comprehensive-look-at-procedural-noise-algorithms-in-gpu-shaders)

---

## 6. C/C++ Noise Libraries

### 6.1 FastNoiseLite (Recommended)

- **URL**: [https://github.com/Auburn/FastNoiseLite](https://github.com/Auburn/FastNoiseLite)
- **License**: MIT
- **Language**: Single-header C++98 compatible (also C99, HLSL, GLSL, and 12+ other languages)
- **Noise types**: OpenSimplex2, OpenSimplex2S, Perlin, Value, Value Cubic, Cellular (Worley)
- **Fractal types**: FBM, Ridged, PingPong, Domain Warp (OpenSimplex2-based, Grid Gradient)
- **Dimensions**: 2D and 3D
- **Integration**: Single header file, zero dependencies
- **Key advantage**: Supports domain warping natively; most comprehensive feature set

### 6.2 stb_perlin.h

- **URL**: [https://github.com/nothings/stb/blob/master/stb_perlin.h](https://github.com/nothings/stb/blob/master/stb_perlin.h)
- **License**: Public domain / MIT
- **Language**: Single-header C (stb-style)
- **Noise types**: Improved Perlin noise (2002)
- **Fractal functions**: `stb_perlin_fbm_noise3`, `stb_perlin_turbulence_noise3`, `stb_perlin_ridge_noise3`
- **Note**: The nWave project already uses stb (stb_image.h), so this is a natural fit
- **Key advantage**: Already in the project's dependency family; simplest integration

### 6.3 siv::PerlinNoise

- **URL**: [https://github.com/Reputeless/PerlinNoise](https://github.com/Reputeless/PerlinNoise)
- **License**: MIT
- **Language**: Single-header C++17/C++20
- **Noise types**: Improved Perlin noise
- **Features**: 1D/2D/3D noise, octave variants, [0,1] and [-1,1] output, seed support, serialization
- **Key advantage**: Clean modern C++ API

### 6.4 libnoise

- **URL**: [https://libnoise.sourceforge.net/](https://libnoise.sourceforge.net/)
- **License**: LGPL
- **Language**: C++ library (requires linking)
- **Noise types**: Perlin, Ridged Multifractal, Billow, Voronoi
- **Features**: Module-based composition pipeline, extensive documentation with tutorials
- **Key disadvantage**: LGPL license may complicate static linking; heavier integration than header-only

### 6.5 FastNoise2

- **URL**: [https://github.com/Auburn/FastNoise2](https://github.com/Auburn/FastNoise2)
- **License**: MIT
- **Language**: C++17 with SIMD acceleration
- **Features**: Node-graph based, SIMD-optimized batch evaluation, all noise types
- **Key advantage**: Highest performance for bulk generation (useful if generating many large textures)
- **Key disadvantage**: More complex integration than FastNoiseLite; requires build system integration

### Library Comparison for This Project

| Library | Integration | Features | Performance | License | Recommendation |
|---------|------------|----------|-------------|---------|---------------|
| FastNoiseLite | Single header | All noise types + domain warp | Good | MIT | Best overall choice |
| stb_perlin.h | Single header | Perlin + FBM/turbulence/ridge | Good | Public domain | Best if Perlin-only is sufficient |
| siv::PerlinNoise | Single header | Perlin + octaves | Good | MIT | Best C++17 API |
| libnoise | Library | All types + modules | Good | LGPL | Avoid (license) |
| FastNoise2 | Library | All types + SIMD | Best | MIT | Overkill for this use case |

**Sources:** Each library's GitHub repository as listed above

---

## 7. Recommended Approach

### Primary Recommendation: Approach A (CPU Pixel Buffer) + FastNoiseLite

**Rationale:**

1. **Minimal pipeline disruption**: The existing texture pipeline (ImageTexture -> SceneFlattener -> GPU buffer -> Metal sampling) is proven and handles equirectangular projection correctly. CPU-generated textures plug into this pipeline with zero shader changes.

2. **Library choice**: FastNoiseLite provides all needed noise types (Perlin, Simplex, Cellular/Worley) with FBM, ridged, and domain warping built in, as a single MIT-licensed header file.

3. **Alternative**: stb_perlin.h is even simpler if only Perlin-based textures (marble, wood, planet) are needed. The project already depends on stb_image.h, making this nearly zero-cost to add.

### Implementation Plan

**Step 1: Add noise library**
- Drop `FastNoiseLite.h` (or `stb_perlin.h`) into `external/` or `src/domain/materials/`

**Step 2: Create ProceduralTexture class**
- Subclass or companion of `ImageTexture`
- Constructor takes: pattern type, width, height, seed, and pattern-specific parameters
- Generates RGBA pixel buffer in constructor using equirectangular 3D sampling

**Step 3: Add YAML support**
- New material type `procedural_texture` in `yaml_scene_loader.cpp`
- Parameters: `pattern`, `width`, `height`, `seed`, plus pattern-specific options

**Step 4: Implement patterns**
- Start with planet (most visually impactful)
- Add marble, wood, moon as subsequent patterns
- Each pattern is a function: `float3 pattern(float3 sphere_point, seed, options) -> RGB color`

**Step 5: Optional -- shader-side procedural textures**
- As a future optimization, add procedural noise to the Metal shader for specific patterns
- This can coexist with CPU-generated textures (use a new material type flag in GPUMaterial)

### Suggested Resolution Guidelines

| Use Case | Resolution | Memory | Quality |
|----------|-----------|--------|---------|
| Preview / test | 1024 x 512 | 2 MB | Visible pixelation at close range |
| Standard | 2048 x 1024 | 8 MB | Good for most scenes |
| High quality | 4096 x 2048 | 32 MB | Publication quality |
| Ultra | 8192 x 4096 | 128 MB | Extreme close-ups only |

For a 4K render (3840x2160), a 4096x2048 procedural texture provides roughly 1:1 texel-to-pixel mapping for a sphere filling half the frame.

---

## 8. Source Analysis

| Source | Type | Reputation | Used For |
|--------|------|-----------|----------|
| [PBR Book (Pharr, Jakob, Humphreys)](https://pbr-book.org/3ed-2018/Texture/Noise) | Academic textbook | Tier 1 | Noise algorithms, FBM, turbulence |
| [NVIDIA GPU Gems (Ch.5)](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-5-implementing-improved-perlin-noise) | Industry reference | Tier 1 | Improved Perlin, GPU noise |
| [Survey of Procedural Noise Functions (Lagae et al.)](https://www.cs.umd.edu/~zwicker/publications/SurveyProceduralNoise-CGF10.pdf) | Peer-reviewed paper | Tier 1 | Comprehensive noise survey |
| [Wikipedia: Perlin noise](https://en.wikipedia.org/wiki/Perlin_noise) | Encyclopedia | Tier 2 | Algorithm overview, history |
| [Wikipedia: Equirectangular projection](https://en.wikipedia.org/wiki/Equirectangular_projection) | Encyclopedia | Tier 2 | Projection definition |
| [Book of Shaders (Gonzalez Vivo)](https://thebookofshaders.com/13/) | Practitioner tutorial | Tier 2 | FBM, domain warping |
| [Toni Sagrista: Procedural Planetary Surfaces](https://tonisagrista.com/blog/2021/procedural-planetary-surfaces/) | Technical blog | Tier 2 | Planet generation, seamless sphere noise |
| [Lodev: Texture Generation](https://lodev.org/cgtutor/randomnoise.html) | Tutorial | Tier 2 | Marble, wood, cloud algorithms |
| [Scratchapixel: Procedural Patterns](https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/procedural-patterns-noise-part-1/simple-pattern-examples.html) | Tutorial | Tier 2 | Marble, wood formulas |
| [Space Nerds in Space](https://smcameron.github.io/space-nerds-in-space/earthlike/example/slideshow.html) | Open-source project | Tier 2 | Earth texture algorithm |
| [boytchev/texture-generator](https://github.com/boytchev/texture-generator) | Open-source project | Tier 2 | Reference implementation, pattern catalog |
| [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) | Library docs | Tier 2 | Noise library features |
| [Red Blob Games: Maps from Noise](https://www.redblobgames.com/maps/terrain-from-noise/) | Tutorial | Tier 2 | Terrain from noise techniques |
| [WebGPU Unleashed: Equirectangular](https://shi-yan.github.io/webgpuunleashed/Advanced/equirectangular_rendering.html) | Tutorial | Tier 2 | Equirectangular rendering math |
| [stb_perlin.h](https://github.com/nothings/stb/blob/master/stb_perlin.h) | Library source | Tier 2 | C noise implementation |

---

## 9. Knowledge Gaps

### 9.1 Worley Noise in Metal Shaders

**Searched:** Metal-specific Worley/Voronoi noise implementations.
**Found:** Limited. NoiseKit (GitHub) exists but is Swift-focused. No production-quality Metal Shading Language Worley noise was found.
**Impact:** Medium. Worley noise is mainly needed for moon/crater textures. If implementing GPU-side procedural textures, a Worley implementation would need to be written from scratch or ported from GLSL.

### 9.2 Performance Benchmarks for GPU Noise

**Searched:** Benchmarks comparing CPU pre-generated textures vs real-time GPU noise evaluation in ray tracers.
**Found:** General statements that GPU noise is "fast" but no specific benchmarks for Metal compute shaders with multi-bounce ray tracing.
**Impact:** Low for Approach A (CPU generation). Would be important if pursuing Approach B (shader-side evaluation).

### 9.3 Anti-Aliasing of Procedural Textures

**Searched:** Techniques for anti-aliasing procedural noise in ray tracers (ray differentials, cone tracing for texture LOD).
**Found:** The PBR Book discusses clamping high-frequency octaves based on Nyquist limits, but detailed Metal-specific implementations were not found.
**Impact:** Low for Approach A (bilinear interpolation in the sampler handles this). Medium for Approach B (would need manual frequency clamping).

### 9.4 Texture-Generator Internal Pattern Algorithms

**Searched:** Source code for all 20 patterns in the texture-generator project.
**Found:** Planet and marble patterns in detail. Other patterns (camouflage, fordite, cave art, scepter head, etc.) were not analyzed as their source files were not individually accessible.
**Impact:** Low. The fundamental technique (3D noise on unit sphere) is consistent across all patterns. Specific pattern recipes can be developed independently using the documented noise composition techniques.

### 9.5 Color Palette Design for Procedural Textures

**Searched:** Systematic approaches to color palette selection for procedural planetary and material textures.
**Found:** Ad-hoc color choices in source code (e.g., "SteelBlue", "LemonChiffon") but no principled methodology.
**Impact:** Low for implementation, but affects visual quality. Could be addressed by sampling color ramps from reference photographs.
