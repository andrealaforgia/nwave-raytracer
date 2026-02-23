# Root Cause Analysis: Earth Lighting Incorrect in Animation Finale

**Date**: 2026-02-22
**Status**: Analysis Complete
**Methodology**: Toyota 5 Whys (multi-causal, evidence-required)
**Scope**: Earth vs Moon lighting in the animation finale (frames `finale_start` to `total_frames`)

---

## Problem Statement

In the ray tracer's animation finale, the Moon appears correctly lit but the Earth appears incorrectly lit. The user has already applied fix M1 (switched the Earth Sun from `PointLight` to `DirectionalLight`). The investigation determines exactly how lighting is computed for both objects and identifies what differs for Earth.

---

## Phase 1: Problem Scoping and Evidence Collection

### Affected Systems
- `src/application/animation_renderer.cpp` (finale loop, lines 336-484)
- `src/infrastructure/metal/shaders/ray_trace.metal` (lighting kernel)
- `src/infrastructure/gpu/scene_flattener.cpp` (light direction negation)

### Objects Under Investigation

| Property | Earth | Moon |
|---|---|---|
| Material | `ImageTexture` (equirectangular map) | `ImageTexture` (equirectangular map) **or** `Lambertian(0.7, 0.7, 0.7)` fallback |
| Shape | `Sphere(0,0,0, earth_radius)` | `Sphere(0,0,0, moon_radius)` |
| Transform | `from_translation_rotation(earth_center, tilt_q * spin_q)` | `from_translation(moon_pos)` -- translation only |
| `has_transform` | **1** (always) | **1** (always) |
| `texture_scale` | `0.0f` (default from ImageTexture) | `0.0f` (default from ImageTexture) |

---

## Phase 2: Toyota 5 Whys Analysis

### Branch A: Directional Light Direction Convention

#### WHY 1 (Symptom): The Earth appears to be lit from the wrong side.

**Evidence**: The DirectionalLight is created in `animation_renderer.cpp:458-460`:

```cpp
anim_scene.add_light(std::make_shared<DirectionalLight>(
    Vec3(-10.0, -2.0, 8.0),
    Color3(1.0, 0.95, 0.85), 1.5));
```

The `DirectionalLight` class documents its direction as:
```cpp
// directional_light.h:19
Vec3 direction_; // direction the light travels (away from source)
```

So `Vec3(-10.0, -2.0, 8.0)` means light **travels** in the direction (-10, -2, 8) -- i.e., toward negative-X, slightly downward, and toward positive-Z.

#### WHY 2 (Context): The scene_flattener negates the direction before sending it to the GPU.

**Evidence** -- `scene_flattener.cpp:290-292`:
```cpp
gpu_light.position[0] = static_cast<float>(-dir->direction().x());
gpu_light.position[1] = static_cast<float>(-dir->direction().y());
gpu_light.position[2] = static_cast<float>(-dir->direction().z());
```

The negated direction stored in `gpu_light.position` is **(+10, +2, -8)** (normalized).

#### WHY 3 (System): The shader interprets `light.position` as "direction toward the light" for directional lights.

**Evidence** -- `ray_trace.metal:811-812`:
```metal
// Directional light: position stores direction toward light
light_dir = normalize(light.position);
```

So `light_dir` = `normalize(10, 2, -8)` which points toward **(+X, slight +Y, -Z)**.

#### WHY 4 (Design): The diffuse factor is computed as `dot(rec.normal, light_dir)`.

**Evidence** -- `ray_trace.metal:861`:
```metal
float diffuse_factor = max(0.0f, dot(rec.normal, light_dir));
```

Surface points whose world-space normal has a positive dot product with `normalize(10, 2, -8)` are illuminated. This means the **+X, -Z hemisphere** of any object is lit.

#### WHY 5 (Root Cause A): For the Moon (translation-only transform), `rec.normal` is the straightforward outward normal in world space. For the Earth (rotation+translation transform), `rec.normal` is the outward normal **after** being transformed through the inverse-transpose of the tilt+spin rotation. The Moon's normals directly face outward toward the light. The Earth's normals are correct in world space (they are properly transformed), so the lighting direction itself is not the root cause -- both should illuminate the same hemisphere. The directional light convention is **correct**.

**Assessment**: Branch A is **not a root cause**. The double-negation (C++ stores travel direction, flattener negates to get toward-light direction, shader uses toward-light direction) is consistent. Both Earth and Moon receive the same `light_dir` in the shader. This branch is eliminated.

---

### Branch B: Normal Computation Difference (Earth has rotation, Moon does not)

#### WHY 1 (Symptom): Earth uses `from_translation_rotation` while Moon uses `from_translation`. Both result in `has_transform = 1` on the GPU.

**Evidence** -- `animation_renderer.cpp:407-413` (Earth):
```cpp
Quaternion tilt_q = Quaternion::from_axis_angle(Vec3(0, 0, 1), tilt_rad);
Quaternion spin_q = Quaternion::from_axis_angle(Vec3(0, 1, 0), earth_spin_angle);
Quaternion earth_rot = tilt_q * spin_q;
Matrix4x4 earth_mat = Matrix4x4::from_translation_rotation(
    Vec3(earth_center.x(), earth_center.y(), earth_center.z()), earth_rot);
earth_transformed->set_transform(earth_mat);
```

`animation_renderer.cpp:422-424` (Moon):
```cpp
Matrix4x4 moon_mat = Matrix4x4::from_translation(
    Vec3(moon_pos.x(), moon_pos.y(), moon_pos.z()));
moon_transformed->set_transform(moon_mat);
```

#### WHY 2 (Context): Both go through the same `intersect_scene` code path on the GPU. The ray is transformed into object space, intersection occurs against a sphere at origin, then the outward normal is transformed back to world space.

**Evidence** -- `ray_trace.metal:628-663`:
```metal
Ray test_ray = ray;
if (shape.has_transform) {
    test_ray = transform_ray(ray, shape.inverse_transform);
}
// ... intersection ...
if (did_hit) {
    float3 object_space_normal = outward_normal;
    if (shape.has_transform) {
        rec.point = ray.origin + t_hit * ray.direction;
        outward_normal = transform_normal(outward_normal, shape.inverse_transform);
    }
    bool ff = dot(ray.direction, outward_normal) < 0.0f;
    rec.front_face = ff;
    rec.normal = ff ? outward_normal : -outward_normal;
```

#### WHY 3 (System): For Earth, the inverse transform includes the inverse of the tilt+spin rotation. The `transform_normal` function applies the **transpose of the inverse** matrix to the normal. For a pure rotation matrix, transpose-of-inverse = the rotation itself. So Earth's world-space normals are correctly rotated. They properly account for the tilt and spin.

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

For a rotation matrix R, `inverse_transform = R^(-1) = R^T`. Then `transpose(R^T) = R`. So normals are multiplied by R, which correctly rotates them to world space. This is mathematically correct.

#### WHY 4 (Design): However, the **hit point** computation uses the original (world-space) ray, not the object-space intersection. `rec.point = ray.origin + t_hit * ray.direction`. The `t_hit` value was computed in **object space** against the transformed ray. For a pure translation, `t` is the same in both spaces (direction unchanged). For a rotation, `t` differs because `test_ray.direction` has a different length/direction than `ray.direction`.

**CRITICAL FINDING** -- `ray_trace.metal:654`:
```metal
rec.point = ray.origin + t_hit * ray.direction;
```

But `t_hit` was solved from `test_ray` (the object-space ray). The object-space ray was created by:
```metal
Ray transform_ray(Ray ray, float4x4 inv_transform) {
    float4 o = inv_transform * float4(ray.origin, 1.0f);
    float4 d = inv_transform * float4(ray.direction, 0.0f);
    transformed.origin = o.xyz;
    transformed.direction = d.xyz;  // NOT normalized
    return transformed;
}
```

The transformed direction `d.xyz` is **not** normalized after the matrix multiply. For a pure rotation, `|d.xyz| == |ray.direction|` (rotations preserve length), so `t_hit` gives the same parametric distance. For a translation-only matrix, the direction is unchanged. So for both Earth and Moon, `t_hit` should be valid for computing `rec.point` from the world-space ray.

**Wait** -- re-examining: the `inv_transform` for Earth includes both inverse-rotation and inverse-translation. When applied to the direction vector (w=0), only the rotation part applies. Since rotation preserves length, `t_hit` is geometrically consistent.

#### WHY 5 (Branch B conclusion): The normal transform pipeline is **mathematically correct** for both Earth and Moon. The normals, hit points, and front-face determination all follow the same code path and are correct for rotation transforms. This branch is **not a root cause**.

---

### Branch C: Texture Sampling Path Difference Between Earth and Moon

#### WHY 1 (Symptom): Earth and Moon both use `ImageTexture`, which sets `texture_scale = 0.0f` (default). Both are `SHAPE_SPHERE`. The texture sampling path selection depends on the texture aspect ratio.

**Evidence** -- `ray_trace.metal:952-993`, the texture path selection:
```metal
if (mat.texture_offset >= 0) {
    bool is_equirectangular = (mat.texture_width > mat.texture_height * 1.8f);
    if (mat.texture_scale < 0.0f && rec.shape_type == SHAPE_SPHERE) {
        // Cube map mode
    } else if (rec.shape_type == SHAPE_SPHERE && is_equirectangular) {
        // Equirectangular maps (Earth, Moon): standard sphere UV with wrapping
        albedo = sample_texture(texture_data, mat.texture_offset,
                                mat.texture_width, mat.texture_height,
                                rec.u, rec.v);
    } else if (rec.shape_type == SHAPE_SPHERE && mat.texture_scale > 0.0f) {
        // Scaled texture on sphere
    } else if (rec.shape_type == SHAPE_SPHERE) {
        // Non-equirectangular on sphere: dual-hemisphere blending
        albedo = sample_sphere_dual_hemisphere(..., rec.normal);
    }
}
```

Both Earth and Moon textures:
- `texture_scale = 0.0f` (not < 0, not > 0), so cube-map and scaled modes are skipped.
- If the texture is equirectangular (width > height * 1.8), the equirectangular path is used, sampling via `rec.u, rec.v`.
- If not equirectangular, the dual-hemisphere path is used, sampling via `rec.normal`.

#### WHY 2 (Context): The equirectangular path uses `rec.u` and `rec.v`, which were computed from the **object-space normal** via `sphere_uv()`.

**Evidence** -- `ray_trace.metal:670-671`:
```metal
if (shape.shape_type == SHAPE_SPHERE) {
    sphere_uv(object_space_normal, rec.u, rec.v);
}
```

This is correct -- UVs are derived from the object-space normal so the texture "sticks to" the sphere as it rotates. The texture appearance will correctly follow Earth's tilt and spin.

#### WHY 3 (System): If either texture is **not** equirectangular (width/height ratio <= 1.8), the code falls through to `sample_sphere_dual_hemisphere`, which uses `rec.normal` (the **world-space** normal). This would be **incorrect** for a textured Earth because `rec.normal` rotates with the transform, meaning the texture would appear fixed in world space rather than rotating with the sphere.

**However**: standard Earth and Moon textures from NASA or similar sources are equirectangular (2:1 aspect ratio), so they would take the equirectangular path using UVs. This branch matters only if a non-equirectangular texture is loaded.

#### WHY 4 (Design): **The texture sampling path is not the root cause of the lighting problem.** Texture sampling affects `albedo` (the color), not the diffuse lighting factor `dot(rec.normal, light_dir)`. Even if the albedo were wrong, the lit/dark hemispheres would be correct -- you'd just see the wrong colors. The user's complaint is about **which side is lit**, not about the texture appearing wrong.

#### WHY 5 (Branch C conclusion): Texture sampling differences cannot cause the Earth to appear lit from the wrong side. This branch is **not a root cause** for the lighting direction problem, though it may contribute to visual anomalies.

---

### Branch D: Shadow Ray Interaction and Self-Shadowing

#### WHY 1 (Symptom): The lighting depends not only on `dot(rec.normal, light_dir)` but also on whether the shadow ray is blocked.

**Evidence** -- `ray_trace.metal:817-862`:
```metal
float3 shadow_origin = rec.point + T_MIN * rec.normal;
// ... trace shadow ray toward light ...
if (!fully_blocked) {
    float diffuse_factor = max(0.0f, dot(rec.normal, light_dir));
    color += attenuation * light_intensity * shadow_transparency * diffuse_factor;
}
```

#### WHY 2 (Context): For a sphere, the shadow ray origin is `rec.point + T_MIN * rec.normal`. The shadow ray direction is `light_dir`. If the light direction is `normalize(10, 2, -8)`, the shadow ray fires in that direction from a point slightly outside the sphere surface.

For points on the **back hemisphere** (facing away from the light), `dot(rec.normal, light_dir) <= 0`, so `diffuse_factor = 0` regardless of shadow. Those points are dark because of the N-dot-L computation, not shadow blocking.

For points on the **front hemisphere** (facing the light), the shadow ray fires outward from the surface. It should NOT intersect the Earth sphere itself because it starts outside the surface (`rec.point + T_MIN * rec.normal`).

#### WHY 3 (System): **Could the Moon shadow the Earth?** The Moon orbits at `orbit_radius = earth_radius * 2.5`. The Moon's radius is `earth_radius * 0.25`. The Moon could geometrically block some light from reaching parts of the Earth, but only a tiny fraction (like a real eclipse). This is not the systemic lighting failure described.

#### WHY 4 (Design): Shadow rays are not the root cause. The N-dot-L computation is the primary determinant of which hemisphere is bright.

#### WHY 5 (Branch D conclusion): Shadow interaction is **not a root cause**.

---

### Branch E: The "Correct" Hemisphere -- Is the Perceived Problem Actually About Light Direction vs Camera Position?

#### WHY 1 (Symptom): The user perceives Earth as incorrectly lit. But the Moon appears correctly lit. Both receive the same directional light. Both have correctly computed world-space normals. What could make them appear different to the viewer?

#### WHY 2 (Context): The key difference is **camera position relative to the lit hemisphere**.

**Evidence** -- Camera position (`animation_renderer.cpp:429-439`):
```cpp
double cam_orbit_angle = finale_t * M_PI * 0.5; // 0 to 90 degrees
double cam_distance = earth_radius * 7.0;
double cam_elevation = earth_center.y() + earth_radius * 2.5;
Point3 earth_cam_pos(
    earth_center.x() + cam_distance * std::sin(cam_orbit_angle),
    cam_elevation,
    earth_center.z() + cam_distance * std::cos(cam_orbit_angle));
```

At `finale_t = 0`: camera is at `(0, elevation, cam_distance)` -- positive Z.
At `finale_t = 1`: camera is at `(cam_distance, elevation, 0)` -- positive X.

**Light direction** (toward light): `normalize(10, 2, -8)` which is roughly **(+X, slight +Y, -Z)**.

The lit hemisphere of Earth/Moon faces toward (+X, -Z). At `finale_t = 0` the camera is at +Z, looking at Earth. The camera sees primarily the **-Z face of Earth**, but the light illuminates the **-Z face** (light_dir has -Z component), so the front of the Earth should actually be partially lit. Let me re-examine.

Actually, `light_dir = normalize(10, 2, -8)`. The dot product `dot(N, light_dir)` is maximal when N points toward `(10, 2, -8)`. So the brightest point on a sphere is the point whose normal points in direction `(10, 2, -8)` -- the point at the (+X, +Y, -Z) corner of the sphere.

At `finale_t = 0`, the camera looks from (0, elev, +cam_distance) toward origin. The camera sees the +Z hemisphere of Earth. The lit region is the -Z hemisphere. So **at the start of the finale, the camera is looking at the dark side of Earth**.

#### WHY 3 (System): The Moon orbits around Earth. At `finale_t = 0`:
```cpp
double moon_angle = 0;
Point3 moon_pos(earth_center.x() + orbit_radius * cos(0), // +X
                earth_center.y(),
                earth_center.z() + orbit_radius * sin(0)); // 0 Z
```

The Moon is at (+orbit_radius, earth_center.y, 0), offset along +X from Earth. From the camera at +Z, the Moon appears off to the right side. The Moon's lit hemisphere is the -Z, +X side. Since the Moon is already offset in +X, the camera can see some of the Moon's lit side through parallax.

As the Moon orbits (negative angle), it moves through the scene. Because the Moon is smaller and further from the camera's direct line of sight, the viewer may see more of its lit side than Earth's.

#### WHY 4 (Design): **The fundamental issue is that the directional light direction and the initial camera position are configured such that the camera looks at the dark side of Earth.**

The light vector in the C++ code is `Vec3(-10.0, -2.0, 8.0)` -- this is the direction light **travels** (away from source). So light **comes from** the direction opposite to this: **(+10, +2, -8)**, meaning the Sun is located conceptually in the (+X, +Y, -Z) octant.

The camera starts at approximately (0, elev, +cam_distance), looking at Earth at origin. The camera is at +Z, but the Sun is at -Z. The camera is **directly opposite the Sun**, viewing Earth's unlit hemisphere.

#### WHY 5 (Root Cause): **ROOT CAUSE IDENTIFIED -- The directional light vector `Vec3(-10.0, -2.0, 8.0)` places the Sun behind the camera's initial position, causing the camera to view Earth's dark hemisphere throughout most of the finale.**

The Moon appears correctly lit because:
1. The Moon orbits around Earth, so during parts of its orbit it is positioned such that its lit side faces the camera.
2. The Moon is smaller and offset, so even when partially lit from the side, it appears "more correct" to the viewer.
3. The Moon does NOT have a rotation transform (translation only), so any potential visual artifacts from the tilt/spin are absent.

---

## Phase 3: Validation (Backwards Chain)

### Forward trace from root cause:

1. `DirectionalLight(Vec3(-10, -2, 8), ...)` means light travels in direction (-10, -2, +8)
2. `scene_flattener.cpp:290-292` negates: GPU receives `position = (+10, +2, -8)`
3. `ray_trace.metal:812`: `light_dir = normalize(10, 2, -8)`
4. Maximum illumination at sphere point whose normal = `normalize(10, 2, -8)` -- the (+X, +Y, -Z) face
5. Camera at `finale_t=0` is at `(0, elev, +cam_distance)` -- looking at the +Z face
6. The +Z face has `dot(normal, light_dir)` dominated by the Z component: `dot((0,0,1), normalize(10,2,-8))` = `-8/sqrt(168)` < 0
7. Therefore: the face visible to the camera has negative N-dot-L -- it is **dark**

**Validation: CONFIRMED.** The camera views Earth's unlit hemisphere.

### Why Moon appears better:

The Moon at `finale_t=0` is at position `(orbit_radius, earth_y, 0)`. From the camera at `(0, elev, +Z)`, the Moon is off to the side. The Moon's visible face normal (from camera's perspective) has a +X component. `dot(normalize(~1,0,~-small_z), normalize(10,2,-8))` has a positive +X contribution, making it partially lit.

**Validation: CONFIRMED.** The Moon's orbital position creates a different viewing angle relative to the light.

---

## Phase 4: Solution Development

### Root Cause Summary

| ID | Root Cause | Severity |
|---|---|---|
| RC-1 | Directional light direction `Vec3(-10, -2, 8)` places the Sun behind the initial camera position, so the camera views Earth's dark hemisphere | **Critical** |
| RC-2 (Contributing) | Camera orbit range (0 to 90 degrees) never brings the camera to a position where Earth's lit hemisphere (+X, -Z face) is fully visible | **High** |

### Immediate Mitigation (M2): Flip the light direction Z-component

Change `animation_renderer.cpp:459` from:
```cpp
Vec3(-10.0, -2.0, 8.0)
```
to:
```cpp
Vec3(-10.0, -2.0, -8.0)
```

This places the Sun in the (+X, +Y, +Z) octant, illuminating the +Z face of Earth -- the same face the camera sees at `finale_t=0`.

**Geometric verification**:
- Light travels in direction `(-10, -2, -8)`, so Sun is in the `(+10, +2, +8)` direction.
- Camera at `finale_t=0` is at `(0, elev, +cam_dist)`.
- Earth's +Z hemisphere normal: `(0, 0, 1)`.
- `dot((0,0,1), normalize(10, 2, 8))` = `+8/sqrt(168)` > 0. **Lit.**

### Alternative Mitigation (M3): Adjust camera starting position to view the lit hemisphere

Instead of changing the light, change `cam_orbit_angle` to start at an angle that views the +X, -Z face:
```cpp
double cam_orbit_angle = M_PI * 0.75 + finale_t * M_PI * 0.5; // start at 135 degrees
```

This is less desirable because it changes the camera motion aesthetics.

### Permanent Fix (P1): Parameterize the Sun direction in `FinaleConfig`

Add a `sun_direction` field to `FinaleConfig` so the light direction can be adjusted without recompiling:
```cpp
struct FinaleConfig {
    // ... existing fields ...
    Vec3 sun_direction{-10.0, -2.0, -8.0}; // direction light travels
};
```

Then in `animation_renderer.cpp`:
```cpp
anim_scene.add_light(std::make_shared<DirectionalLight>(
    config_.finale.sun_direction,
    Color3(1.0, 0.95, 0.85), 1.5));
```

### Early Detection Measure

Add a debug assertion or log that computes `dot(camera_forward, light_dir)` and warns if the camera is looking at the unlit hemisphere:
```cpp
Vec3 cam_forward = normalize(earth_center - cam_pos);
Vec3 sun_toward = normalize(-config_.finale.sun_direction);
double alignment = dot(cam_forward, sun_toward);
if (alignment < 0.0) {
    std::cerr << "[Finale] Warning: camera is viewing the dark side of Earth "
              << "(dot=" << alignment << ")\n";
}
```

---

## Phase 5: Evidence Summary

### Key Code Locations

| File | Lines | Evidence |
|---|---|---|
| `src/application/animation_renderer.cpp` | 458-460 | DirectionalLight created with `Vec3(-10, -2, 8)` |
| `src/application/animation_renderer.cpp` | 407-413 | Earth tilt+spin rotation transform |
| `src/application/animation_renderer.cpp` | 422-424 | Moon translation-only transform |
| `src/application/animation_renderer.cpp` | 429-439 | Camera starts at +Z, orbits to +X |
| `src/domain/lights/directional_light.h` | 19 | `direction_` = "direction light travels (away from source)" |
| `src/infrastructure/gpu/scene_flattener.cpp` | 290-292 | Negates direction for GPU: `position = -direction` |
| `ray_trace.metal` | 811-812 | `light_dir = normalize(light.position)` -- toward light |
| `ray_trace.metal` | 861 | `diffuse_factor = max(0, dot(rec.normal, light_dir))` |
| `ray_trace.metal` | 574-581 | `transform_normal` correctly applies transpose-of-inverse |
| `ray_trace.metal` | 649-671 | Object-space normal saved for UV, world-space normal for shading |

### Direction Convention Trace (end-to-end)

```
C++ DirectionalLight:  Vec3(-10, -2, +8)     "direction light TRAVELS"
                        |
scene_flattener:       negates to (+10, +2, -8)  "direction TOWARD light"
                        |
GPU light.position:    (10, 2, -8)
                        |
Shader light_dir:      normalize(10, 2, -8)   "direction TOWARD light"
                        |
N.L diffuse:           dot(surface_normal, normalize(10, 2, -8))
                        |
Brightest point:       surface normal aligned with (+10, +2, -8)
                        = the (+X, +Y, -Z) octant face of the sphere
                        |
Camera at t=0:         (0, elev, +Z)  -- viewing the +Z face
                        |
Result:                Camera sees the DARK side (Z-components opposed)
```

---

## Eliminated Hypotheses

| Hypothesis | Status | Reason |
|---|---|---|
| Earth's tilt/rotation affects shading normal incorrectly | Eliminated | `transform_normal` correctly applies transpose-of-inverse; rotation preserves normal correctness |
| Texture mapping interferes with lighting | Eliminated | Texture affects albedo color, not N-dot-L diffuse factor |
| `front_face` determination wrong for large spheres | Eliminated | Same code path for both Earth and Moon; front_face is based on `dot(ray_dir, outward_normal)` which is size-independent |
| Ambient factor too high masking directional light | Eliminated | `ambient_factor = 0.03f` is very low; directional light has intensity 1.5 |
| Shadow rays blocked incorrectly | Eliminated | Shadow origin offset `+ T_MIN * rec.normal` prevents self-intersection |
| Moon uses different code path than Earth | Eliminated | Both are `SHAPE_SPHERE` with `ImageTexture` (or Lambertian fallback); same intersection and lighting code |
| `t_hit` mismatch between object-space and world-space | Eliminated | Rotation preserves direction vector length, so parametric `t` is consistent |

---

## Recommended Fix Priority

1. **M2 (Immediate)**: Flip Z-component of light direction in `animation_renderer.cpp:459`
   `Vec3(-10.0, -2.0, 8.0)` --> `Vec3(-10.0, -2.0, -8.0)`
2. **P1 (Permanent)**: Parameterize Sun direction in `FinaleConfig` for YAML configurability
3. **Detection**: Add camera-vs-light alignment warning log
