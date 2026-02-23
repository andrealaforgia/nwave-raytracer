# Root Cause Analysis: Dark/Black Lines at Frame 520

**Date**: 2026-02-23
**Analyst**: Rex (Root Cause Analysis Specialist)
**Status**: Complete
**Severity**: Medium (visual artifact, not a crash or data loss)

---

## Problem Statement

At frame 520 of the nwave bowling animation, dark/black lines are visible in the rendered image of the Earth sphere during the finale phase.

## Scope

- **Affected frame**: 520 (finale_idx = 130, well into the Earth rotation sequence)
- **Scene state**: Bowling objects cleared; only Earth and Moon spheres remain
- **Rendering config**: Single directional light from camera direction, ambient_factor = 0.0, background near-black
- **Texture**: `/Users/andrealaforgia/Desktop/flat_earth03.jpg` (2048x1024 equirectangular JPEG)

---

## Toyota 5 Whys Analysis

### Branch A: Moon Shadow on Earth Surface

**WHY 1 (Symptom)**: Dark lines/arcs are visible on the rendered Earth sphere.

- Evidence: User report of dark/black lines at frame 520.

**WHY 2 (Context)**: The Moon casts a hard shadow onto the Earth surface via directional light shadow rays.

- Evidence: At frame 520, the Moon is at position (-0.651, 2.0, 3.693) in world space. The directional light travels in direction (0.414, -0.336, -0.846). The Moon lies roughly in the light direction from the Earth (dot product of earth-to-moon direction with light direction = 0.905). The Moon shadow cylinder (radius 0.375) passes within 1.596 units of the Earth center, and the cylinder edge (1.596 - 0.375 = 1.221) penetrates the Earth sphere (radius 1.5). The shadow creates a crescent-shaped dark region near the limb of the visible Earth disk, approximately 181 pixels of arc and up to 152 pixels wide.
- Code reference: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, lines 837-887 (`compute_direct_lighting`). Shadow rays from Earth surface points that intersect the Moon (Lambertian/opaque material) are fully blocked, setting `fully_blocked = true` and producing zero diffuse contribution.

**WHY 3 (System)**: The shadow is hard-edged because the scene uses a single directional light with no soft shadow implementation.

- Evidence: The `compute_direct_lighting` function casts a single shadow ray per light. Directional lights produce parallel shadow rays. An opaque occluder (Moon) produces a binary shadow (fully lit or fully blocked) with no penumbra. This creates a sharp dark boundary that appears as a distinct line/arc on the Earth surface.
- Code reference: `ray_trace.metal` line 876: `fully_blocked = true; break;` -- no partial occlusion or soft shadow calculation.

**WHY 4 (Design)**: The lighting setup places the light source in the same direction as the Moon's orbit, creating an alignment where the Moon shadow falls on the visible Earth surface.

- Evidence: `animation_renderer.cpp` line 482: `Vec3 light_dir = normalize(earth_center - cam_pos);` -- the light always comes from the camera direction. The Moon orbit at frame 520 (moon_angle = -260 degrees) places the Moon roughly in the camera/light direction relative to the Earth (dot product 0.905 with light direction). The Moon orbit radius (3.75) is small enough that the Moon's shadow cylinder clips the Earth at this alignment.
- Code reference: `animation_renderer.cpp` lines 414-419 (Moon orbit computation), line 482 (light direction).

**WHY 5 (Root Cause)**: The finale animation design does not account for the geometric relationship between the Moon orbit, the camera-following directional light, and shadow casting. The Moon periodically enters the light path, creating transient shadow artifacts on the Earth surface.

- Evidence: The Moon completes 2 full orbits during the 360-frame finale (`moon_angle = -finale_idx * (2 * 2 * PI / finale_frames)`). The directional light follows the camera, which orbits slowly. At certain frame ranges (including frame 520), the Moon's orbital position aligns with the light direction, causing it to cast shadows on the Earth. This shadow sweeps across the Earth as the Moon orbits, creating transient dark arcs that appear as "dark lines" in individual frames.

### Branch B: Zero Ambient Amplifies Shadow Visibility

**WHY 1 (Symptom)**: The dark lines appear stark and highly visible against the lit Earth surface.

- Evidence: The lines are described as "dark/black" rather than "slightly dimmer."

**WHY 2 (Context)**: With `ambient_factor = 0.0`, any pixel whose shadow ray is blocked receives exactly zero light contribution.

- Evidence: `animation_renderer.cpp` line 490: `finale_settings.ambient_factor = 0.0f;`. In the shader, `color += throughput * camera.ambient_factor * albedo;` at bounce 0 (line 1041) adds 0.0 when `ambient_factor` is 0. Direct lighting blocked by Moon shadow contributes 0. Total color = 0 (pure black).
- Code reference: `ray_trace.metal` lines 1040-1041 (ambient term) and lines 1045-1047 (direct lighting).

**WHY 3 (System)**: There is no fill light, ambient occlusion, or environment lighting to soften shadows in the finale scene.

- Evidence: The scene contains exactly one directional light (cleared and re-added each frame at line 479-484). No secondary lights provide fill. `ambient_factor = 0.0` eliminates all ambient contribution.

**WHY 4 (Design)**: The finale was designed for dramatic space-like lighting (dark sky, single "sun" illumination) without considering the visual impact of hard Moon shadows.

- Evidence: Background colors are near-black (`background_top = (0.02, 0.02, 0.05)`, `background_bottom = (0.01, 0.01, 0.02)`). The design intent is a space scene, but space lighting with hard shadows produces stark artifacts when objects cast shadows on each other.

**WHY 5 (Root Cause)**: The lighting model lacks the soft shadow or penumbra computation necessary for realistic Earth-Moon rendering, and the zero-ambient design choice converts partial shadows into full black.

### Branch C: SPP Batching RNG Bug (Secondary Finding)

**WHY 1 (Symptom)**: The rendering at 512 SPP does not provide the expected noise reduction.

- Evidence: RNG seed initialization in the shader uses only the pixel index, not the batch index.

**WHY 2 (Context)**: All SPP batches produce identical random sequences, making batch accumulation redundant.

- Evidence: `ray_trace.metal` line 915: `uint rng_seed = pcg_hash(idx * 1099u + 7919u);` -- `idx` is `gid.y * camera.image_width + gid.x`, with no dependency on `camera.batch_index`. Each batch of 32 SPP generates the exact same jitter patterns, scattered ray directions, and Schlick reflectance decisions.

**WHY 3 (System)**: The batching system (`metal_buffer_manager.mm` lines 351-392) splits SPP into batches of 32 for thermal management, but the kernel invocation does not propagate the batch index into the RNG seed.

**WHY 4 (Design)**: The batching was added to prevent GPU thermal overload (commit d3ab802) without updating the RNG seeding to account for multiple kernel invocations per frame.

**WHY 5 (Root Cause)**: The RNG seed does not incorporate the batch index, making 512 SPP with 16 batches functionally equivalent to 32 SPP. This does NOT cause the dark lines but reduces rendering quality and wastes 15/16 of the GPU computation.

---

## Validation (Backwards Chain)

### Branch A (Moon Shadow):
- IF the Moon's orbit places it in the light direction relative to the Earth at frame 520 (verified: dot = 0.905)
- AND the shadow cylinder clips the Earth (verified: closest approach 1.596, minus moon radius 0.375 = 1.221 < earth radius 1.5)
- AND shadow rays that hit the Moon are fully blocked (verified: Lambertian material is opaque, code path at line 876)
- THEN dark arcs/lines appear on the Earth surface at the shadow projection (matches user report)

### Branch B (Zero Ambient):
- IF ambient_factor = 0.0 (verified from code)
- AND shadow-blocked pixels get zero direct light (verified from shader logic)
- THEN shadowed pixels render as pure black (matches "dark/black" description)

### Branch C (RNG Bug):
- IF all batches use the same RNG seed (verified from code)
- THEN all batches produce identical results
- THEN effective SPP = batch_spp (32), not total_spp (512)
- This does NOT produce dark lines but wastes computation (confirmed: orthogonal to main issue)

---

## Hypotheses Investigated and Ruled Out

| Hypothesis | Status | Reasoning |
|---|---|---|
| Texture seam (atan2 discontinuity) | Ruled out | Seam is on far side of Earth at frame 520 (dot with camera = -0.382); wrapping code handles boundary correctly |
| BVH AABB wrong for transformed shapes | Ruled out | `bvh_flattener.cpp` uses `transform_aabb` with Arvo's method; verified numerically that AABB contains the sphere |
| Shadow ray self-intersection | Ruled out | All shadow ray t-values are negative (ray goes away from sphere); double guard (origin offset + T_MIN) prevents self-intersection |
| Float32 precision in inverse_transform | Ruled out | Maximum float32 error < 1e-7; well within rendering tolerance |
| Pole epsilon boundary discontinuity | Ruled out | Color difference at v=0.02 boundary is < 8/255 (3%) for this texture; not visible as dark lines |
| Source texture dark bands | Ruled out | No significant row-to-row brightness discontinuities; largest step is 19/255 at row 1022 (far south pole, not visible) |
| acos NaN from outward_normal precision | Ruled out | Occurs only at extreme grazing angles where diffuse_factor is already near zero |
| Moon visual overlap with Earth | Ruled out | Angular separation 11.63 degrees > sum of angular radii 10.37 degrees |
| transform_normal producing NaN | Ruled out | Rigid rotation preserves vector length; normalize always produces valid result |
| Gamma correction or batch accumulation bug | Ruled out | Gamma applied only on final batch; accumulation arithmetic is correct |

---

## Solutions

### Immediate Mitigations

1. **Add minimal ambient light** to soften the Moon shadow visibility:
   - In `animation_renderer.cpp` line 490, change `ambient_factor` from `0.0f` to `0.02f`.
   - This provides a small fill light so shadowed pixels are very dark grey instead of pure black.
   - Impact: Reduces shadow contrast while preserving the space-like appearance.

2. **Exclude Moon from shadow testing** by adding a flag to skip shadow rays against specific shapes, or by checking if the shadow-casting shape is the Moon and allowing light through.

### Permanent Fixes

1. **Fix the Moon shadow alignment**: Adjust the Moon orbit radius, speed, or the light direction to avoid the Moon entering the shadow path during the visible animation. Options:
   - Increase `orbit_radius` from `earth_radius * 2.5` to `earth_radius * 4.0` so the Moon is farther from the light path.
   - Change the light direction to not exactly follow the camera (e.g., offset by 15-20 degrees).
   - Add a check: if the Moon is within N degrees of the light direction, temporarily shift the light.

2. **Fix the RNG batching bug**: Incorporate the batch index into the RNG seed to get true multi-batch sampling:
   - In `ray_trace.metal` line 915, change to:
     `uint rng_seed = pcg_hash(idx * 1099u + 7919u + camera.batch_index * 6971u);`
   - This makes each batch generate a unique random sequence, giving true 512 SPP when requested.

### Early Detection

1. **Add shadow visualization mode**: A debug render mode that highlights pixels where shadow rays are blocked, making it easy to see unintended shadows during development.
2. **Automated frame-difference check**: Flag frames where more than N% of previously-lit Earth pixels suddenly go dark, indicating a transient shadow event.

---

## Key Files

| File | Relevance |
|---|---|
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal` | Shadow ray computation (lines 805-887), sphere UV (lines 163-168), texture sampling (lines 181-238) |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp` | Finale rendering loop, Moon orbit (lines 414-419), light direction (line 482), ambient factor (line 490) |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/nwave_bowling.yaml` | Finale configuration (lines 358-364) |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/gpu/bvh_flattener.cpp` | BVH AABB computation for transformed shapes (lines 35-55, 57-145) |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/metal_buffer_manager.mm` | SPP batching dispatch (lines 351-392) |

---

## Summary

The dark/black lines at frame 520 are caused by the **Moon casting a hard shadow onto the Earth surface**. At this frame, the Moon's orbital position aligns closely with the directional light direction (dot product 0.905), placing the Moon's shadow cylinder in a position that clips the Earth's lit hemisphere. The shadow creates a crescent-shaped dark region near the limb of the visible Earth disk, appearing as a dark arc/line. The effect is amplified by zero ambient lighting, which makes shadowed pixels pure black. A secondary finding revealed that the SPP batching RNG bug makes the 512 SPP render effectively 32 SPP, which does not cause the dark lines but wastes significant GPU computation.
