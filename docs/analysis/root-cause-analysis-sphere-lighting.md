# Root Cause Analysis: Hard Lighting Terminator on Moving Spheres and Earth Sphere Lighting Artifacts

**Date**: 2026-02-22
**Investigator**: Rex (Root Cause Analysis Specialist)
**Method**: Toyota 5 Whys, multi-causal investigation (6 branches)
**Investigation Depth**: 5 levels per branch
**Status**: Complete

---

## Problem Statement

Two related but distinct visual defects are observed in the ray tracer animation output:

1. **Issue 1 (Hard Line)**: Moving textured spheres show a hard, clearly-visible line between their lit and unlit halves, instead of the expected smooth Lambertian gradient/falloff.
2. **Issue 2 (Earth Lighting)**: The Earth sphere in the finale sequence appears lit in a weird/incorrect way.

**Scope**: GPU Metal shader pipeline (`ray_trace.metal`), animation rendering pipeline (`animation_renderer.cpp`), scene lighting configuration, Earth finale implementation.

---

## Executive Summary

Six root causes were identified across two issues:

| # | Root Cause | Issue | Severity |
|---|-----------|-------|----------|
| RC-A | Renderer uses infinitely-small point light with single shadow ray -- no area light / soft shadow support | 1 | High |
| RC-B | GPULight struct lacks area extent field, making soft shadows architecturally impossible | 1 | High |
| RC-C | Flat 5% scalar ambient is insufficient to soften the terminator perceptually | 1 | Medium |
| RC-D | Earth finale uses PointLight (with 1/(1+d^2) attenuation) instead of DirectionalLight for sunlight simulation | 2 | Critical |
| RC-E | Bowling scene geometry not cleared before finale -- causes shadow/reflection interference | 2 | High |
| RC-F | Finale ambient (1%) combined with severely attenuated point light produces near-zero illumination | 2 | High |

---

## Evidence Gathered

### E1: Diffuse Lighting Calculation

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, line 861

```metal
float diffuse_factor = max(0.0f, dot(rec.normal, light_dir));
color += attenuation * light_intensity * shadow_transparency * diffuse_factor;
```

This is a standard Lambertian cosine-falloff: `max(0, N dot L)`. The diffuse computation itself is correct and produces a smooth gradient. **The hard line is NOT caused by a step function in the diffuse calculation.**

### E2: Single Point Light with Hard Shadows

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/nwave_bowling.yaml`, lines 337-340

```yaml
lights:
  - type: point
    position: [-4, 10, 2]
    color: [1.0, 0.97, 0.9]
    intensity: 0.7
```

The scene uses exactly one point light. Shadow rays (lines 819-858 of shader) cast a single ray toward the light's infinitesimal position:

```metal
float3 shadow_origin = rec.point + T_MIN * rec.normal;
// ... single shadow ray cast toward light_dir
Ray shadow_ray;
shadow_ray.origin = shadow_origin;
shadow_ray.direction = light_dir;
```

No jittering, no multi-sampling, no area light disk sampling. This produces **binary shadow results**: either fully lit or fully shadowed, with no penumbra.

### E3: Point Light Quadratic Attenuation

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, line 808

```metal
float attenuation = 1.0f / (1.0f + light_dist * light_dist);
light_intensity = light.color * light.intensity * attenuation;
```

The formula `1/(1 + d^2)` provides inverse-square-like attenuation. For the main scene, the light at [-4, 10, 2] illuminating spheres at z~1, the distances are ~10 units, giving attenuation ~0.01. This means effective intensity is `0.7 * 0.01 = 0.007`. This is workable because the ambient adds a baseline.

### E4: Ambient Factor for Main Animation

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, line 263

```cpp
frame_settings.ambient_factor = 0.05f;
```

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, line 1009

```metal
color += throughput * camera.ambient_factor * albedo;
```

The unlit side of a sphere receives only 5% of its albedo as illumination. With the lit side receiving the direct light diffuse contribution, the contrast ratio between the lit and unlit hemispheres is extreme. After gamma correction (sqrt), the display-space values are:
- Lit side: sqrt(diffuse + 0.05) -- depends on light intensity
- Unlit side: sqrt(0.05) = 0.224

The perceptual jump from 0.224 to the lit-side value across the terminator is sharp enough to appear as a hard line, even though the underlying math is a smooth cosine.

### E5: Indirect Lighting at Low SPP

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, lines 1017-1027

```metal
float3 scatter_dir = rec.normal + random_in_unit_sphere(rng_seed);
// ... throughput *= albedo;
```

The shader traces scattered rays for indirect illumination. However, at the SPP values used for animation (typically 1-4), the indirect contribution is extremely noisy and provides no meaningful fill light. In a production renderer, hundreds of samples would smooth this into soft global illumination; at 1 SPP, each pixel gets one random bounce direction.

### E6: Earth Finale -- Sun Light as PointLight

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, lines 448-455

```cpp
// Dim ambient fill light far away
anim_scene.add_light(std::make_shared<PointLight>(
    Point3(0, 50, 0), Color3(1.0, 1.0, 1.0), 0.1));
// Bright "Sun" light from the side
anim_scene.add_light(std::make_shared<PointLight>(
    Point3(10, earth_center.y() + 2.0, -8),
    Color3(1.0, 0.95, 0.85), 1.5));
```

The "Sun" is a point light at position (10, ~4.0, -8). Earth center is at (0, 2.0, 0) with radius 1.5.

Distance from Sun light to Earth center:
```
d = sqrt(10^2 + (4.0 - 2.0)^2 + (-8)^2) = sqrt(100 + 4 + 64) = sqrt(168) = 12.96
```

Attenuation at this distance: `1 / (1 + 12.96^2) = 1 / 169.0 = 0.0059`

Effective intensity: `1.5 * 0.0059 = 0.0089`

The fill light at (0, 50, 0): distance = 48 units, attenuation = `1 / (1 + 2304) = 0.00043`, intensity = `0.1 * 0.00043 = 0.000043` -- negligible.

**The Sun light delivers less than 1% of its configured intensity to the Earth's surface.**

### E7: Earth Finale Ambient Factor

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, line 461

```cpp
finale_settings.ambient_factor = 0.01f;
```

Combined with E6: total illumination on the dark side of Earth = 1% ambient. Total on the lit side = 1% + 0.89% direct = 1.89%. After gamma: sqrt(0.019) = 0.138. The Earth is barely visible at all.

### E8: DirectionalLight Support in Shader

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, lines 810-815

```metal
} else {
    // Directional light: position stores direction toward light
    light_dir = normalize(light.position);
    light_dist = T_MAX;
    light_intensity = light.color * light.intensity;
}
```

The shader already supports directional lights with **no distance attenuation**. The `light_intensity` is applied directly without any `1/(1+d^2)` factor. This is exactly the behavior needed for sunlight.

### E9: Scene Not Cleared Before Finale

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, lines 335-388

```cpp
// ===== EARTH FINALE =====
if (config_.finale.enabled && finale_start < total_frames) {
    // ... load textures ...
    // Line 448: lights ARE cleared
    anim_scene.clear_lights();
    // But shapes are NOT cleared -- bowling objects remain
    // ...
    anim_scene.add_shape(earth_transformed);
    anim_scene.add_shape(moon_transformed);
```

The code calls `anim_scene.clear_lights()` but never `anim_scene.clear_shapes()`. The metallic floor (at y=-0.05 to y=0.0), bowling ball, scattered letter blocks, and all other objects remain in the scene. The metallic floor at y~0 with the Earth center at y=2.0 means:
- The floor reflects the dark sky back up toward the Earth
- Scattered objects at various positions cast unexpected shadows
- The BVH includes many irrelevant objects, potentially affecting traversal

### E10: Earth Texture Mapping Path

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, lines 953-970

The Earth texture loaded via `ImageTexture::load_from_file()` gets `texture_scale = 0.0f` (default). In the shader:

```metal
bool is_equirectangular = (mat.texture_width > mat.texture_height * 1.8f);
```

If the Earth texture file `flat_earth03.jpg` has a ~2:1 aspect ratio (standard equirectangular), this test passes and the equirectangular sampling path is taken:

```metal
albedo = sample_texture(texture_data, mat.texture_offset,
                        mat.texture_width, mat.texture_height,
                        rec.u, rec.v);
```

The UVs are computed from object-space normals (line 670-671), which is correct for a rotating sphere. **The texture mapping itself is correct; the "weird" appearance is caused by the lighting, not the texture.**

### E11: Scene Flattener DirectionalLight Handling

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/gpu/scene_flattener.cpp`, lines 288-296

```cpp
} else if (auto* dir = dynamic_cast<const DirectionalLight*>(light_ptr.get())) {
    gpu_light.light_type = static_cast<uint32_t>(GPULightType::DIRECTIONAL);
    gpu_light.position[0] = static_cast<float>(-dir->direction().x());
    gpu_light.position[1] = static_cast<float>(-dir->direction().y());
    gpu_light.position[2] = static_cast<float>(-dir->direction().z());
```

The scene flattener **negates** the direction when packing for GPU. The shader comment says "position stores direction toward light." This means the GPU convention is: `GPULight.position` for directional lights stores the **toward-light** direction. The `DirectionalLight` class stores the direction light travels **from** (toward the scene), so the flattener correctly negates it. This path is ready to use.

---

## Phase 2: Toyota 5 Whys Analysis

### Issue 1: Hard Line Between Lit and Unlit Halves of Moving Spheres

#### Branch A: Shadow Ray Produces Binary Illumination

| Level | Question | Finding | Evidence |
|-------|----------|---------|----------|
| WHY 1 | Why is there a hard visible line between lit and unlit? | The shadow ray produces a binary lit/unlit result -- fully illuminated or fully in shadow -- with no intermediate penumbra region | E2: single shadow ray per light, no jittering |
| WHY 2 | Why is there no penumbra? | Because the light source is an infinitely-small point, and a single ray is cast toward it -- either it hits an occluder or it doesn't | E2: `shadow_ray.direction = light_dir` -- one direction only |
| WHY 3 | Why is only one shadow ray cast per light? | The light has no spatial extent (no radius/area); there is nothing to jitter toward; the `GPULight` struct has no `radius` field | `GPULight` struct at `gpu_types.h:106` has no radius field |
| WHY 4 | Why was no area light support implemented? | Area lights increase shadow ray count per sample, increasing shader complexity and render time; the initial design prioritized single-sample correctness | Architectural decision; `GPULight` has 16 bytes of reserved/padding space available |
| WHY 5 | **ROOT CAUSE A**: The renderer uses an infinitely-small point light with a single shadow ray per sample. No area light / soft shadow support exists in the architecture. This produces a binary lit/unlit boundary that appears as a hard line even though the underlying diffuse cosine is smooth. | E2: shader lines 819-858; `GPULight` struct lacks radius |

#### Branch B: GPULight Architecture

| Level | Question | Finding | Evidence |
|-------|----------|---------|----------|
| WHY 1 | Why can't shadows be softened? | Soft shadows require casting multiple shadow rays toward different points on a light surface; the light has no surface | E2: single point position per light |
| WHY 2 | Why does the light have no surface? | `GPULight` is a 64-byte struct with fields for position, intensity, and color, but no radius or area extent | `gpu_types.h` lines 106-116 |
| WHY 3 | Why no radius field? | The `GPULight` struct was designed with only point and directional light types; area lights were not in scope | `GPULightType` enum has only POINT and DIRECTIONAL |
| WHY 4 | Why not extend it? | The struct has 16 bytes of reserved/padding space (`_pad0`, `_pad3`, `_reserved`); a radius field could fit at offset 4 (currently `_pad0`) | `gpu_types.h` line 108: `float _pad0` at offset 4 |
| WHY 5 | **ROOT CAUSE B**: The GPULight data structure lacks an area extent field, making soft shadow ray jittering architecturally impossible without struct modification. The padding space at offset 4 is available for this purpose. | `gpu_types.h` lines 106-116 |

#### Branch C: Insufficient Ambient Fill

| Level | Question | Finding | Evidence |
|-------|----------|---------|----------|
| WHY 1 | Why doesn't ambient lighting smooth the terminator? | Ambient is a flat 5% scalar applied uniformly to both sides -- it raises the floor but does not create a gradient across the terminator | E4: `camera.ambient_factor * albedo` at line 1009 |
| WHY 2 | Why does 5% appear as a hard transition? | Human vision is logarithmic. After gamma correction, the display-space ratio is sqrt(lit)/sqrt(0.05) -- if lit-side diffuse adds 0.05 total, the ratio is sqrt(0.10)/sqrt(0.05) = 1.41. But if diffuse adds 0.5, it's sqrt(0.55)/sqrt(0.05) = 3.3 -- still visually abrupt | Calculation: perceptual contrast ratio depends on direct light contribution |
| WHY 3 | Why not use hemisphere-weighted ambient? | The renderer only supports a single scalar `ambient_factor`; there is no concept of ambient direction or hemisphere weighting | `GPUCamera.ambient_factor` is one float at offset 112 |
| WHY 4 | Why was the ambient model kept minimal? | It was designed as a placeholder for indirect illumination; the expectation was that multi-sample indirect bounces would provide fill | E5: indirect bounce exists but is noise at low SPP |
| WHY 5 | **ROOT CAUSE C**: The flat scalar ambient (5%) is too low to perceptually soften the terminator line. Combined with low SPP (no meaningful indirect fill), the dark side receives only 5% illumination, creating a contrast ratio that human vision perceives as a hard edge. | E4, E5 |

---

### Issue 2: Earth Sphere Weird/Incorrect Lighting

#### Branch D: Light Attenuation at Earth Scale

| Level | Question | Finding | Evidence |
|-------|----------|---------|----------|
| WHY 1 | Why does the Earth sphere look weirdly lit? | The "Sun" light intensity is nearly zero at the Earth's surface due to quadratic distance attenuation | E6: effective intensity = 1.5 * 0.0059 = 0.0089 |
| WHY 2 | Why is the light so dim? | The attenuation formula `1/(1+d^2)` was designed for room-scale scenes (1-5 unit distances); at d=13, it eliminates 99.4% of the light | E3: formula at shader line 808 |
| WHY 3 | Why use quadratic attenuation for a sun-like light? | The code uses `PointLight` for the Sun; the PointLight code path always applies `1/(1+d^2)` attenuation; there is no opt-out | E6: `std::make_shared<PointLight>(...)` |
| WHY 4 | Why use PointLight instead of DirectionalLight? | The shader already supports `LIGHT_DIRECTIONAL` with no distance attenuation (E8), and the scene flattener correctly handles it (E11); the finale code simply chose the wrong light type | E6 vs E8: `PointLight` at line 453 vs `DirectionalLight` support at shader line 810 |
| WHY 5 | **ROOT CAUSE D**: The Earth finale uses `PointLight` with quadratic distance attenuation to simulate sunlight. At the Earth's distance from the light (~13 units), the `1/(1+d^2)` formula attenuates the light to less than 1% of its configured intensity. The shader already supports `DirectionalLight` with no distance attenuation, which is the correct light type for simulating sunlight. | E6, E8, E11 |

#### Branch E: Scene Clutter Not Cleared

| Level | Question | Finding | Evidence |
|-------|----------|---------|----------|
| WHY 1 | Why does the Earth look visually "wrong" beyond just dimness? | The bowling scene objects (metallic floor, scattered blocks, bowling ball) remain in the scene during the finale, causing unexpected shadows, reflections, and visual interference | E9: shapes not cleared |
| WHY 2 | Why are old objects still in the scene? | The finale adds Earth and Moon with `anim_scene.add_shape()` without first clearing existing shapes | `animation_renderer.cpp` lines 387-388 |
| WHY 3 | Why not clear shapes? | The code clears lights (`anim_scene.clear_lights()` at line 448) but there is no corresponding `clear_shapes()` call | E9: asymmetric cleanup |
| WHY 4 | Why was shape clearing omitted? | The finale was built as an extension of the main animation loop, reusing the same `anim_scene` object; the bowling objects were not considered as interferents | Code architecture: single `anim_scene` spans both phases |
| WHY 5 | **ROOT CAUSE E**: The Earth finale does not clear prior scene geometry before adding Earth/Moon. The bowling floor (metallic, reflective, at y=0) and ~100+ scattered objects remain in the scene, causing shadow casting, metallic reflections of the dark sky, and visual clutter around the Earth (center y=2.0, radius 1.5, bottom at y=0.5 -- just above the floor). | E9 |

#### Branch F: Combined Low Ambient + Attenuated Light

| Level | Question | Finding | Evidence |
|-------|----------|---------|----------|
| WHY 1 | Why is the Earth's dark side nearly invisible? | Total illumination on the dark side is approximately 1%: the ambient factor is 0.01 and no other light reaches the back hemisphere | E7: `ambient_factor = 0.01f` |
| WHY 2 | Why was 1% ambient chosen? | It was set to simulate the darkness of space; the comment says "dark sky, low ambient" | `animation_renderer.cpp` line 457 |
| WHY 3 | Why does 1% produce an incorrect appearance? | Combined with ROOT CAUSE D (point light delivering <1% intensity), the lit side receives only ~2% total illumination. After gamma: sqrt(0.02) = 0.14. The entire Earth is dimly visible, and the lit/dark contrast is still harsh because the absolute illumination values are so low that the relative difference dominates | E6 + E7: 1% ambient + 0.89% direct = 1.89% lit side |
| WHY 4 | Why weren't these parameters tested together? | The ambient and light intensity were set independently without computing the compound effect of attenuation at the actual scene distances | Missing integration test for finale lighting parameters |
| WHY 5 | **ROOT CAUSE F**: The 1% ambient factor and the severely-attenuated point light compound to produce near-zero total illumination on the Earth. The parameters were set independently without accounting for the `1/(1+d^2)` attenuation formula at d=13 units, which reduces the "1.5 intensity" Sun to an effective 0.009. | E6, E7 |

---

## Phase 3: Backward Chain Validation

### RC-A Forward Trace (Point light + single shadow ray -> hard line)
1. Scene has one point light at [-4, 10, 2] -> infinitely small light source
2. Shadow ray cast as single ray toward light position -> binary hit/miss
3. No penumbra region exists -> illumination transitions from full diffuse to zero in one pixel
4. Ambient (5%) provides a floor on the dark side but the jump from 5% to (5% + diffuse) is perceptually sharp
5. **Result**: Visible hard line at the terminator. **MATCHES observed symptom.**

### RC-B Forward Trace (No radius in GPULight -> no soft shadows possible)
1. GPULight has no `radius` field -> shadow ray has no target area to sample
2. Cannot jitter shadow ray direction -> single sample produces binary result
3. Even with high SPP, shadow is still hard (same direction every sample)
4. **Result**: Architecturally impossible to produce soft shadows. **MATCHES root cause of RC-A.**

### RC-C Forward Trace (5% ambient -> harsh perceptual contrast)
1. Ambient = 0.05 -> dark side of sphere gets 5% albedo
2. Lit side gets 5% + diffuse component -> lit side is significantly brighter
3. Human perception (Weber's law): a 2x-10x luminance jump across 1-2 pixels reads as a "hard edge"
4. After gamma (sqrt): dark=0.22, lit varies by angle but transitions from 0.22 to higher values
5. **Result**: Terminator appears as a visible line even with smooth underlying math. **MATCHES observation.**

### RC-D Forward Trace (PointLight for Sun -> near-zero illumination)
1. Sun created as PointLight at (10, 4, -8) with intensity 1.5
2. Distance to Earth center = 13 units
3. Attenuation = 1/(1 + 169) = 0.0059
4. Effective intensity = 0.0089 -> less than 1% reaches surface
5. Combined with 1% ambient -> total ~2% -> after gamma = 14% display brightness
6. **Result**: Earth is extremely dim; lit/dark contrast is still visible but both sides are dark. **MATCHES "weird/incorrect lighting" observation.**

### RC-E Forward Trace (Scene clutter -> interference)
1. Bowling floor (metal, y=0) remains in scene
2. Earth center at y=2.0, radius 1.5 -> bottom at y=0.5, close to floor
3. Floor reflects dark sky (background_top = [0.02, 0.02, 0.05]) upward toward Earth
4. Scattered box shapes at various positions may partially occlude light paths
5. **Result**: Visual artifacts not attributable to Earth's own geometry. **MATCHES "looks wrong" observation.**

### RC-F Forward Trace (1% ambient + attenuated light -> compound dimness)
1. Ambient = 0.01 -> dark side gets 1% of albedo
2. Point light delivers 0.9% at surface -> lit side gets 1.9%
3. Both sides are extremely dark -> barely visible
4. Gamma: dark = sqrt(0.01) = 0.1, lit = sqrt(0.019) = 0.138
5. Contrast ratio in display space: 0.138/0.1 = 1.38 -> very low contrast, earth looks flat
6. **Result**: Earth appears as a dim, nearly-flat shape with insufficient contrast to show 3D form. **MATCHES observation.**

### Cross-Validation
- RC-A, RC-B, RC-C are independent and collectively explain Issue 1 (hard terminator on rolling spheres)
- RC-D, RC-E, RC-F are independent and collectively explain Issue 2 (Earth lighting)
- RC-C (5% ambient) and RC-F (1% ambient) are the same class of issue at different severity levels
- No contradictions between any root causes
- RC-D is the dominant cause of Issue 2; RC-E and RC-F are amplifying factors

---

## Phase 4: Proposed Solutions

### Immediate Mitigations

#### M1: Switch Earth Sun to DirectionalLight [Fixes RC-D, RC-F]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, lines 452-455

**Before**:
```cpp
// Bright "Sun" light from the side
anim_scene.add_light(std::make_shared<PointLight>(
    Point3(10, earth_center.y() + 2.0, -8),
    Color3(1.0, 0.95, 0.85), 1.5));
```

**After**:
```cpp
// Bright "Sun" directional light -- no distance attenuation (physically correct for sunlight)
// DirectionalLight takes the direction light travels FROM (away from source)
// The scene_flattener negates it for the GPU (toward-light convention)
anim_scene.add_light(std::make_shared<DirectionalLight>(
    Vec3(-10.0, -(earth_center.y() + 2.0 - earth_center.y()), 8.0),
    Color3(1.0, 0.95, 0.85), 1.5));
```

Note: The `DirectionalLight` constructor signature must be verified. The flattener (line 290-292 of `scene_flattener.cpp`) stores `-dir->direction()` into `GPULight.position`, and the shader uses `normalize(light.position)` as `light_dir` (toward the light). So pass the direction that light travels FROM the sun (e.g., from upper-right to lower-left: `Vec3(-1, -0.2, 0.8)` normalized). The exact direction should match the original point light's relative position: from (10, 4, -8) toward (0, 2, 0), so light travels in direction (-10, -2, 8). Normalize this for the DirectionalLight constructor.

Requires adding: `#include "domain/lights/directional_light.h"` at top of file.

**Impact**: Eliminates distance attenuation entirely. Earth receives full 1.5 intensity on the lit side. This is the single highest-impact fix.

#### M2: Clear scene shapes before Earth finale [Fixes RC-E]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`

Add before line 387 (before `anim_scene.add_shape(earth_transformed)`):

```cpp
// Remove bowling scene geometry for clean Earth finale
anim_scene.clear_shapes();
```

Requires adding a `clear_shapes()` method to `Scene` if not present.

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/scene.h`

Add method:
```cpp
void clear_shapes() { shapes_.clear(); }
```

**Impact**: Removes floor reflections, scattered object shadows, and visual clutter.

#### M3: Raise finale ambient to 3% [Fixes RC-F]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, line 461

**Before**:
```cpp
finale_settings.ambient_factor = 0.01f;
```

**After**:
```cpp
finale_settings.ambient_factor = 0.03f;
```

**Impact**: Dark side goes from sqrt(0.01)=10% to sqrt(0.03)=17% display brightness. Enough to see texture detail while maintaining space aesthetic. Combined with M1 (directional light), the lit side is now properly bright.

#### M4: Raise main animation ambient to soften terminator [Fixes RC-C]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, line 263

**Before**:
```cpp
frame_settings.ambient_factor = 0.05f;
```

**After**:
```cpp
frame_settings.ambient_factor = 0.15f;
```

**Impact**: Reduces lit:unlit contrast from ~20:1 to ~7:1. After gamma correction, the terminator transition is much smoother perceptually. Does not require shader changes.

---

### Permanent Fixes

#### P1: Add area light support for soft shadows [Fixes RC-A, RC-B]

**Files affected**:
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/core/gpu_types.h`
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/gpu/scene_flattener.cpp`

Step 1: Add radius to GPULight (use existing padding at offset 4):

**Before** (`gpu_types.h`, line 107-108):
```cpp
uint32_t light_type;      //  0: GPULightType
float    _pad0;           //  4: padding
```

**After**:
```cpp
uint32_t light_type;      //  0: GPULightType
float    radius;           //  4: 0 = point/directional, >0 = area light sphere radius
```

Step 2: In shader, add multi-sample jittering (Metal shader):

**Before** (line 804-808):
```metal
if (light.light_type == LIGHT_POINT) {
    float3 to_light = light.position - rec.point;
    light_dist = length(to_light);
    light_dir = to_light / light_dist;
    float attenuation = 1.0f / (1.0f + light_dist * light_dist);
    light_intensity = light.color * light.intensity * attenuation;
}
```

**After**:
```metal
if (light.light_type == LIGHT_POINT) {
    float3 light_pos = light.position;
    // Jitter toward area light sphere if radius > 0
    if (light.radius > 0.0f) {
        light_pos += random_in_unit_sphere(rng_seed) * light.radius;
    }
    float3 to_light = light_pos - rec.point;
    light_dist = length(to_light);
    light_dir = to_light / light_dist;
    float attenuation = 1.0f / (1.0f + light_dist * light_dist);
    light_intensity = light.color * light.intensity * attenuation;
}
```

Note: This requires passing `rng_seed` into `compute_direct_lighting()`. Since the function does not currently take the RNG seed, the signature must be extended. Alternatively, the jittering can be done in the main kernel before calling the function.

With multi-SPP, each sample jitters to a different point on the light sphere, converging to soft shadows. At 1 SPP, the shadow is still effectively hard per frame, but across animation frames, temporal averaging produces a softer appearance.

#### P2: Hemisphere-weighted ambient [Improves RC-C]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, line 1009

**Before**:
```metal
color += throughput * camera.ambient_factor * albedo;
```

**After**:
```metal
// Hemisphere-weighted ambient: surfaces facing up get more ambient (sky contribution)
float hemisphere_weight = 0.5f * (dot(rec.normal, float3(0, 1, 0)) + 1.0f);
float3 ambient_color = mix(camera.background_bottom, camera.background_top, hemisphere_weight);
color += throughput * camera.ambient_factor * albedo * ambient_color;
```

**Impact**: Surfaces facing the sky receive more ambient than those facing the ground, creating a natural gradient across the terminator region instead of a flat fill. This simulates environment lighting without a full environment map.

---

### Early Detection Measures

1. **Visual regression test**: Render a test sphere with a single point light and measure the pixel-to-pixel luminance ratio across the terminator. Assert it does not exceed a threshold (e.g., 3:1 in display space).
2. **Lighting parameter validation**: When `ambient_factor < 0.02` and only point lights exist at distance > 10 units, emit a warning about potential insufficient illumination.
3. **Finale scene audit**: Assert that `clear_shapes()` is called before adding finale-specific geometry.

---

## Phase 5: Recommended Fix Order

| Priority | Action | Effort | Impact | Fixes |
|----------|--------|--------|--------|-------|
| 1 | **M1**: Switch Earth Sun to DirectionalLight | 15 min | Critical -- restores Earth illumination | RC-D, RC-F |
| 2 | **M2**: Clear scene shapes before finale | 10 min | High -- removes visual clutter | RC-E |
| 3 | **M3**: Raise finale ambient to 3% | 2 min | High -- restores dark-side visibility | RC-F |
| 4 | **M4**: Raise main animation ambient to 15% | 2 min | Medium -- softens terminator on rolling spheres | RC-C |
| 5 | **P1**: Add area light soft shadows | 2 hours | High -- permanent fix for hard terminator | RC-A, RC-B |
| 6 | **P2**: Hemisphere-weighted ambient | 30 min | Medium -- natural-looking fill lighting | RC-C |

---

## Files Analyzed

- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal` -- GPU shader (1119 lines)
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/core/gpu_types.h` -- GPU struct definitions
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/gpu/scene_flattener.cpp` -- CPU-to-GPU data flattener
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp` -- Animation rendering pipeline
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.h` -- Animation renderer interface
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/renderer.h` -- RenderSettings struct
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/renderer.cpp` -- CPU renderer (reference implementation)
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/metal_render_backend.mm` -- Metal backend dispatch
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/lights/point_light.h` -- PointLight class
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/lights/point_light.cpp` -- PointLight::sample()
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/materials/image_texture.h` -- ImageTexture class
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/materials/image_texture.cpp` -- Texture loading
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/yaml_scene_loader.cpp` -- YAML scene parsing
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/nwave_bowling.yaml` -- Main animation scene config
- `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/scene.h` -- Scene class
