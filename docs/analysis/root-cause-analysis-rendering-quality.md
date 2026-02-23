# Root Cause Analysis: Rendering Quality Degradation (Horizontal Stripes and Washed-Out Lighting)

**Date:** 2026-02-23
**Analyst:** Rex (Root Cause Analysis Specialist)
**Status:** Complete
**Severity:** High -- visual quality is unacceptable despite high SPP (512)

---

## 1. Problem Statement

The rendered video from the `nwave_bowling.yaml` scene exhibits two distinct visual quality defects:

1. **Horizontal stripe artifacts on the floor** -- The checkered floor (white/blue `CheckerMetal` material) displays horizontal stripe artifacts instead of clean checker squares.
2. **Blurred/washed-out lighting** -- Light appears diffuse and lacking crispness, reducing overall scene contrast and definition.

**Scope:** Metal GPU ray tracer, `nwave_bowling.yaml` scene, animation rendering path via `AnimationRenderer`.

**Environment:** macOS, Metal compute shader (`ray_trace.metal`), 1920x1080 at 512 SPP.

---

## 2. Toyota 5 Whys Analysis

### Branch A: Horizontal Stripe Artifacts on Floor

#### WHY 1 (Symptom): The checkered floor displays horizontal stripes instead of clean squares.

**Evidence:** Visual inspection of rendered frames shows alternating color bands running horizontally across the floor surface, rather than the expected regular checkerboard grid.

#### WHY 2 (Context): The checker pattern formula oscillates erratically along the floor surface.

**Evidence:** The `MAT_CHECKER_METAL` block in `ray_trace.metal` (lines 1091-1120) computes the checker pattern using a 3D formula:

```metal
int ix = int(floor(rec.point.x * checker_scale));
int iy = int(floor(rec.point.y * checker_scale));
int iz = int(floor(rec.point.z * checker_scale));
bool is_even = ((ix + iy + iz) & 1) == 0;
```

The floor box is defined in `nwave_bowling.yaml` (line 102) as:
```yaml
min: [-15.0, -0.05, -10.0], max: [10.0, 0.0, 10.0]
```

The top surface of the floor sits at y = 0.0 exactly. With `checker_scale = 1.0`, the term `floor(rec.point.y * 1.0)` evaluates `floor(y)` where y is nominally 0.0 on the floor surface.

#### WHY 3 (System): Floating-point imprecision at y = 0.0 causes `floor(y)` to oscillate between 0 and -1.

**Evidence:** Ray-surface intersection computes the hit point as `ray.origin + t * ray.direction`. Due to 32-bit floating-point arithmetic (Metal `float`), the computed y-coordinate of the hit point will not be exactly 0.0. It will be a value like 0.000001 or -0.000001 depending on the specific ray direction, pixel jitter, and accumulated floating-point error.

- When `rec.point.y` is slightly positive (e.g., +0.000001): `floor(0.000001) = 0`
- When `rec.point.y` is slightly negative (e.g., -0.000001): `floor(-0.000001) = -1`

This means `iy` flips between 0 and -1 unpredictably across the surface. Since the checker parity depends on `(ix + iy + iz) & 1`, and `iy` flips by 1, the checker color **inverts** at every position where the y-component crosses zero. This creates the observed stripe pattern: horizontal bands where all pixels in a row have the same floating-point rounding behavior due to similar ray geometry.

The stripes are specifically **horizontal** because pixels on the same scanline share similar ray elevation angles, causing their y-intersection values to round the same way. Adjacent scanlines with slightly different angles round differently, creating alternating horizontal bands.

#### WHY 4 (Design): The checker formula uses all three spatial coordinates (X, Y, Z) without considering surface orientation.

**Evidence:** The 3D checker formula `floor(x * scale) + floor(y * scale) + floor(z * scale)` is a standard approach for volumetric checkers that works well when surfaces do not align with coordinate planes. However, for a floor lying on the XZ plane at y = 0, the Y component adds no meaningful pattern information -- it only introduces instability. The identical formula appears in the CPU path (`checker_metal.cpp`, lines 15-17), confirming this is a design choice, not a porting error.

A floor checker should use only the X and Z coordinates, or the formula should offset the coordinate to move it away from the `floor()` discontinuity at integer boundaries.

#### WHY 5 (Root Cause): The MAT_CHECKER_METAL implementation lacks surface-orientation-aware coordinate selection, causing floating-point boundary artifacts on axis-aligned surfaces.

**Root Cause A:** The 3D checker pattern uses `floor()` on all three world-space coordinates without accounting for the fact that axis-aligned planar surfaces sit exactly on `floor()` discontinuities. There is no mechanism to:
- Select only the two relevant axes based on surface normal (e.g., X and Z for a horizontal floor), or
- Add a small epsilon offset to move coordinates away from integer boundaries.

This is a well-known "checker on boundary" artifact in ray tracing. The canonical solution is to compute the checker using only the two coordinates that vary across the surface (determined by the dominant normal axis), or to apply a half-unit offset to the problematic coordinate.

---

### Branch B: Blurred / Washed-Out Lighting

#### WHY 1 (Symptom): The rendered image appears blurred and washed-out, lacking contrast and crisp shadows.

**Evidence:** Visual inspection shows uniformly bright surfaces without strong directional shadows or specular highlights that would be expected from metallic surfaces.

#### WHY 2 (Context): The ambient factor is set to 0.35, which is 7x the default value.

**Evidence:** The `AnimationRenderer` sets `ambient_factor = 0.35f` in `animation_renderer.cpp` (line 258):
```cpp
frame_settings.ambient_factor = 0.35f;
```

The default `RenderSettings` in `renderer.h` (line 21) uses `ambient_factor = 0.05f`. The animation path uses 7x higher ambient illumination.

In the Metal shader (`ray_trace.metal`), the ambient term is applied at bounce 0 for all material types:
- Lambertian (line 1041): `color += throughput * camera.ambient_factor * albedo;`
- Metal (line 1077): `color += throughput * camera.ambient_factor * albedo;`
- Checker Metal (line 1111): `color += throughput * camera.ambient_factor * checker_albedo;`

With `ambient_factor = 0.35`, every surface receives 35% of its base color as flat, directionless illumination before any lighting calculations. This fundamentally reduces contrast: shadow areas that should be dark instead receive a large flat fill, and lit areas are pushed toward overexposure.

#### WHY 3 (System): The high ambient factor combines with two directional lights to produce additive overexposure.

**Evidence:** The animation renderer adds two directional lights (`animation_renderer.cpp`, lines 174-179):

```cpp
anim_scene.add_light(std::make_shared<DirectionalLight>(
    normalize(Vec3(-0.7071, -1.0, -0.7071)),
    Color3(1.0, 0.97, 0.9), 1.2));  // intensity 1.2
anim_scene.add_light(std::make_shared<DirectionalLight>(
    normalize(Vec3(-1.0, -1.0, 0.0)),
    Color3(0.9, 0.93, 1.0), 0.8));  // intensity 0.8
```

Additionally, the YAML scene defines a point light (`nwave_bowling.yaml`, lines 339-342):
```yaml
lights:
  - type: point
    position: [-4, 10, 2]
    color: [1.0, 0.97, 0.9]
    intensity: 0.7
```

However, the animation renderer does NOT clear the scene's existing lights before adding its own directional lights. The scene is constructed from the YAML first (including the point light), then the two directional lights are added on top. This means the scene has **three lights total** (1 point + 2 directional).

The total illumination budget is:
- Ambient: 0.35 * albedo (flat fill)
- Point light: up to 0.7 * albedo (with distance falloff)
- Directional light 1: up to 1.2 * albedo
- Directional light 2: up to 0.8 * albedo

For a surface facing all three lights, the total contribution can reach: `0.35 + 0.7 + 1.2 + 0.8 = 3.05` times albedo (before cosine weighting). Even with cosine falloff, a white surface (`albedo = 0.9`) receiving light from favorable angles will exceed 1.0 and be clamped, losing detail.

#### WHY 4 (Design): The direct lighting computation divides albedo by PI, partially compensating, but the ambient term does not.

**Evidence:** In `compute_direct_lighting` (line 882), the diffuse contribution is:
```metal
color += (attenuation / M_PI_F) * light_intensity * shadow_transparency * diffuse_factor;
```

The `/ M_PI_F` normalization ensures energy conservation for the diffuse BRDF. However, the ambient term at lines 1041 and 1077 applies albedo directly without any such normalization:
```metal
color += throughput * camera.ambient_factor * albedo;
```

This means the ambient contribution is disproportionately strong relative to the direct lighting. With `ambient_factor = 0.35` and `albedo = 0.9`, the ambient contribution alone is `0.315` -- already a significant portion of the [0, 1] output range, before any directional lighting is added.

#### WHY 5 (Root Cause): The ambient_factor value of 0.35 was set without calibrating against the actual light intensity and count in the scene, resulting in excessive flat-fill illumination that destroys contrast.

**Root Cause B:** The animation renderer uses an ambient factor 7x higher than the default renderer, combined with three separate light sources (2 directional + 1 point from YAML), producing overexposure. The ambient term is not energy-normalized (no PI divisor), making it disproportionately dominant. The result is a washed-out, low-contrast image where shadow regions receive too much fill light and highlight regions are clamped.

---

### Branch C: Contributing Factor -- YAML Point Light Stacking

#### WHY 1-3: The YAML-defined point light is not removed when animation-specific directional lights are added.

**Evidence:** `animation_renderer.cpp` line 174 calls `anim_scene.add_light()` but never calls `anim_scene.clear_lights()` for the main animation phase. The `clear_lights()` call only appears in the finale phase (line 477). The point light from the YAML file therefore persists throughout the main animation, stacking on top of the two programmatic directional lights.

This is a contributing factor to Branch B. The scene was designed with a single point light at intensity 0.7, but the animation adds two more directional lights at intensity 1.2 and 0.8 on top of it, creating unintended triple-light illumination.

**Root Cause C:** Missing `anim_scene.clear_lights()` before adding animation-specific directional lights causes the YAML point light to stack, contributing to overexposure.

---

## 3. Backwards Chain Validation

### Root Cause A Validation (Checker Stripe)
- If the checker formula includes `floor(point.y * scale)` and the surface is at y = 0.0 --> THEN floating-point imprecision causes y to be sometimes +epsilon, sometimes -epsilon --> THEN `floor(y)` oscillates between 0 and -1 --> THEN checker parity flips unpredictably --> THEN horizontal stripes appear (because pixels on the same scanline share similar ray angles).
- **VALIDATED:** The causal chain fully explains horizontal (not vertical or random) stripe artifacts on a horizontal surface at y = 0.

### Root Cause B Validation (Washed-Out Lighting)
- If ambient_factor = 0.35 without PI normalization --> THEN every surface gets 35% fill before lighting --> THEN shadows are never dark --> THEN contrast is reduced.
- If two directional lights (1.2 + 0.8) plus a point light (0.7) all contribute --> THEN well-lit surfaces can exceed 1.0 --> THEN gamma-corrected output clips to white --> THEN highlights lose detail.
- Combined effect: low contrast + clipped highlights = "blurred/washed-out" appearance.
- **VALIDATED:** The causal chain produces the observed visual symptoms.

### Root Cause C Validation (Light Stacking)
- If animation renderer adds lights without clearing existing ones --> THEN YAML point light persists alongside 2 directional lights --> THEN total light intensity is higher than intended --> THEN contributes to overexposure.
- **VALIDATED:** This is a contributing factor to Root Cause B.

---

## 4. Completeness Check

| Symptom | Root Cause | Explanation Complete? |
|---------|-----------|----------------------|
| Horizontal stripes on floor | A: 3D checker formula on boundary | Yes |
| Blurred/washed-out lighting | B: Ambient factor 7x too high, no PI normalization | Yes |
| -- (contributing) | C: Uncleared YAML point light stacks with animation lights | Yes |

All observed symptoms are fully explained by the identified root causes. No additional branches required.

---

## 5. Solution Recommendations

### Immediate Mitigations (Restore Acceptable Quality)

| ID | Action | Addresses | Priority |
|----|--------|-----------|----------|
| M1 | Reduce `ambient_factor` from 0.35 to 0.08-0.10 in `animation_renderer.cpp` line 258 | Root Cause B | HIGH |
| M2 | Add `anim_scene.clear_lights()` before adding directional lights at line 174 of `animation_renderer.cpp` | Root Cause C | HIGH |

### Permanent Fixes (Prevent Recurrence)

| ID | Action | Addresses | Priority |
|----|--------|-----------|----------|
| P1 | Modify the `MAT_CHECKER_METAL` block in `ray_trace.metal` to use a 2D checker formula based on surface normal: select X+Z for horizontal surfaces (abs(normal.y) dominant), X+Y for vertical Z-facing surfaces, Y+Z for vertical X-facing surfaces. This eliminates the `floor()` discontinuity on the surface plane. | Root Cause A | CRITICAL |
| P2 | Alternatively for P1: add a half-unit offset to the problematic coordinate: `floor((point.y + 0.5) * scale)`. This shifts the `floor()` boundary away from y=0.0. Simpler than P1 but less general. | Root Cause A | CRITICAL (alternative) |
| P3 | Apply the same 2D checker fix to the CPU path in `checker_metal.cpp` (lines 15-17) to keep both paths consistent. | Root Cause A | MEDIUM |
| P4 | Normalize the ambient term by PI to match the direct lighting energy budget, or calibrate the ambient factor relative to total scene light intensity. | Root Cause B | MEDIUM |

### Early Detection Measures

| ID | Action | Addresses | Priority |
|----|--------|-----------|----------|
| D1 | Add a visual regression test that renders a single frame of the bowling scene at low resolution and compares against a reference image, detecting stripe artifacts or gross brightness changes. | All | LOW |
| D2 | Add a shader unit test that evaluates the checker formula at y=0.0 +/- epsilon and asserts stable output. | Root Cause A | LOW |

---

## 6. Detailed Evidence Appendix

### A1: Checker Formula in Metal Shader

**File:** `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`
**Lines:** 1091-1120

```metal
if (mat_type == MAT_CHECKER_METAL) {
    constant GPUMaterial& cmat = *(constant GPUMaterial*)(materials + rec.material_index * 48);
    float checker_scale = cmat.texture_scale;
    if (checker_scale <= 0.0f) checker_scale = 1.0f;
    int ix = int(floor(rec.point.x * checker_scale));
    int iy = int(floor(rec.point.y * checker_scale));  // <-- PROBLEMATIC for y=0 surfaces
    int iz = int(floor(rec.point.z * checker_scale));
    bool is_even = ((ix + iy + iz) & 1) == 0;
    float3 checker_albedo = is_even ? albedo : tint;
    // ...
}
```

### A2: Floor Geometry Definition

**File:** `/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/nwave_bowling.yaml`
**Line:** 102

```yaml
- { name: marble_floor, type: box, min: [-15.0, -0.05, -10.0], max: [10.0, 0.0, 10.0], material: floor_marble, physics: { body_type: static } }
```

Top surface at y = 0.0 exactly.

### A3: Ambient Factor Setting

**File:** `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`
**Line:** 258

```cpp
frame_settings.ambient_factor = 0.35f;
```

Default in `renderer.h` line 21: `float ambient_factor = 0.05f;`

### A4: Directional Lights Added Without Clearing

**File:** `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`
**Lines:** 174-179

```cpp
anim_scene.add_light(std::make_shared<DirectionalLight>(
    normalize(Vec3(-0.7071, -1.0, -0.7071)),
    Color3(1.0, 0.97, 0.9), 1.2));
anim_scene.add_light(std::make_shared<DirectionalLight>(
    normalize(Vec3(-1.0, -1.0, 0.0)),
    Color3(0.9, 0.93, 1.0), 0.8));
```

No preceding `anim_scene.clear_lights()` -- the YAML point light persists.

### A5: YAML Point Light

**File:** `/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/nwave_bowling.yaml`
**Lines:** 339-342

```yaml
lights:
  - type: point
    position: [-4, 10, 2]
    color: [1.0, 0.97, 0.9]
    intensity: 0.7
```

### A6: CPU Checker Formula (Identical Issue)

**File:** `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/materials/checker_metal.cpp`
**Lines:** 15-17

```cpp
int ix = static_cast<int>(std::floor(rec.point.x() * scale_));
int iy = static_cast<int>(std::floor(rec.point.y() * scale_));
int iz = static_cast<int>(std::floor(rec.point.z() * scale_));
```

---

## 7. Summary

Three root causes were identified:

| # | Root Cause | Impact | Fix Complexity |
|---|-----------|--------|----------------|
| A | 3D checker formula includes Y-axis `floor()` at y=0 surface boundary, causing parity oscillation | Horizontal stripes on floor | Low (use 2D formula or offset) |
| B | Ambient factor 0.35 (7x default), unnormalized, combined with high-intensity lights | Washed-out, low-contrast rendering | Low (reduce to ~0.08) |
| C | Animation renderer does not clear YAML lights before adding its own, causing 3 light sources | Contributes to overexposure | Low (add clear_lights call) |

All three are independently verifiable, non-contradictory, and collectively explain both observed symptoms. The fixes are straightforward and low-risk.
