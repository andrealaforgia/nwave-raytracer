# Directional Lighting and Illumination Models in Ray Tracing

## Research Metadata
- **Date**: 2026-02-22
- **Researcher**: Nova (nw-researcher)
- **Topic**: Directional lighting, illumination models, and diagnosis of harsh geometric light/dark patterns on spheres
- **Depth**: Detailed
- **Source Count**: 10
- **Confidence**: High (core illumination theory), High (codebase analysis), Medium (double-counting diagnosis)

---

## Executive Summary

The nwave-raytracer's implementation of directional lighting in `ray_trace.metal` is largely correct in its core Lambertian shading formula. However, analysis of the shader reveals **three specific issues** that can produce the harsh geometric light/dark patterns observed on the Earth sphere:

1. **Ambient light accumulates at every bounce** (lines 1035, 1070), multiplied by throughput, causing progressively washed-out indirect illumination that creates uneven brightness patterns.
2. **Direct lighting is computed at every Lambertian bounce** (lines 1038-1040), which combines with the scattered bounce to double-count light energy -- effectively a form of next-event estimation (NEE) without the corresponding MIS weight to prevent scattered rays from also collecting that same light.
3. **The shadow ray origin bias direction may interact poorly with the face-corrected normal** for back-face hits, although for a convex sphere lit from the front this is less likely to be the primary cause.

The most probable cause of the harsh geometric patterns is **issue #2**: the combination of direct lighting (explicit shadow rays) at every bounce with unconstrained indirect bounces that can also hit the light background, leading to accumulated energy inconsistencies that manifest as bright/dark geometric facets on the sphere surface.

---

## Table of Contents

1. [How Directional Lights Should Work in a Ray Tracer](#1-how-directional-lights-should-work)
2. [Illumination Models for Smooth Sphere Shading](#2-illumination-models-for-smooth-sphere-shading)
3. [Causes of Harsh Geometric Light/Dark Patterns](#3-causes-of-harsh-geometric-lightdark-patterns)
4. [Production Implementation of Direct Lighting](#4-production-implementation-of-direct-lighting)
5. [Shadow Rays for Directional Lights](#5-shadow-rays-for-directional-lights)
6. [Codebase Analysis: Identified Discrepancies](#6-codebase-analysis-identified-discrepancies)
7. [Recommended Fixes](#7-recommended-fixes)
8. [Source Analysis](#8-source-analysis)
9. [Knowledge Gaps](#9-knowledge-gaps)

---

## 1. How Directional Lights Should Work

**Confidence: High** (5 independent sources)

A directional light models an infinitely distant light source (e.g., the Sun) where all light rays travel in parallel along a single direction vector. The key properties are:

- **No distance attenuation**: Since the source is infinitely far away, the inverse-square falloff is constant (effectively 1.0) for all points in the scene.
- **Uniform illumination direction**: Every surface point sees the light arriving from the same direction.
- **Shadow ray direction**: The shadow ray is cast from the hit point **toward the light** (opposite to the light's travel direction). For a directional light, this ray extends to infinity (t_max = very large).

### Direction Convention

The standard convention stores the directional light's travel direction (the direction light photons move), then **negates** it when computing the direction-to-light vector used for shading:

```
// Light stores: direction light travels (away from source, toward scene)
// Shading uses: direction from surface toward light = -light_travel_direction
Vec3 L = -light.direction;  // L points toward light source
```

This is confirmed across multiple authoritative sources:
- Scratchapixel's shading tutorial uses `Vec3f L = -light->dir` to convert stored direction to toward-light.
- The Rust raytracer implementation uses `direction_to_light = -scene.light.direction`.
- The DXR tutorial computes direction from hit point to light position.

### nwave-raytracer Implementation (CORRECT)

The nwave-raytracer handles this correctly through two negation steps:

1. **DirectionalLight::sample()** (directional_light.cpp, line 11): `ls.direction = -direction_` -- negates the stored travel direction to produce "toward light".
2. **SceneFlattener** (scene_flattener.cpp, lines 290-292): Stores `-dir->direction()` in the GPU position field -- which is `-(-travel_dir)` = the toward-light direction.
3. **Metal shader** (ray_trace.metal, line 831): `light_dir = normalize(light.position)` -- uses the stored toward-light direction directly.

**Verdict**: The direction convention is correct. The GPU receives the toward-light direction and uses it properly.

---

## 2. Illumination Models for Smooth Sphere Shading

**Confidence: High** (6 independent sources)

### Lambertian (Diffuse) Shading

The fundamental illumination model for matte surfaces. The reflected intensity depends only on the angle between the surface normal and the light direction:

```
I_diffuse = (albedo / pi) * L_i * max(0, dot(N, L))
```

Where:
- `albedo` = surface color/reflectance (0 to 1)
- `L_i` = incident light intensity * color
- `N` = unit surface normal at hit point
- `L` = unit direction from hit point toward light
- `1/pi` = energy conservation factor (ensures hemisphere integral does not exceed incident energy)

**Properties on spheres**: Lambertian shading produces a smooth gradient from full brightness (where N is parallel to L) to complete darkness (where N is perpendicular to L, the terminator line). The gradient is inherently smooth because the dot product varies continuously across a sphere's surface.

### Phong / Blinn-Phong (Specular + Diffuse)

Extends Lambertian with a view-dependent specular highlight:

```
I_blinn_phong = I_diffuse + k_s * L_i * max(0, dot(N, H))^n
```

Where `H` is the half-vector between the view direction and light direction, `k_s` is the specular coefficient, and `n` is the shininess exponent.

Both Phong and Blinn-Phong produce smooth shading on spheres because the dot products vary continuously with the surface normal.

### Key Insight: Any Standard Model Produces Smooth Gradients on Spheres

Since a sphere has analytically continuous normals (the normal at any point is simply `(hit_point - center) / radius`), **any** standard illumination model -- Lambertian, Phong, Blinn-Phong, or physically-based -- will produce smooth shading gradients. Harsh geometric patterns on a sphere are therefore **always** a bug, not a limitation of the illumination model.

---

## 3. Causes of Harsh Geometric Light/Dark Patterns

**Confidence: High** (4 independent sources, plus direct code analysis)

Harsh geometric (non-smooth) light/dark patterns on a sphere can be caused by:

### 3.1 Shadow Acne / Self-Intersection

**Description**: Floating-point imprecision causes shadow rays to intersect the surface they originate from, producing dark speckles or patches.

**Pattern**: Random dark dots/noise, not large geometric regions.

**nwave status**: The shader uses a bias of `T_MIN = 0.001` and offsets shadow ray origin along the normal (line 838: `shadow_origin = rec.point + T_MIN * rec.normal`). This is the standard fix. **Unlikely cause of large geometric patterns.**

### 3.2 Incorrect Normal Computation

**Description**: If the surface normal is computed incorrectly (e.g., not normalized, using wrong coordinate space, or flipped), the dot(N, L) term produces discontinuous or incorrect values.

**Pattern**: Sharp brightness boundaries that follow coordinate-space axes or geometric features.

**nwave status**: The sphere normal is `(hit_point - center) / radius` (line 406), which is the correct outward unit normal. The front-face logic and normal flipping (lines 680-682) are standard. **Normals appear correct.**

### 3.3 Energy Double-Counting (Direct + Indirect)

**Description**: When a renderer computes explicit direct lighting (shadow rays to lights) at each hit point AND also allows scattered rays to continue bouncing (which may themselves encounter the light or bright background), the same light energy gets counted multiple times. This creates brightness inconsistencies that vary with surface geometry and viewing angle.

**Pattern**: Regions of unexpected brightness or darkness that follow the geometry of how bounce rays interact with the scene. Can appear as harsh geometric boundaries.

**nwave status**: **THIS IS PRESENT IN THE CODE.** See Section 6.1 for detailed analysis.

### 3.4 Ambient Light Per-Bounce Accumulation

**Description**: If an ambient term is added at every bounce (multiplied by the current throughput), surfaces that scatter many bounces accumulate more ambient light than those that scatter few bounces. This is physically incorrect -- ambient light is a global approximation that should be applied once.

**Pattern**: Some surface regions appear brighter or darker depending on how many bounces occur, which varies with the random scatter direction and surface geometry.

**nwave status**: **THIS IS PRESENT IN THE CODE.** See Section 6.2 for detailed analysis.

### 3.5 Normal Used in Shadow Bias vs. Light Dot Product Mismatch

**Description**: If the normal used for shadow ray bias points in a different direction than expected, the shadow ray might start on the wrong side of the surface, causing incorrect shadow results.

**nwave status**: The shader uses `rec.normal` (the face-corrected normal, always facing the ray) for both shadow bias and lighting calculations. For front-face hits on a convex sphere, this is correct. **Unlikely primary cause.**

---

## 4. Production Implementation of Direct Lighting

**Confidence: High** (5 independent sources)

Production ray tracers implement direct lighting using one of two approaches:

### 4.1 Pure Path Tracing (Ray Tracing in One Weekend style)

No explicit light sampling. Rays bounce randomly and accumulate color only when they hit an emissive surface or the background. Simple but noisy:

```
color = throughput * background_color;  // only on miss (no hit)
// No explicit shadow rays, no ambient term
```

### 4.2 Next Event Estimation (NEE) with MIS

At each hit point, explicitly sample the light (cast shadow ray) AND scatter a bounce ray. To avoid double-counting, use Multiple Importance Sampling (MIS) weights, or exclude direct light contribution from scattered rays:

```
// At each bounce:
direct = shadow_ray_to_light() * BRDF * dot(N, L);  // explicit light sampling
color += throughput * direct;

// Scatter ray for indirect lighting
// IMPORTANT: if the scattered ray hits a light source, do NOT add its
// emission (or weight it with MIS), since we already counted it via NEE
```

The critical principle: **if you explicitly sample lights, you must not also count them when scattered rays happen to hit them.** This is confirmed by the NVIDIA NEE article and multiple academic sources on MIS.

### nwave-raytracer's Hybrid Approach

The nwave shader uses a hybrid that is neither pure path tracing nor proper NEE:

- It computes **explicit direct lighting** (shadow rays) at every Lambertian bounce (lines 1038-1040)
- It ALSO adds an **ambient term** at every bounce (line 1035)
- It ALSO **scatters a bounce ray** that continues to the next iteration, which may hit the background (sky gradient) or other lit surfaces
- There is **no MIS weighting** and **no exclusion of direct light from scattered rays**

This creates a double-counting scenario where light energy is accumulated both via explicit shadow rays and via scattered rays hitting the lit background.

---

## 5. Shadow Rays for Directional Lights

**Confidence: High** (4 independent sources)

The correct shadow ray computation for directional lights:

```
shadow_ray.origin = hit_point + bias * surface_normal;
shadow_ray.direction = light_direction_toward_light;  // normalized
shadow_ray.t_max = infinity;  // directional light is infinitely far

if (no_intersection_found(shadow_ray)):
    diffuse = albedo * light_intensity * max(0, dot(N, L))
```

### nwave-raytracer Implementation (CORRECT)

Lines 829-833 of `ray_trace.metal`:
```metal
// Directional light: position stores direction toward light
light_dir = normalize(light.position);
light_dist = T_MAX;
light_intensity = light.color * light.intensity;
```

Line 838: `shadow_origin = rec.point + T_MIN * rec.normal;`
Line 880: `diffuse_factor = max(0.0f, dot(rec.normal, light_dir));`
Line 881: `color += attenuation * light_intensity * shadow_transparency * diffuse_factor;`

The shadow ray computation is correct:
- Direction is toward the light (normalized)
- Distance is T_MAX (effectively infinity)
- Origin is biased along the surface normal
- Diffuse factor uses clamped dot product
- Shadow transparency handles glass objects

**One missing element**: The 1/pi energy conservation factor from the Lambertian BRDF. The formula should be `albedo / pi * light_intensity * dot(N, L)`, but the code uses `albedo * light_intensity * dot(N, L)`. This makes surfaces appear approximately 3.14x brighter than physically correct under direct lighting, though this is a common simplification in non-PBR renderers and does not cause geometric patterns.

---

## 6. Codebase Analysis: Identified Discrepancies

### 6.1 CRITICAL: Direct Lighting + Indirect Bounce Double-Counting

**Location**: `ray_trace.metal`, lines 1033-1053 (Lambertian material block)

```metal
// --- Lambertian material ---
if (mat_type == MAT_LAMBERTIAN) {
    // Ambient term
    color += throughput * camera.ambient_factor * albedo;          // LINE 1035

    // Direct lighting
    color += throughput * compute_direct_lighting(                  // LINE 1038
        rec, albedo, lights, light_count, shapes, shape_count,
        materials, material_count, bvh_nodes, bvh_count);

    // Scatter: random direction on hemisphere
    float3 scatter_dir = rec.normal + random_in_unit_sphere(rng_seed);
    // ...
    throughput *= albedo;
    current_ray.origin = rec.point + T_MIN * rec.normal;
    current_ray.direction = scatter_dir;
    continue;  // <-- continues to next bounce, which may hit background
}
```

**The problem**: At each Lambertian hit, the shader:
1. Adds ambient light (scaled by throughput and albedo)
2. Adds explicit direct lighting from all lights (shadow rays, Lambertian dot product)
3. Scatters a bounce ray in a random hemisphere direction

The bounce ray (step 3) continues and on the **next** iteration either:
- Hits another surface (where direct lighting is again computed explicitly)
- Misses all surfaces and adds the **sky gradient background** (line 953)

The sky gradient serves as environmental illumination. By adding explicit direct lighting AND allowing bounced rays to accumulate sky light, the renderer double-counts illumination. The effect is geometry-dependent because the scatter direction determines which subsequent surfaces or sky regions are sampled, creating **view-dependent brightness variations** that follow the statistics of the random scatter -- which, at low sample counts, appear as harsh geometric patterns.

**Severity**: HIGH. This is the most likely cause of the observed artifacts at low sample counts. At high sample counts, the random scattering averages out, but at low SPP the variance manifests as bright/dark patches.

### 6.2 MODERATE: Ambient Light Accumulated Per-Bounce

**Location**: `ray_trace.metal`, line 1035

```metal
color += throughput * camera.ambient_factor * albedo;
```

This line executes at **every** Lambertian bounce. With `ambient_factor = 0.03` and multiple bounces:

- Bounce 0: adds `1.0 * 0.03 * albedo`
- Bounce 1: adds `albedo * 0.03 * albedo` = `0.03 * albedo^2`
- Bounce 2: adds `albedo^2 * 0.03 * albedo` = `0.03 * albedo^3`
- Total ambient: `0.03 * albedo * (1 + albedo + albedo^2 + ...)` = `0.03 * albedo / (1 - albedo)`

For an Earth texture with average albedo ~0.3, total ambient ~= `0.03 * 0.3 / 0.7` = 0.013. For bright regions (albedo ~0.8): `0.03 * 0.8 / 0.2` = 0.12. This means **bright regions accumulate 9x more ambient light than dark regions**, creating a non-linear brightness amplification that follows the texture pattern but with exaggerated contrast.

**Severity**: MODERATE. The effect is subtle for the Earth scene (ambient_factor is only 0.03), but it contributes to uneven illumination, especially in areas of high albedo contrast.

### 6.3 MINOR: Missing 1/pi Energy Conservation Factor

**Location**: `ray_trace.metal`, line 881

```metal
color += attenuation * light_intensity * shadow_transparency * diffuse_factor;
```

The correct Lambertian BRDF includes a `1/pi` normalization:

```metal
// Physically correct:
color += (attenuation / M_PI_F) * light_intensity * shadow_transparency * diffuse_factor;
```

Without this factor, surfaces are ~3.14x brighter under direct lighting than they should be. This does not cause geometric patterns but it does make the direct lighting contribution overly dominant compared to the indirect (bounced) contribution, which could exacerbate the visual impact of the double-counting issue.

**Severity**: MINOR for the pattern issue specifically. However, fixing this would bring the renderer closer to physical correctness and improve the balance between direct and indirect illumination.

### 6.4 OBSERVATION: Light Direction Construction

**Location**: `animation_renderer.cpp`, lines 484-486

```cpp
anim_scene.add_light(std::make_shared<DirectionalLight>(
    Vec3(-std::sin(cam_orbit_angle), -0.3, -std::cos(cam_orbit_angle)),
    Color3(1.0, 0.95, 0.85), 2.5));
```

The direction vector `(-sin(angle), -0.3, -cos(angle))` represents the light's travel direction (from source toward scene). This is then negated twice (once in DirectionalLight::sample, once in SceneFlattener) to arrive at the toward-light direction on the GPU. The pipeline is correct.

The `-0.3` Y component means the light comes slightly from above, which is appropriate for sunlight. The XZ components track the camera orbit angle so the lit hemisphere always faces the camera. This is correct.

---

## 7. Recommended Fixes

### Fix 1: Move Ambient to Primary Hit Only (Quick Fix)

Add ambient light only at the first bounce (bounce == 0), not at every Lambertian hit:

```metal
// In the Lambertian block:
if (bounce == 0) {
    color += throughput * camera.ambient_factor * albedo;
}
```

Or pass the bounce index and only add ambient on the first Lambertian hit.

### Fix 2: Proper NEE with Light Exclusion (Correct Fix)

If the renderer uses explicit direct lighting (shadow rays), it should not also add the sky background when a scattered ray misses all geometry. The approach:

**Option A -- No sky on indirect rays**: Only add the sky gradient on the primary ray miss (bounce 0). Subsequent misses contribute nothing (they are accounted for by the explicit direct lighting):

```metal
if (!scene_hit) {
    if (bounce == 0) {
        // Primary ray: show sky
        float3 unit_dir = normalize(current_ray.direction);
        float a = 0.5f * (unit_dir.y + 1.0f);
        color += throughput * ((1.0f - a) * camera.background_bottom + a * camera.background_top);
    }
    // else: indirect ray miss -- no contribution (NEE handles lighting)
    break;
}
```

**Option B -- Remove explicit direct lighting, use pure path tracing**: Remove `compute_direct_lighting()` calls entirely and rely solely on bounced rays hitting emissive surfaces or the background. This is simpler but noisier.

**Option C -- Full MIS (production-quality)**: Keep both NEE and scattered rays, but weight them with MIS to avoid double-counting. This is the most correct approach but also the most complex to implement.

### Fix 3: Add 1/pi Factor (Correctness)

```metal
// In compute_direct_lighting, line 881:
color += (attenuation / M_PI_F) * light_intensity * shadow_transparency * diffuse_factor;
```

This reduces the direct lighting contribution to physically correct levels and improves the balance between direct and indirect illumination.

### Recommended Implementation Order

1. **Fix 1** (ambient per-bounce) -- simplest, immediate improvement
2. **Fix 2, Option A** (no sky on indirect rays) -- eliminates the double-counting, most impactful for the observed artifacts
3. **Fix 3** (1/pi factor) -- correctness improvement, apply alongside Fix 2

---

## 8. Source Analysis

| # | Source | Type | Reputation | Used For |
|---|--------|------|------------|----------|
| 1 | [Scratchapixel: Diffuse and Lambertian Shading](https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/diffuse-lambertian-shading.html) | Tutorial | High (established CG education) | Lambertian formula, 1/pi factor, light direction convention |
| 2 | [Scratchapixel: Light and Shadows](https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/ligth-and-shadows.html) | Tutorial | High | Shadow ray implementation, bias technique |
| 3 | [Writing a Raytracer in Rust - Part 2](https://bheisler.github.io/post/writing-raytracer-in-rust-part-2/) | Tutorial/Blog | Medium | Directional light implementation, shadow acne, direction convention |
| 4 | [NVIDIA DXR Tutorial: Simple Lighting](https://developer.nvidia.com/rtx/raytracing/dxr/dx12-raytracing-tutorial/extra/dxr_tutorial_extra2_simple_lighting) | Official Tutorial | High (NVIDIA) | DXR directional light, N dot L formula |
| 5 | [NVIDIA: NEE for Ray Tracing](https://developer.nvidia.com/blog/conquering-noisy-images-in-ray-tracing-with-next-event-estimation/) | Technical Blog | High (NVIDIA) | Next event estimation, importance sampling |
| 6 | [Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html) | Book/Tutorial | High (canonical reference) | Path tracing bounce model, throughput accumulation |
| 7 | [NVIDIA: Solving Self-Intersection Artifacts in DXR](https://developer.nvidia.com/blog/solving-self-intersection-artifacts-in-directx-raytracing/) | Technical Blog | High (NVIDIA) | Shadow acne, self-intersection, bias techniques |
| 8 | [Cornell CS4620: Ray Tracing Shading](https://www.cs.cornell.edu/courses/cs4620/2012fa/lectures/35raytracing.pdf) | Academic Lecture | High (Cornell) | Illumination models, Lambertian/Phong on spheres |
| 9 | [Wikipedia: Blinn-Phong reflection model](https://en.wikipedia.org/wiki/Blinn%E2%80%93Phong_reflection_model) | Encyclopedia | Medium | Model comparison, formula verification |
| 10 | [Stanford CS148: Raytracing Assignment](https://graphics.stanford.edu/courses/cs148-10-summer/as3/instructions/as3.pdf) | Academic | High (Stanford) | Direct lighting, shadow rays, illumination models |

---

## 9. Knowledge Gaps

### 9.1 Exact Visual Pattern Diagnosis

Without seeing the actual rendered output image, the diagnosis relies on code analysis and theoretical understanding. The identified issues (double-counting, per-bounce ambient) are the most likely causes based on the code structure, but a visual comparison before/after each fix would confirm which issue is primary.

**Searched**: Could not find authoritative sources showing the exact visual signature of NEE double-counting artifacts. The diagnosis is based on first-principles analysis of the shader code.

### 9.2 MIS Implementation for Metal Shaders

Full MIS (Multiple Importance Sampling) implementation details for Metal compute shaders were not found. The general algorithm is well-documented, but Metal-specific implementation patterns (thread synchronization, performance implications) are not covered in this research.

**Searched**: "Metal compute shader MIS multiple importance sampling ray tracing" -- no relevant results.

### 9.3 Interaction with Low SPP and Batching

The nwave-raytracer uses a batching system (lines 1130-1143) where samples are accumulated across multiple kernel invocations. At low SPP per batch, the variance from double-counting could be amplified by the batch averaging. This interaction was not researched in depth.

### 9.4 Quantitative Impact of 1/pi Factor

While the theory is clear (3.14x brightness reduction), the perceptual impact on the specific Earth scene (with its dark sky background and 2.5 intensity directional light) was not quantified. The gamma correction (sqrt, line 1142) also interacts with the brightness levels in a non-linear way.

---

## Appendix: Critical Code Paths Summary

### Light Direction Pipeline

```
animation_renderer.cpp:485  Vec3(-sin(angle), -0.3, -cos(angle))  // light travel direction
         |
         v
directional_light.cpp:7     direction_ = normalize(input)          // stored normalized
directional_light.cpp:11    ls.direction = -direction_              // CPU: toward light
         |
         v
scene_flattener.cpp:290     position = -dir->direction()            // GPU: -(-travel) = toward light
         |
         v
ray_trace.metal:831         light_dir = normalize(light.position)   // GPU: toward light (CORRECT)
```

### Lighting Accumulation per Lambertian Bounce

```
ray_trace.metal:1035  color += throughput * ambient * albedo    // ambient (EVERY BOUNCE)
ray_trace.metal:1038  color += throughput * direct_lighting     // NEE shadow rays (EVERY BOUNCE)
ray_trace.metal:1050  throughput *= albedo                      // attenuate for next bounce
ray_trace.metal:1051  scatter ray continues...                  // may hit sky (DOUBLE-COUNT)
```
