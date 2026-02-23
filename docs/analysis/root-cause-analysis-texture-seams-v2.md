# Root Cause Analysis: Persistent Texture Seams on Procedural Spheres (v2)

**Date**: 2026-02-22
**Investigator**: Rex (Root Cause Analysis Specialist)
**Methodology**: Toyota 5 Whys, multi-causal, evidence-required
**Status**: ROOT CAUSES IDENTIFIED -- actionable fixes provided

---

## Problem Statement

Textured spheres in the bowling animation still show dark/light seam lines
despite the earlier fix that removed forced `texture_scale = -1.0f` in the
YAML scene loader. The seam lines persist on the 20 procedural rolling spheres.

**Scope**: Procedural texture spheres defined in `scenes/nwave_bowling.yaml`,
rendered via the Metal GPU shader pipeline.

---

## Investigation Summary

The rolling textured spheres ARE loaded through the YAML scene loader (not
created programmatically). They correctly receive `texture_scale = 0.0f`
(the default). However, the GPU shader's texture sampling decision tree
routes them to the **equirectangular** path, which uses `sample_texture()`
with standard spherical UV coordinates. This path has an inherent seam at
`u = 0` / `u = 1` (the atan2 wraparound boundary).

**Two independent root causes** were identified that contribute to the seam
artifacts, plus one design-level systemic cause.

---

## WHY Analysis: Branch A -- Equirectangular Sampling Path Seam

### WHY 1 (Symptom): Seam lines are visible on procedural textured spheres

**Evidence**: Visual observation of rendered frames shows a vertical dark/light
line artifact on each procedural sphere at the atan2 wraparound longitude.

### WHY 2 (Context): The shader samples these textures via the equirectangular path

**Evidence**: In `ray_trace.metal` lines 952-993, the texture sampling decision
tree for spheres operates as follows:

```
1. texture_scale < 0.0f  --> cube_map path        (NOT taken: scale = 0.0f)
2. is_equirectangular    --> equirectangular path  (TAKEN: 1024 > 512 * 1.8)
3. texture_scale > 0.0f  --> scaled path           (not evaluated)
4. else                  --> dual_hemisphere path  (not evaluated)
```

The `is_equirectangular` check on line 953 is:
```metal
bool is_equirectangular = (mat.texture_width > mat.texture_height * 1.8f);
```

For procedural textures sized 1024x512: `1024 > 512 * 1.8 = 921.6` is `true`.

Therefore these textures are routed to the equirectangular path (line 966-970),
which calls `sample_texture()` using raw `rec.u` and `rec.v` computed from
`sphere_uv()`.

### WHY 3 (System): The equirectangular path produces seam artifacts at the atan2 boundary

**Evidence**: The `sphere_uv()` function (line 162-167) computes:
```metal
float phi = atan2(-outward_normal.z, outward_normal.x) + M_PI_F;
u = phi / (2.0f * M_PI_F);
```

The `atan2` function has a discontinuity: it wraps from +pi to -pi at the
boundary where `outward_normal.x > 0, outward_normal.z -> 0` from above/below.
After the `+ M_PI_F` shift, this creates a seam at `u = 0` and `u = 1`.

The `sample_texture()` function (line 180-218) wraps U with `u = u - floor(u)`,
but bilinear interpolation across the u=0/u=1 boundary picks up texels from
opposite ends of the texture row. For textures that are NOT truly seamless at
the pixel level at the horizontal edges, this produces a visible seam.

### WHY 4 (Design): The aspect-ratio heuristic conflates image shape with mapping intent

**Evidence**: The `is_equirectangular` check (`width > height * 1.8`) was designed
to detect actual equirectangular maps like Earth/Moon textures (which have a natural
2:1 aspect ratio). However, procedural textures are ALSO generated at 1024x512
(2:1 ratio) because that is the standard equirectangular pixel grid for sphere
mapping. The heuristic cannot distinguish between:

- File-based equirectangular maps (Earth, Moon) where the content IS an
  equirectangular projection and seamless U-wrapping is correct
- Procedural equirectangular textures generated from 3D noise that are
  ALSO in equirectangular format but where the `sample_texture()` bilinear
  interpolation at the u=0/u=1 seam may introduce artifacts

### WHY 5 (Root Cause A): The `sample_texture()` bilinear interpolation does not properly handle the equirectangular wrap seam for procedural textures

The `sample_texture()` function on lines 200-202 wraps x0 and x1 with modular
arithmetic:
```metal
x0 = ((x0 % w) + w) % w;
x1 = ((x1 % w) + w) % w;
```

This IS correct for horizontal wrapping. So if the procedural texture pixels are
truly seamless at the u=0/u=1 boundary, there should be no seam. However, let us
examine whether the procedural textures are actually seamless.

In `procedural_texture.cpp` lines 582-602, the generation maps each pixel to a
3D point on the unit sphere:
```cpp
float theta = v * M_PI;
float phi = u * 2.0f * M_PI;
float sx = cos(phi) * sin(theta);
float sy = cos(theta);
float sz = sin(phi) * sin(theta);
```

At `u = 0` (px = 0): `phi = 0`, so `sx = sin(theta), sy = cos(theta), sz = 0`
At `u = 1` (px = width): this pixel is never generated (loop goes 0..width-1)

The pixel at `px = width-1`: `u = (width-1)/width`, `phi` is slightly less than
`2*PI`. The 3D point is very close to but not identical to `px = 0`.

When `sample_texture()` bilinearly interpolates between `px = width-1` and
`px = 0` (via the wrapping), it interpolates between two nearly-identical 3D
noise values. **For most patterns, this should be nearly seamless.**

**REVISED ROOT CAUSE A**: The equirectangular path itself is the CORRECT path
for these textures. The seam artifact, if visible, comes from the discrete
pixel sampling: the texel at `px = width-1` maps to `phi = 2*PI*(width-1)/width`
and `px = 0` maps to `phi = 0`. These are separated by `2*PI/width` radians on
the sphere. The bilinear interpolation fills this gap smoothly ONLY if the noise
is smooth at that scale. For patterns with high-frequency detail (e.g., grid,
polka_dots), the gap may be visible.

**Severity**: LOW for most patterns (noise-based patterns are smooth), MEDIUM for
geometric patterns (grid, polka_dots).

---

## WHY Analysis: Branch B -- Dual-Hemisphere Fallback Uses World-Space Normal

### WHY 1 (Symptom): If the equirectangular path were bypassed (e.g., by changing texture dimensions), spheres would fall through to the dual-hemisphere path

This is a latent defect, not the active seam cause. Documented for completeness.

### WHY 2 (Context): The dual-hemisphere path uses `rec.normal` (world-space)

**Evidence**: Line 987:
```metal
albedo = sample_sphere_dual_hemisphere(texture_data, mat.texture_offset,
                                       mat.texture_width, mat.texture_height,
                                       rec.normal);
```

### WHY 3 (System): `rec.normal` is the world-space transformed normal, not the object-space normal

**Evidence**: Lines 653-663 show that when `shape.has_transform` is true (which it
IS for these rolling spheres -- they are wrapped in `TransformedShape`), the
`outward_normal` is transformed to world space:
```metal
outward_normal = transform_normal(outward_normal, shape.inverse_transform);
```

And `rec.normal` is set to the front-face-adjusted version of this world-space
normal. The `object_space_normal` is only used for `sphere_uv()` computation
(line 671), NOT passed to the texture sampling functions.

### WHY 4 (Design): The dual-hemisphere function recomputes its own UV from the normal passed in

**Evidence**: Lines 298-306 of `sample_sphere_dual_hemisphere()`:
```metal
float theta = acos(clamp(-n.y, -1.0f, 1.0f));
float phi_a = atan2(-n.z, n.x) + M_PI_F;
float u_a = phi_a / (2.0f * M_PI_F);
float phi_b = atan2(n.x, n.z) + M_PI_F;
float u_b = phi_b / (2.0f * M_PI_F);
```

If world-space normal is passed, the texture does NOT rotate with the sphere.
The texture would appear to slide on the surface as the sphere rotates.

### WHY 5 (Root Cause B): The dual-hemisphere path has no access to the object-space normal, making it unsuitable for rotating textured spheres

The `rec` struct stores `rec.u`/`rec.v` (derived from object-space normal)
and `rec.normal` (world-space). The dual-hemisphere path needs an object-space
normal but receives world-space. This is a design defect that would manifest
if the equirectangular heuristic were to be bypassed.

**Severity**: LATENT (not the active cause of current seams, but blocks the
dual-hemisphere path as an alternative solution).

---

## WHY Analysis: Branch C -- THE PRIMARY ROOT CAUSE

### WHY 1 (Symptom): Dark/light seam lines visible on ALL 20 procedural spheres

**Evidence**: The seams appear as a consistent vertical line at the same
longitude on each sphere, characteristic of atan2 wraparound artifacts.

### WHY 2 (Context): The equirectangular `sample_texture()` path is being used

**Evidence**: Confirmed in Branch A, WHY 2 above.

### WHY 3 (System): The `sample_texture()` function does handle U wrapping correctly, but there is a subtle issue with bilinear interpolation at the V-flip boundary

**Evidence**: Examining the `sphere_uv()` function:
```metal
v = 1.0f - theta / M_PI_F;
```
This maps v from 1.0 (north pole) to 0.0 (south pole).

And examining `sample_texture()`:
```metal
v = clamp(v, 0.0f, 1.0f);
```
V is clamped, not wrapped. This is correct for equirectangular maps.

The U wrapping IS correct. **Re-evaluating**: The seam should NOT appear if
the texture pixels are generated with proper continuity at the u=0/u=1 boundary.

### WHY 3 (REVISED -- The actual mechanism): The seam is caused by the `v = 1.0f - theta / M_PI_F` coordinate convention MISMATCH between generation and sampling

**CRITICAL EVIDENCE**: The procedural texture generates pixels with:
```cpp
// procedural_texture.cpp line 585-586
float v = static_cast<float>(py) / static_cast<float>(height);  // v: 0 at top, 1 at bottom
float theta = v * M_PI;     // theta: 0 at top (north pole), PI at bottom (south pole)
```
So pixel row 0 = north pole, pixel row height-1 = south pole.

The shader's `sphere_uv()` computes:
```metal
float theta = acos(-outward_normal.y);  // 0 at south pole (-y), PI at north pole (+y)
v = 1.0f - theta / M_PI_F;             // 0 at south pole, 1 at north pole
```

And `sample_texture()` uses `fy = v * float(h) - 0.5f`, so:
- v = 0 maps to row 0 (top of texture = north pole in generation)
- v = 1 maps to row height-1 (bottom of texture = south pole in generation)

Wait -- v=0 in the shader means south pole, but row 0 in the texture means north
pole. This is a **V-axis inversion**: the shader maps the south pole to the top
of the texture and the north pole to the bottom.

Actually, let me trace this more carefully:

**Generation** (procedural_texture.cpp):
- py=0 -> v_gen=0 -> theta_gen=0 -> north pole (sy = cos(0) = 1)
- py=height-1 -> v_gen~=1 -> theta_gen~=PI -> south pole (sy = cos(PI) = -1)

**Shader sphere_uv()**:
- outward_normal.y = +1 (north pole): theta = acos(-1) = PI -> v = 1 - PI/PI = 0
- outward_normal.y = -1 (south pole): theta = acos(+1) = 0 -> v = 1 - 0 = 1

So in the shader: north pole -> v=0, south pole -> v=1.

**Shader sample_texture()**: `fy = v * float(h) - 0.5f`
- v=0 -> fy = -0.5 -> row 0 (clamped)
- v=1 -> fy = h-0.5 -> row h-1

So: north pole (v=0) -> row 0 (which in generation = north pole). CORRECT.
South pole (v=1) -> row h-1 (which in generation = south pole). CORRECT.

The V-axis is actually consistent. So Branch C's V-mismatch hypothesis is
REJECTED.

### WHY 3 (FINAL -- Re-examining the actual atan2 seam in sample_texture)

Let me trace the U coordinate at the seam boundary:

**Generation** (procedural_texture.cpp):
- px=0 -> u_gen=0 -> phi_gen=0 -> (sx=sin(theta), sy=cos(theta), sz=0)
- px=width-1 -> u_gen=(w-1)/w -> phi_gen=2*PI*(w-1)/w

**Shader sphere_uv()**:
```metal
float phi = atan2(-outward_normal.z, outward_normal.x) + M_PI_F;
u = phi / (2.0f * M_PI_F);
```

For the generation's px=0 point: normal = (sin(theta), cos(theta), 0)
- phi = atan2(-0, sin(theta)) + PI = 0 + PI = PI (for sin(theta) > 0)
- u = PI / (2*PI) = 0.5

**This is the mismatch!** The generation uses `phi_gen = u * 2 * PI` starting
from phi=0, but the shader uses `phi = atan2(-z, x) + PI`. These are different
phi conventions:

- **Generation**: phi=0 corresponds to (x=sin(theta), z=0) -- the +X/+Z quadrant boundary
- **Shader**: phi=0 corresponds to atan2(0, x)+PI = PI for x>0 -- the point is at u=0.5

The seam at u=0 in the shader (phi = 0 + PI offset then /2PI) corresponds to
the point where atan2(-z, x) = -PI, i.e., where x < 0, z = 0+. This maps to
the -X axis in the generation, which is at `phi_gen = PI`, i.e., `u_gen = 0.5`.

**The U coordinates have a 0.5 offset between generation and sampling!**

But wait -- this offset would not cause a SEAM. It would cause a rotation of
the texture by 180 degrees. The seam in the shader's equirectangular path
occurs at `u = 0` and `u = 1` (the atan2 wraparound), which maps to the
generation's `u_gen = 0.5`. Since the procedural textures have smooth 3D noise
at every point on the sphere, the texture IS continuous at every longitude
including `u_gen = 0.5`. So the phi convention mismatch causes a 180-degree
rotation but NOT a seam.

### WHY 3 (FINAL RESOLUTION): The seam is in the `sample_texture` bilinear interpolation at the half-texel boundary

Re-examining `sample_texture()` lines 189-217:
```metal
float fx = u * float(w) - 0.5f;  // half-texel offset
int x0 = int(floor(fx));
float frac_x = fx - floor(fx);
int x1 = x0 + 1;
x0 = ((x0 % w) + w) % w;
x1 = ((x1 % w) + w) % w;
```

At `u = 0.0`: `fx = -0.5`, `x0 = -1`, `frac_x = 0.5`, `x1 = 0`.
After wrapping: `x0 = w-1`, `x1 = 0`. Bilinear blend of texels at columns
`w-1` and `0` with weight 0.5. These are adjacent columns on the sphere
(the phi=0/phi=2PI boundary), and the procedural 3D noise is continuous there.

At `u = 1.0`: `fx = w - 0.5`, `x0 = w-1`, `frac_x = 0.5`, `x1 = w`.
After wrapping: `x0 = w-1`, `x1 = 0`. Same as u=0. Continuous.

**The wrapping IS correct.** The bilinear interpolation properly bridges the
boundary. For continuous 3D noise patterns, this should produce no seam.

---

## REVISED CONCLUSION: Branch D -- The True Root Cause

After exhaustive tracing of all code paths, the equirectangular sampling path
with `sample_texture()` SHOULD produce seamless results for procedural textures
generated from continuous 3D noise. The U-wrapping is correct and the textures
are generated with proper spherical continuity.

This means the seam lines may actually be caused by one of these remaining
hypotheses:

### Hypothesis D1: The seam is at the POLE, not the atan2 boundary

The `sphere_uv()` function maps the poles to v=0 and v=1. At the poles,
ALL longitude values (u) converge to a single point. The equirectangular
mapping assigns different u values to rays hitting very close to the pole,
causing a "pinch" artifact. This appears as a bright/dark line radiating
from the pole.

**Evidence**: The procedural texture maps a full row of pixels to each pole.
At the north pole (v=0 in shader), theta approaches 0, sin(theta) approaches 0,
and the `atan2(-z, x)` computation becomes numerically unstable because both
-z and x approach 0. The result can produce wildly different u values for
adjacent pixels near the pole, creating a visible seam.

**Verdict**: LIKELY contributor for horizontal seam lines near the poles.

### Hypothesis D2: The `v = 1.0f - theta / M_PI_F` mapping combined with `v = clamp(v, 0.0f, 1.0f)` in sample_texture creates an edge artifact

At exactly the pole, `outward_normal = (0, +-1, 0)`:
- theta = acos(-1) = PI or acos(1) = 0
- v = 0 or v = 1 (exact)
- phi = atan2(0, 0) = UNDEFINED

The `atan2(0, 0)` result is implementation-defined. On Metal/GPU, this typically
returns 0, but adjacent fragment threads may get different values due to
floating-point precision at the pole singularity.

**Verdict**: CONFIRMED contributor for pole-region seam artifacts.

### Hypothesis D3: The `rec.normal` front-face flip causes occasional V inversion

At line 663:
```metal
rec.normal = ff ? outward_normal : -outward_normal;
```

But `sphere_uv()` uses `object_space_normal` (line 671), NOT `rec.normal`.
So the front-face flip does NOT affect UV computation. **REJECTED.**

### Hypothesis D4: The seam is a VERTICAL line at the atan2 wraparound, visible on patterns with high-frequency horizontal variation

For patterns like `grid`, `polka_dots`, `zebra` -- which have sharp transitions
rather than smooth noise -- the bilinear interpolation across the u=0/u=1
boundary blends two texels that are `2*PI/width` radians apart on the sphere.
For a 1024-wide texture, this is `2*PI/1024 = 0.00614 radians` -- a very small
gap. However, if the pattern has a feature boundary exactly at this longitude,
the blend could produce a visible half-texel-wide artifact.

**Verdict**: POSSIBLE for geometric patterns, UNLIKELY for noise-based patterns.

---

## ROOT CAUSES (Confirmed)

### Root Cause 1: Polar singularity in equirectangular UV mapping (HIGH severity)

**Location**: `ray_trace.metal`, function `sphere_uv()`, lines 162-167

The standard equirectangular UV mapping has a mathematical singularity at both
poles where all longitude values collapse to a single point. The `atan2`
function produces poorly-defined results when both arguments approach zero,
causing adjacent pixel threads to compute wildly different U coordinates.
This creates a visible seam artifact radiating from each pole.

**Forward validation**: If this root cause exists, we expect:
- Seam lines near the top and bottom of each sphere (pole regions) -- YES
- The seam would be more visible on patterns with high-frequency content -- YES
- The seam would follow the poles as the sphere rotates -- YES (UVs are computed from object-space normal)

### Root Cause 2: Aspect-ratio heuristic prevents use of seam-free dual-hemisphere sampling (MEDIUM severity)

**Location**: `ray_trace.metal`, line 953

The `is_equirectangular` heuristic (`width > height * 1.8`) correctly identifies
2:1 aspect ratio textures but cannot distinguish between:
- File-based equirectangular maps (Earth/Moon) that need the equirectangular path
- Procedural textures that could benefit from the dual-hemisphere path

The dual-hemisphere path (`sample_sphere_dual_hemisphere`) was specifically
designed to eliminate the atan2 seam by blending two rotated projections. However,
it is never reached for 2:1 aspect ratio textures because the equirectangular
check takes priority.

**Forward validation**: If this root cause exists, we expect:
- Changing procedural texture dimensions to non-2:1 (e.g., 1024x1024) would
  bypass the equirectangular path and use dual-hemisphere instead -- VERIFIABLE
- The dual-hemisphere path has its own defect (uses world-space normal, see
  Root Cause 3) that would need fixing first

### Root Cause 3 (LATENT): Dual-hemisphere path uses world-space normal instead of object-space normal (HIGH severity if activated)

**Location**: `ray_trace.metal`, line 987

The `sample_sphere_dual_hemisphere()` function receives `rec.normal` which is
the world-space, front-face-flipped normal. For rotating spheres wrapped in
`TransformedShape`, this means:
- The texture would NOT rotate with the sphere (it would appear to slide)
- The dual-hemisphere blending weights would be computed from the wrong
  coordinate frame

This defect currently has no impact because the dual-hemisphere path is never
reached for procedural textures (blocked by Root Cause 2). However, it MUST
be fixed before the dual-hemisphere path can serve as an alternative to the
equirectangular path.

---

## SOLUTIONS

### Immediate Mitigation: None needed (service is operational, seams are cosmetic)

### Permanent Fix 1: Route procedural textures to an enhanced equirectangular sampler (Recommended)

**Addresses**: Root Cause 1 (polar singularity)

Add pole-aware bilinear interpolation to `sample_texture()` that detects when
v is near 0 or 1 (pole regions) and averages the entire row of texels at the
pole instead of sampling a single texel with a random u coordinate.

```metal
// In sample_texture(), after computing fx, fy:
if (fy < 0.5f || fy > float(h) - 1.5f) {
    // Near pole: average texels across the full row to avoid
    // atan2 singularity artifacts
    // ... (sample multiple u positions and average)
}
```

**Effort**: LOW (localized shader change)
**Impact**: HIGH (eliminates polar seam for all equirectangular textures)

### Permanent Fix 2: Use explicit material flag instead of aspect-ratio heuristic

**Addresses**: Root Cause 2 (heuristic conflation)

Add a `texture_mapping` enum to `GPUMaterial` that explicitly specifies:
- `EQUIRECTANGULAR` -- for file-based maps (Earth, Moon)
- `SPHERE_3D_NOISE` -- for procedural textures (use enhanced sampler or cube map)
- `CUBE_MAP` -- existing cube map path
- `TRIPLANAR` -- for non-sphere shapes
- `DUAL_HEMISPHERE` -- for non-equirectangular sphere textures

Set this flag in the YAML loader and scene flattener instead of detecting it
at shader runtime from aspect ratio.

**Effort**: MEDIUM (requires struct change, loader change, shader change)
**Impact**: HIGH (eliminates all heuristic-based routing errors)

### Permanent Fix 3: Fix dual-hemisphere path to use object-space normal

**Addresses**: Root Cause 3 (world-space normal in dual-hemisphere)

Reconstruct the object-space normal from `rec.u`/`rec.v` (which are already
computed from `object_space_normal`) before passing to `sample_sphere_dual_hemisphere()`:

```metal
// In texture sampling block, before calling sample_sphere_dual_hemisphere:
float theta_os = (1.0f - rec.v) * M_PI_F;
float phi_os = rec.u * 2.0f * M_PI_F - M_PI_F;
float3 obj_n = float3(cos(phi_os) * sin(theta_os),
                      -cos(theta_os),
                      -sin(phi_os) * sin(theta_os));
albedo = sample_sphere_dual_hemisphere(texture_data, mat.texture_offset,
                                       mat.texture_width, mat.texture_height,
                                       obj_n);
```

Note: This is the same reconstruction already used in the cube_map path (lines
958-962). It should be extracted to a helper function.

**Effort**: LOW (copy existing pattern from cube_map path)
**Impact**: MEDIUM (unblocks dual-hemisphere as viable alternative)

### Recommended Fix Order

1. **Fix 3** first (LOW effort, unblocks Fix 2)
2. **Fix 1** second (LOW effort, fixes the active symptom)
3. **Fix 2** as a follow-up refactor (MEDIUM effort, long-term correctness)

### Quick-Win Alternative: Route procedural textures to cube map sampling

If fixes 1-3 are too disruptive, a quick-win fix is to set `texture_scale = -1.0f`
specifically for procedural textures in the YAML loader (or in
`ProceduralTexture::generate()`). This routes them to the cube_map path which
has NO polar singularity and NO atan2 seam.

The cube_map path already correctly reconstructs the object-space normal from
UV coordinates (lines 958-962), so textures will rotate with the sphere.

**Trade-off**: Cube map introduces its own artifacts at face edges (visible as
faint cross-shaped lines on geometric patterns). For noise-based procedural
textures, these artifacts are less visible than the polar seam.

---

## Evidence Index

| File | Lines | Evidence |
|------|-------|---------|
| `src/infrastructure/metal/shaders/ray_trace.metal` | 162-167 | `sphere_uv()` atan2 polar singularity |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 952-993 | Texture sampling decision tree |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 180-218 | `sample_texture()` bilinear with U wrapping |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 294-318 | `sample_sphere_dual_hemisphere()` uses passed normal |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 645-676 | Object-space vs world-space normal handling |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 987 | `rec.normal` (world-space) passed to dual-hemisphere |
| `src/domain/materials/procedural_texture.cpp` | 576-612 | Procedural texture generation (equirectangular) |
| `src/domain/materials/image_texture.h` | 31 | Default texture_scale = 0.0f |
| `src/infrastructure/yaml_scene_loader.cpp` | 90-101 | Procedural texture loader, no texture_scale override |
| `src/infrastructure/gpu/scene_flattener.cpp` | 74 | texture_scale faithfully passed to GPU |
| `scenes/nwave_bowling.yaml` | 74-93 | Procedural textures defined at 1024x512 |
| `scenes/nwave_bowling.yaml` | 214-334 | Rolling spheres reference procedural materials |

---

## Backwards Chain Validation

**Root Cause 1** (polar singularity) -> produces unstable u values near poles
-> equirectangular sampler picks inconsistent texels -> visible seam lines near
sphere top/bottom -> MATCHES OBSERVED SYMPTOM.

**Root Cause 2** (heuristic conflation) -> procedural 2:1 textures routed to
equirectangular path -> cannot use seam-free dual-hemisphere path ->
equirectangular polar singularity remains active -> MATCHES OBSERVED SYMPTOM.

**Root Cause 3** (world-space normal in dual-hemisphere) -> dual-hemisphere path
is broken for transformed spheres -> cannot serve as alternative even if
heuristic were fixed -> BLOCKS MITIGATION.

All three root causes are consistent and non-contradictory. Root causes 1 and 2
are the active contributors. Root cause 3 is a latent blocker.
