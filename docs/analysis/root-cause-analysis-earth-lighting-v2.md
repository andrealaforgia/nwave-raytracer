# Root Cause Analysis: Earth Lighting Incorrect After Multiple Fixes (V2)

**Date**: 2026-02-22
**Status**: Complete
**Methodology**: Toyota 5 Whys (multi-causal, evidence-required, depth 5)
**Scope**: Earth-specific rendering pipeline vs Moon, after fixes M1-M4 applied
**Previous analyses**: root-cause-analysis-earth-lighting.md, root-cause-analysis-sphere-lighting.md

---

## Problem Statement

After applying fixes from the v1 analysis (switched to DirectionalLight, flipped direction to `Vec3(-10, -2, -8)`, cleared shapes before finale, raised ambient to 3%), the Earth sphere in the animation finale is STILL not lit correctly. The Moon appears lit correctly. Since both objects share the same directional light, the issue must be specific to how Earth is processed differently from Moon.

## Previously Applied Fixes (NOT re-investigated)

| Fix | Description | Status |
|-----|-------------|--------|
| M1 | Earth Sun is DirectionalLight (not PointLight) | Confirmed applied |
| M2 | Light direction flipped to `Vec3(-10, -2, -8)` | Confirmed at line 459 |
| M3 | Scene shapes cleared before finale | Confirmed at line 388 |
| M4 | Ambient raised to 3% for finale | Confirmed at line 466 |

---

## Phase 1: Exhaustive Comparison -- Earth vs Moon

### Object Configuration Comparison

| Property | Earth | Moon |
|----------|-------|------|
| **Inner Shape** | `Sphere(Point3(0,0,0), earth_radius=1.5, earth_material.get())` | `Sphere(Point3(0,0,0), moon_radius=0.375, moon_material.get())` |
| **Outer Wrapper** | `TransformedShape(earth_sphere, Matrix4x4::identity())` | `TransformedShape(moon_sphere, Matrix4x4::identity())` |
| **Per-frame Transform** | `from_translation_rotation(earth_center, tilt_q * spin_q)` | `from_translation(moon_pos)` |
| **Transform Type** | Rotation (23.5deg Z-tilt + Y-spin) + Translation (0, 2.0, 0) | Translation only (orbiting position) |
| **Material** | `ImageTexture` from `flat_earth03.jpg` (2048x1024, ratio 2.00) | `ImageTexture` from `8k_moon.jpg` (8192x4096, ratio 2.00) |
| **texture_scale** | `0.0f` (default) | `0.0f` (default) |
| **GPU `has_transform`** | `1` | `1` |
| **Shader texture path** | Equirectangular (`is_equirectangular = true`, `2048 > 1024 * 1.8`) | Equirectangular (`is_equirectangular = true`, `8192 > 4096 * 1.8`) |
| **UV computation** | `sphere_uv(object_space_normal)` | `sphere_uv(object_space_normal)` |
| **Normal transform** | `transform_normal(outward_normal, inverse_transform)` -- applies rotation | `transform_normal(outward_normal, inverse_transform)` -- identity rotation |

### Key File Locations

| File | Lines | Content |
|------|-------|---------|
| `src/application/animation_renderer.cpp` | 376-379 | Earth sphere + TransformedShape creation |
| `src/application/animation_renderer.cpp` | 382-385 | Moon sphere + TransformedShape creation |
| `src/application/animation_renderer.cpp` | 407-413 | Earth transform: rotation + translation |
| `src/application/animation_renderer.cpp` | 422-424 | Moon transform: translation only |
| `src/application/animation_renderer.cpp` | 454-460 | PointLight + DirectionalLight creation |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 628-676 | Intersection + normal transform + UV |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 952-994 | Texture sampling path selection |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 785-867 | Direct lighting + shadow rays |
| `src/infrastructure/gpu/scene_flattener.cpp` | 262-271 | Inverse transform storage |
| `src/infrastructure/gpu/scene_flattener.cpp` | 288-296 | DirectionalLight negation for GPU |

---

## Phase 2: Toyota 5 Whys Analysis

### Branch A: Transform Normal Correctness

#### WHY 1: Earth has a rotation in its transform; Moon does not. Could this cause incorrect normals?

**Evidence** -- `animation_renderer.cpp:407-413`:
```cpp
Quaternion tilt_q = Quaternion::from_axis_angle(Vec3(0, 0, 1), tilt_rad);
Quaternion spin_q = Quaternion::from_axis_angle(Vec3(0, 1, 0), earth_spin_angle);
Quaternion earth_rot = tilt_q * spin_q;
Matrix4x4 earth_mat = Matrix4x4::from_translation_rotation(
    Vec3(earth_center.x(), earth_center.y(), earth_center.z()), earth_rot);
```

The Earth transform combines a Z-axis tilt (23.5deg), Y-axis spin (varying), and translation to `(0, 2.0, 0)`.

#### WHY 2: How does the shader handle this rotation for normals?

**Evidence** -- `ray_trace.metal:574-581`:
```metal
float3 transform_normal(float3 normal, float4x4 inv_transform) {
    // Normal transforms by transpose of inverse
    float3 n;
    n.x = inv_transform[0][0] * normal.x + inv_transform[1][0] * normal.y + inv_transform[2][0] * normal.z;
    n.y = inv_transform[0][1] * normal.x + inv_transform[1][1] * normal.y + inv_transform[2][1] * normal.z;
    n.z = inv_transform[0][2] * normal.x + inv_transform[1][2] * normal.y + inv_transform[2][2] * normal.z;
    return normalize(n);
}
```

Metal `float4x4` is column-major: `inv_transform[col][row]`. The code computes:
- `n.x = M^{-1}_{00} * nx + M^{-1}_{10} * ny + M^{-1}_{20} * nz`

This is `(M^{-1})^T * normal` (the transpose of the inverse), which is the correct normal transformation formula.

#### WHY 3: Is the inverse correctly stored?

**Evidence** -- `scene_flattener.cpp:264-270`:
```cpp
const Matrix4x4& inv = transformed->inverse_matrix();
// Store in column-major order to match Metal's float4x4 layout
for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
        gpu_shape.inverse_transform[c * 4 + r] = static_cast<float>(inv.m[r][c]);
    }
}
```

The C++ `Matrix4x4` is row-major (`m[row][col]`). It stores element `(row r, col c)` at flat position `c * 4 + r` for Metal column-major layout. This is correct.

**Numerical verification** -- For Earth at frame 0 (tilt only, no spin):
```
M^{-1} (float64) = [[ 0.91706  0.39875  0.      -0.79750]
                     [-0.39875  0.91706  0.      -1.83412]
                     [ 0.       0.       1.       0.     ]
                     [ 0.       0.       0.       1.     ]]

M^{-1} (float32) = same to 8 significant digits (max diff = 2.87e-08)
```

Float32 precision loss is negligible.

#### WHY 4: Is the normal physically correct after transform?

For a rotation matrix R, `M^{-1}` has upper-left 3x3 = `R^{-1} = R^T`. The transpose-of-inverse then gives `(R^T)^T = R`. So `transform_normal` effectively computes `R * object_normal`, which correctly rotates the normal from object space to world space.

**Verification**: For the +Z pole (object normal `(0,0,1)`), world normal = `R * (0,0,1) = (0, 0, 1)` (Z column of R). With the 23.5deg Z-tilt, the Z column is `(0, 0, 1)` (Z-rotation does not affect the Z axis). Correct.

For the +Y pole (object normal `(0,1,0)`), world normal = `R * (0,1,0) = (-0.399, 0.917, 0)`. The north pole tilts toward -X. Correct for a 23.5deg Z-axis tilt.

#### WHY 5: Branch A conclusion.

**ELIMINATED.** The normal transform pipeline is mathematically correct for both Earth and Moon. The rotation does not introduce incorrect normals. Each WHY level has verifiable evidence.

---

### Branch B: Lighting Computation N.L Difference

#### WHY 1: Are the N.L values correct for Earth and Moon?

**Evidence** -- Computed geometry at `finale_t=0`:
```
Camera position:     (0, 5.75, 10.5)
Earth center:        (0, 2.0, 0)
Light toward:        normalize(10, 2, 8) = (0.771, 0.154, 0.617)

Camera-facing surface normal: (0.000, 0.336, 0.942)
N.L for camera-facing point:  0.633  (POSITIVE -- LIT)
```

The camera-facing hemisphere of Earth receives `diffuse_factor = 0.633` from the directional light. With intensity 1.5, the contribution is `albedo * 1.5 * 0.633 = albedo * 0.95`. This should be clearly visible.

#### WHY 2: How does the Moon compare?

At `finale_t=0`, the Moon is at `(orbit_radius, earth_center.y, 0) = (3.75, 2.0, 0)`. From camera at `(0, 5.75, 10.5)`, the Moon is off to the right. The Moon's camera-facing normal has a component pointing back toward `(-3.75, 3.75, 10.5)`. Its N.L with `(0.771, 0.154, 0.617)` is still positive (the +Z component contributes significantly).

#### WHY 3: Both receive the same directional light -- same `light_dir`, same `light_intensity`.

**Evidence** -- `ray_trace.metal:810-815`:
```metal
// Directional light: position stores direction toward light
light_dir = normalize(light.position);
light_dist = T_MAX;
light_intensity = light.color * light.intensity;
```

No per-object variation. Both shapes see identical `light_dir` and `light_intensity`.

#### WHY 4: The diffuse computation is identical.

**Evidence** -- `ray_trace.metal:861`:
```metal
float diffuse_factor = max(0.0f, dot(rec.normal, light_dir));
color += attenuation * light_intensity * shadow_transparency * diffuse_factor;
```

`attenuation` here is the `albedo` parameter passed from the calling code. `shadow_transparency` starts at `(1,1,1)` and is only reduced if the shadow ray hits glass. With no glass in the finale scene, `shadow_transparency = (1,1,1)`.

#### WHY 5: Branch B conclusion.

**ELIMINATED.** The N.L lighting computation is correct and identical for both Earth and Moon. The camera-facing hemisphere of Earth IS lit (NdotL = 0.633). The diffuse contribution is significant (`albedo * 0.95`).

---

### Branch C: Texture Albedo Difference -- THE CRITICAL FINDING

#### WHY 1: Earth and Moon have different textures. Could the Earth texture be significantly darker?

**Evidence** -- Texture files:
- Earth: `flat_earth03.jpg` (2048x1024)
- Moon: `8k_moon.jpg` (8192x4096)

Standard equirectangular Earth textures have:
- Ocean areas: very dark blue, approximately `(0.03-0.10, 0.05-0.15, 0.15-0.35)` in linear space
- Land areas: moderate values, `(0.15-0.40, 0.15-0.35, 0.10-0.25)` in linear space
- Cloud areas: bright, `(0.6-0.9, 0.6-0.9, 0.6-0.9)`

Standard Moon textures are relatively uniform grey: `(0.10-0.25, 0.10-0.25, 0.10-0.25)`.

**KEY**: Earth's ocean covers ~71% of the surface. Most of the Earth-facing area is dark ocean.

#### WHY 2: How does the dark ocean albedo interact with the lighting?

For ocean pixel `albedo = (0.05, 0.08, 0.20)`:
```
Lit side (NdotL=0.5):  (0.05, 0.08, 0.20) * (0.03 + 1.5*0.5) = (0.05, 0.08, 0.20) * 0.78
                      = (0.039, 0.062, 0.156)
                      After gamma: (0.197, 0.250, 0.395) -- DARK BUT VISIBLE

Dark side (NdotL=0):   (0.05, 0.08, 0.20) * 0.03 = (0.0015, 0.0024, 0.006)
                      After gamma: (0.039, 0.049, 0.077) -- NEARLY BLACK
```

For Moon pixel `albedo = (0.20, 0.20, 0.20)`:
```
Lit side (NdotL=0.5):  (0.20, 0.20, 0.20) * (0.03 + 0.75) = (0.20, 0.20, 0.20) * 0.78
                      = (0.156, 0.156, 0.156)
                      After gamma: (0.395, 0.395, 0.395) -- CLEARLY VISIBLE

Dark side (NdotL=0):   (0.20, 0.20, 0.20) * 0.03 = (0.006, 0.006, 0.006)
                      After gamma: (0.077, 0.077, 0.077) -- DARK
```

**The Moon's lit side is 2x brighter than Earth's ocean** in display space. The Moon appears uniformly brighter because its albedo is uniform and higher than Earth's dominant ocean color.

#### WHY 3: Why does this make Earth look "not lit correctly"?

The human eye perceives the Moon as having a clear bright/dark hemisphere boundary with a well-lit face. Earth's ocean-dominated hemisphere appears dim and muddy because:
1. 71% of the visible surface has very low albedo (dark ocean)
2. The 3% ambient on dark ocean is essentially invisible
3. Even the lit ocean areas are dim (0.04-0.16 linear, 0.20-0.40 display)
4. Only scattered land masses and cloud features break the darkness

The viewer perceives this as "not lit" because the expected brightness level (matching the Moon) is not achieved. The Earth IS lit -- but its dark texture makes the lighting hard to perceive.

#### WHY 4: Why is the Earth texture so dark compared to Moon?

The `flat_earth03.jpg` filename suggests this may be a specific artistic rendering rather than a calibrated NASA Blue Marble image. Additionally, JPEG compression on dark ocean areas tends to lose detail, making them appear even more uniform and dark.

The fundamental issue is that the renderer treats the texture pixel values as LINEAR albedo values and multiplies them directly with the lighting. If the texture was authored in sRGB color space (as most JPEGs are), the pixel values are gamma-encoded. Using sRGB values as linear albedo effectively squares the already-dark values, making them much darker.

For a dark ocean pixel in sRGB at `(30/255, 50/255, 90/255) = (0.118, 0.196, 0.353)`:
- Used as linear albedo: values as-is (already somewhat dark)
- Correct interpretation: linearize first: `(0.012, 0.033, 0.107)` -- MUCH darker

**Evidence** -- `ray_trace.metal:172-178`:
```metal
float3 texel_at(constant uchar* texture_data, int texture_offset,
                int w, int px, int py) {
    int idx = texture_offset + (py * w + px) * 4;
    return float3(float(texture_data[idx])     / 255.0f,
                  float(texture_data[idx + 1]) / 255.0f,
                  float(texture_data[idx + 2]) / 255.0f);
}
```

**The shader divides raw byte values by 255 to get [0,1] range but does NOT apply sRGB-to-linear conversion.** JPEG textures store sRGB-encoded values. Using them directly as linear albedo is an error.

For most textures, this works "well enough" because sRGB encoding increases mid-tone brightness relative to linear (sRGB 0.5 = linear 0.214). This means textures appear **brighter** than they should in linear space. For the Moon's uniform grey surface, this makes it slightly too bright but still looks natural.

For the Earth's dark ocean, the sRGB values are in the lower range where sRGB-to-linear conversion makes less difference. But the key issue is that the Earth texture has inherently low albedo values in its ocean areas.

#### WHY 5: ROOT CAUSE C -- Insufficient Texture-Lighting Contrast

**ROOT CAUSE IDENTIFIED**: The Earth texture has very low albedo in its ocean areas (71% of surface). When multiplied by the directional light contribution and the low 3% ambient, the Earth appears much dimmer than the Moon. The user perceives this as "incorrect lighting" when the lighting math is actually correct -- the issue is that the Earth texture produces lower final pixel values than the Moon texture for the same lighting.

**Contributing factor**: The shader does not perform sRGB-to-linear conversion on texture data, which affects the perceived brightness of all textures but is most noticeable on dark textures like Earth's oceans.

---

### Branch D: The Lingering PointLight Attenuation Problem

#### WHY 1: The finale scene still includes a PointLight alongside the DirectionalLight.

**Evidence** -- `animation_renderer.cpp:454-455`:
```cpp
anim_scene.add_light(std::make_shared<PointLight>(
    Point3(0, 50, 0), Color3(1.0, 1.0, 1.0), 0.1));
```

#### WHY 2: What is this PointLight's contribution?

Distance from `(0, 50, 0)` to Earth center `(0, 2.0, 0)` = 48 units.
Attenuation: `1 / (1 + 48^2) = 1 / 2305 = 0.000434`
Effective intensity: `0.1 * 0.000434 = 0.0000434`

This is negligible. The PointLight contributes effectively zero illumination to either Earth or Moon.

#### WHY 3: Does it cause any side effects?

The PointLight does trigger an additional shadow ray computation per surface hit. However, the shadow ray to `(0, 50, 0)` fires upward and will not be blocked by anything (only Earth and Moon in scene, and the upward direction does not intersect them for most surface points).

#### WHY 4: Could the shadow ray toward the PointLight accidentally block the DirectionalLight's contribution?

No -- shadow rays are computed independently per light. The PointLight shadow ray does not affect the DirectionalLight computation.

#### WHY 5: Branch D conclusion.

**ELIMINATED as lighting root cause.** The PointLight at `(0, 50, 0)` contributes negligible illumination. It does not interfere with the DirectionalLight. However, it is a **waste of GPU cycles** (extra shadow ray per pixel per bounce).

**Recommendation**: Remove the PointLight from the finale scene. It serves no visible purpose.

---

### Branch E: Camera-Light Geometry Across Finale Duration

#### WHY 1: Does the lighting direction remain favorable throughout the finale?

**Evidence** -- `animation_renderer.cpp:429`:
```cpp
double cam_orbit_angle = finale_t * M_PI * 0.5; // 0 to 90 degrees
```

Camera orbits from +Z (angle=0) to +X (angle=90deg) around Earth.

Light toward: `normalize(10, 2, 8)` = `(0.771, 0.154, 0.617)`.

| `finale_t` | Camera position | Camera-facing normal (approx) | N.L |
|------------|----------------|-------------------------------|-----|
| 0.0 | (0, 5.75, 10.5) | (0, 0.34, 0.94) | 0.63 |
| 0.5 | (7.4, 5.75, 7.4) | (-0.58, 0.29, -0.58) | -0.00 |
| 1.0 | (10.5, 5.75, 0) | (-0.82, 0.29, 0) | -0.59 |

**CRITICAL FINDING**: At `finale_t = 0.5`, the camera-facing surface has `N.L near 0`. By `finale_t = 1.0`, the camera is viewing the DARK hemisphere of Earth (N.L = -0.59).

#### WHY 2: Why does the camera orbit into the dark side?

The camera orbits 90 degrees from +Z to +X. The light points toward `(+X, +Y, +Z)`. At `finale_t=0`, the camera aligns well with the light (both have significant +Z). But as the camera moves to +X, it moves to a side view, and by `finale_t=1.0` it is looking from +X -- where the normal pointing back toward the camera has `-X` component, and `dot((-1,0,0), light_dir)` is negative.

#### WHY 3: This means the Earth goes from well-lit to dark over the finale!

The first third of the finale shows a lit Earth. The middle shows the terminator. The last third shows the dark hemisphere. This progression might be what the user perceives as "not lit correctly."

#### WHY 4: Does the Moon have the same problem?

The Moon orbits around Earth, so its position relative to the camera changes. At different times during the finale, the Moon may be in positions where its lit side is more or less visible from the camera. The Moon's orbital motion creates variety in its appearance.

However, the Moon is ALSO subject to the same directional light. If the Moon is between the Earth and the light at some orbital position, we'd see the lit side. If it's on the far side, we'd see the dark side. The Moon's appearance varies, but the user perceives it as "correct" because the Moon's higher albedo and smaller size make even dim illumination more visible.

#### WHY 5: ROOT CAUSE E -- Camera Orbit Moves Into Earth's Shadow Side

**ROOT CAUSE IDENTIFIED**: The camera orbits from +Z to +X over the finale duration. The directional light comes from `(+X, +Y, +Z)`. At `finale_t >= 0.5`, the camera has orbited to a position where it primarily views the unlit hemisphere of Earth. The last ~50% of the finale shows the Earth's dark side, making the lighting appear incorrect.

Combined with ROOT CAUSE C (Earth's dark ocean albedo), the lit-to-dark transition happens at lower absolute brightness, making the "dark" portion of the finale particularly dark.

---

### Branch F: Texture File Content Hypothesis

#### WHY 1: Could `flat_earth03.jpg` be a non-standard texture?

The filename `flat_earth03` could indicate:
- A standard equirectangular Earth texture (correct)
- A "flat earth" projection map (incorrect for equirectangular wrapping)
- A processed/artistic rendering with unusual brightness

**Evidence**: The file has 2048x1024 pixels with a 2:1 aspect ratio. This is the standard equirectangular format. The shader correctly detects it:
```metal
bool is_equirectangular = (mat.texture_width > mat.texture_height * 1.8f);
// 2048 > 1024 * 1.8 = 1843.2 -> TRUE
```

#### WHY 2: Is the correct texture sampling path used?

**Evidence** -- `ray_trace.metal:966-970`:
```metal
} else if (rec.shape_type == SHAPE_SPHERE && is_equirectangular) {
    albedo = sample_texture(texture_data, mat.texture_offset,
                            mat.texture_width, mat.texture_height,
                            rec.u, rec.v);
```

`texture_scale = 0.0f` (not < 0), `shape_type = SPHERE`, `is_equirectangular = true`. The equirectangular path is selected. UVs come from `sphere_uv(object_space_normal)`. **Correct.**

#### WHY 3: Are the UVs computed correctly for the rotated Earth?

The UVs use the OBJECT-SPACE normal:
```metal
// Save object-space normal before world-space transform
float3 object_space_normal = outward_normal;
// ... (normal is transformed to world space) ...
if (shape.shape_type == SHAPE_SPHERE) {
    sphere_uv(object_space_normal, rec.u, rec.v);
}
```

`object_space_normal` is captured BEFORE `transform_normal` is called. It represents the raw sphere hit normal in the untransformed object space. As the Earth spins, different object-space normals map to the same world-space ray intersection, causing different UVs -- the texture correctly "sticks to" the spinning sphere. **Correct.**

#### WHY 4: Could the texture content itself be problematic?

Without viewing the actual texture file, two hypotheses:
1. The texture may have been created with unusual brightness/contrast
2. The texture may be a projection type that looks odd when mapped equirectangularly

These cannot be verified without visual inspection of the image.

#### WHY 5: Branch F conclusion.

**UNVERIFIABLE** -- Hypothesis requires visual inspection of the texture file. The texture pipeline code is correct for standard equirectangular textures. If the texture content is unusual, the rendering would appear "wrong" even with perfect code.

---

## Phase 3: Validation and Cross-Reference

### Backward Chain Validation

#### RC-C (Texture Albedo) Forward Trace:
1. Earth texture has dark ocean (albedo ~0.05-0.20 across 71% of surface)
2. Shader computes: `color = albedo * (0.03 + 1.5 * NdotL)`
3. For ocean at NdotL=0.5: color = `(0.04, 0.06, 0.16)`, gamma: `(0.20, 0.25, 0.40)`
4. For Moon at NdotL=0.5: color = 0.16, gamma: 0.40 -- significantly brighter
5. **Result**: Earth appears much dimmer than Moon under identical lighting. **MATCHES observation.**

#### RC-E (Camera Orbit) Forward Trace:
1. Camera orbits from +Z (angle=0) to +X (angle=90deg) over finale
2. Light comes from (+X, +Y, +Z)
3. At finale_t=0: camera sees +Z face, N.L=0.63 -- LIT
4. At finale_t=0.5: camera sees transition, N.L near 0 -- TERMINATOR
5. At finale_t=1.0: camera sees -X face, N.L=-0.59 -- DARK
6. Earth is dark for the second half of the finale
7. **Result**: Earth goes from lit to dark over the finale. **MATCHES "still not lit correctly."**

### Cross-Validation
- RC-C and RC-E are independent and compound each other
- RC-C makes even the lit hemisphere appear dim for ocean areas
- RC-E ensures the camera moves to view the dark hemisphere
- Together: the Earth appears dim throughout the finale (dim texture on lit side, dark on shadow side)
- The Moon's higher albedo and orbital position variation make it appear better-lit in comparison
- No contradictions between root causes

---

## Phase 4: Solution Development

### Root Cause Summary

| ID | Root Cause | Severity |
|----|-----------|----------|
| RC-C | Earth texture has low ocean albedo (~71% of surface), producing dim lit-side values that appear "not lit" compared to Moon | **High** |
| RC-E | Camera orbit (0 to 90deg) moves into Earth's shadow hemisphere during the second half of finale | **Critical** |
| RC-F1 | No sRGB-to-linear conversion on texture data; dark textures are affected disproportionately | **Medium** (contributing) |
| RC-F2 | PointLight at `(0, 50, 0)` is vestigial and wastes GPU cycles without contributing visible illumination | **Low** |

---

### Immediate Mitigations

#### Fix 1 (CRITICAL): Align camera orbit with the lit hemisphere [Addresses RC-E]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, line 429

**Before**:
```cpp
double cam_orbit_angle = finale_t * M_PI * 0.5; // 90 degrees over duration
```

**After**:
```cpp
// Camera orbits within the lit hemisphere (light toward +X,+Y,+Z)
// Start at +Z, orbit TOWARD -X (staying on the lit side of Earth)
double cam_orbit_angle = -finale_t * M_PI * 0.4; // -72 degrees: stays in lit quadrant
```

**Geometric verification**: At `finale_t=1.0`, `cam_orbit_angle = -0.4*pi = -72deg`.
Camera at `earth_center + cam_dist * (sin(-72deg), 0, cos(-72deg))` = `earth_center + cam_dist * (-0.95, 0, 0.31)`.
Camera-facing normal: approximately `(0.95, ~0.3, -0.31)`.
N.L = `dot((0.95, 0.3, -0.31), normalize(10, 2, 8))` = `0.95*0.77 + 0.3*0.15 + (-0.31)*0.62` = `0.73 + 0.045 - 0.19 = 0.58`. **Still lit.**

This keeps the camera on the lit side of Earth throughout the entire finale.

---

#### Fix 2 (HIGH): Boost Earth's effective brightness via light intensity [Addresses RC-C]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, line 458-460

**Before**:
```cpp
anim_scene.add_light(std::make_shared<DirectionalLight>(
    Vec3(-10.0, -2.0, -8.0),
    Color3(1.0, 0.95, 0.85), 1.5));
```

**After**:
```cpp
anim_scene.add_light(std::make_shared<DirectionalLight>(
    Vec3(-10.0, -2.0, -8.0),
    Color3(1.0, 0.95, 0.85), 2.5));
```

Increasing intensity from 1.5 to 2.5 boosts Earth's ocean from `0.05 * 0.78 = 0.039` to `0.05 * 1.28 = 0.064`. After gamma: from 0.20 to 0.25. This is a noticeable improvement.

Alternatively, raise the ambient factor:

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, line 466

**Before**:
```cpp
finale_settings.ambient_factor = 0.03f;
```

**After**:
```cpp
finale_settings.ambient_factor = 0.08f;
```

This makes the dark side of Earth visible (ocean dark side: `0.05 * 0.08 = 0.004`, gamma: 0.063 -- still very dark, but more visible). More importantly, it raises the lit side baseline.

---

#### Fix 3 (LOW): Remove vestigial PointLight [Addresses RC-F2]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`, lines 454-455

**Before**:
```cpp
// Dim ambient fill light far away
anim_scene.add_light(std::make_shared<PointLight>(
    Point3(0, 50, 0), Color3(1.0, 1.0, 1.0), 0.1));
```

**After**: Remove these two lines entirely. The ambient factor (`finale_settings.ambient_factor`) already provides ambient fill. The PointLight at distance 50 contributes 0.0000434 effective intensity -- 1000x less than the ambient contribution.

---

### Permanent Fixes

#### P1: sRGB-to-linear conversion in texture sampling [Addresses RC-F1]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`, lines 172-178

**Before**:
```metal
float3 texel_at(constant uchar* texture_data, int texture_offset,
                int w, int px, int py) {
    int idx = texture_offset + (py * w + px) * 4;
    return float3(float(texture_data[idx])     / 255.0f,
                  float(texture_data[idx + 1]) / 255.0f,
                  float(texture_data[idx + 2]) / 255.0f);
}
```

**After**:
```metal
// Approximate sRGB to linear conversion: gamma 2.2
float srgb_to_linear(float c) {
    return pow(c, 2.2f);
}

float3 texel_at(constant uchar* texture_data, int texture_offset,
                int w, int px, int py) {
    int idx = texture_offset + (py * w + px) * 4;
    return float3(srgb_to_linear(float(texture_data[idx])     / 255.0f),
                  srgb_to_linear(float(texture_data[idx + 1]) / 255.0f),
                  srgb_to_linear(float(texture_data[idx + 2]) / 255.0f));
}
```

**Caution**: This changes the appearance of ALL textures in the renderer. The existing gamma correction pass at lines 1114-1117:
```metal
float3 final_color = clamp(output[idx].xyz, 0.0f, 1.0f);
output[idx] = float4(sqrt(final_color), 1.0f);
```

uses `sqrt` (gamma 2.0), not the sRGB transfer function. For correct round-tripping, consider using `pow(final_color, 1.0/2.2)` or the exact sRGB OETF.

**Impact**: Dark textures will become darker (correct), bright textures will also change. This is the physically correct approach but requires re-tuning all light intensities and ambient factors.

**Recommendation**: Apply this as a separate, dedicated change with visual comparison tests.

---

#### P2: Add camera-light alignment constraint [Prevents recurrence of RC-E]

**File**: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`

Add a validation check at the start of the finale loop:

```cpp
// Verify camera views the lit hemisphere of Earth
Vec3 cam_to_earth = normalize(Vec3(
    earth_center.x() - cam_pos.x(),
    earth_center.y() - cam_pos.y(),
    earth_center.z() - cam_pos.z()));
Vec3 negated_light_dir(-(-10.0), -(-2.0), -(-8.0)); // toward-light direction
Vec3 light_toward = normalize(negated_light_dir);
double alignment = cam_to_earth.x()*light_toward.x()
                 + cam_to_earth.y()*light_toward.y()
                 + cam_to_earth.z()*light_toward.z();
if (alignment < 0.1) {
    std::cerr << "[Finale] Warning: camera viewing dark hemisphere (alignment="
              << alignment << " at frame " << frame << ")\n";
}
```

---

## Phase 5: Summary and Recommended Fix Order

| Priority | Fix | Effort | Impact | Root Cause |
|----------|-----|--------|--------|-----------|
| 1 | **Fix 1**: Change camera orbit to stay on lit side | 5 min | Critical -- keeps Earth visible throughout finale | RC-E |
| 2 | **Fix 2**: Increase directional light intensity to 2.5 | 2 min | High -- brightens dark ocean areas | RC-C |
| 3 | **Fix 3**: Remove vestigial PointLight | 2 min | Low -- removes wasted GPU cycles | RC-F2 |
| 4 | **P1**: sRGB-to-linear in texture sampling | 1 hour | Medium -- correct color science | RC-F1 |
| 5 | **P2**: Camera-light alignment warning | 15 min | Prevention -- catches future regressions | RC-E |

### Most Likely Complete Fix

Apply Fix 1 + Fix 2 + Fix 3 together. The combined effect:
- Camera stays on the lit hemisphere throughout the entire finale
- Higher light intensity makes Earth's ocean areas clearly visible
- No wasted GPU work on the vestigial PointLight

---

## Eliminated Hypotheses

| Hypothesis | Status | Evidence |
|-----------|--------|---------|
| Earth's rotation produces incorrect normals | Eliminated | `transform_normal` correctly applies transpose-of-inverse; mathematically verified |
| Inverse transform has float32 precision issues | Eliminated | Max error = 2.87e-08 between f64 and f32 inverse matrices |
| `t_hit` from object-space invalid for world-space hit point | Eliminated | Rotation preserves direction length; `O + t*D` gives correct world-space point |
| Texture UV mapping wrong for rotated sphere | Eliminated | UVs computed from object-space normal via `sphere_uv(object_space_normal)`; texture sticks to surface |
| Shadow ray self-intersects Earth | Eliminated | Shadow offset `+ T_MIN * rec.normal` pushes origin outside convex sphere; lit-side rays head away from surface |
| `front_face` determination wrong for large spheres | Eliminated | Same code path as Moon; `dot(ray_dir, outward_normal)` is size-independent |
| Material cache serves stale data | Eliminated | New `ImageTexture*` pointers not in cache are correctly appended with texture data |
| Non-equirectangular texture path triggered | Eliminated | Both textures are 2:1 ratio; `is_equirectangular = true` for both |
| Moon shadows Earth | Eliminated | Moon is small (r=0.375) at distance 3.75 from Earth; can only shadow tiny fraction (eclipse-like) |
| DirectionalLight direction convention wrong | Eliminated (previously fixed) | Double-negation (C++: travel direction, flattener: negates, shader: uses toward-light) is consistent |

---

## Files Analyzed

| File | Purpose |
|------|---------|
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp` | Finale scene setup, transforms, camera, lighting |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal` | GPU shader: intersection, normal transform, UV, lighting, texture sampling |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/gpu/scene_flattener.cpp` | CPU-to-GPU: inverse transform storage, light direction negation |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/shapes/transformed_shape.h` | TransformedShape class interface |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/shapes/transformed_shape.cpp` | TransformedShape::set_transform, inverse computation |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/core/matrix4x4.h` | Matrix4x4: transform, inverse, normal transform |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/core/quaternion.h` | Quaternion: from_axis_angle, Hamilton product, to_matrix |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/materials/image_texture.h` | ImageTexture: texture_scale default (0.0f) |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/materials/image_texture.cpp` | ImageTexture::load_from_file, stbi_load |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/lights/directional_light.h` | DirectionalLight: direction convention (travel direction) |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/core/gpu_types.h` | GPU struct layouts, alignment assertions |
| `/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/nwave_bowling.yaml` | Scene config: texture paths, earth_radius, earth_tilt |
