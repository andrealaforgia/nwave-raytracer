# Root Cause Analysis: Textured Spheres Not Visually Rotating

**Date**: 2026-02-22
**Severity**: Visual correctness defect
**Status**: Root cause identified, fix applied in working copy (uncommitted)

---

## Problem Statement

The 20 textured spheres that roll across the plane after the glass sweeper passes
are NOT visually rotating. Despite the previous fix that added `friction: 0.3` and
`initial_angular_velocity: [0.0, 0.0, -25.0]` to all 20 spheres in
`scenes/nwave_bowling.yaml`, the spheres slide across the surface without their
texture patterns rotating.

**Impact**: All 20 textured spheres appear to slide instead of roll, breaking the
visual realism of the bowling demo animation.

**Scope**: Affects only the main animation loop for dynamic spheres with textures.
The Earth finale (which uses explicit `from_translation_rotation()` at line 410)
is NOT affected.

---

## Investigation Scope

- **Affected systems**: animation_renderer.cpp (per-frame transform pipeline),
  scene_flattener.cpp (GPU transform upload), ray_trace.metal (sphere UV computation)
- **Time range**: Introduced by commit `9538da9` on 2026-02-19
- **Unrelated**: Physics engine (Jolt) correctly simulates rotation; GPU texture
  sampling is correct; YAML configuration is correct

---

## Evidence Trail

### E1: YAML Configuration (Confirmed Correct)

File: `/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/nwave_bowling.yaml`
Lines 220, 226, 232, etc. (all 20 spheres)

```yaml
physics: { body_type: dynamic, mass: 2.0, friction: 0.3, restitution: 0.3,
           initial_velocity: [5.0, 0.0, 0.0],
           initial_angular_velocity: [0.0, 0.0, -25.0],
           start_asleep: true, wake_frame: 160 }
```

All 20 spheres have `initial_angular_velocity: [0.0, 0.0, -25.0]` and
`friction: 0.3`. Configuration is correct.

### E2: Physics Pipeline (Confirmed Correct)

File: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/jolt_physics_simulator.cpp`

**Body creation** (lines 225-233): Angular velocity is set on the Jolt body at creation:
```cpp
body_settings.mAngularVelocity = JPH::Vec3(
    static_cast<float>(desc.properties.initial_angular_velocity.x()),
    static_cast<float>(desc.properties.initial_angular_velocity.y()),
    static_cast<float>(desc.properties.initial_angular_velocity.z()));
```

**Transform retrieval** (lines 262-285): `get_transform()` returns BOTH position
AND rotation quaternion from Jolt:
```cpp
JPH::RVec3 pos = body_interface.GetCenterOfMassPosition(jolt_id);
JPH::Quat rot = body_interface.GetRotation(jolt_id);
// ... populates PhysicsTransform with both position and rotation
```

**Angular velocity API** (lines 322-331): `set_angular_velocity()` correctly
calls Jolt's `SetAngularVelocity()`.

### E3: Per-Body Wake Logic (Confirmed Correct)

File: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`
Lines 291-305:

```cpp
// Per-body wake
for (int i = 0; i < shape_count; ++i) {
    if (i < static_cast<int>(shape_physics_.size()) &&
        should_wake_at_frame(shape_physics_[i], frame)) {
        physics_->wake_body(body_ids[i]);
        const auto& vel = shape_physics_[i].initial_velocity;
        if (vel.x() != 0.0 || vel.y() != 0.0 || vel.z() != 0.0) {
            physics_->set_linear_velocity(body_ids[i], vel);
        }
        const auto& ang_vel = shape_physics_[i].initial_angular_velocity;
        if (ang_vel.x() != 0.0 || ang_vel.y() != 0.0 || ang_vel.z() != 0.0) {
            physics_->set_angular_velocity(body_ids[i], ang_vel);
        }
    }
}
```

When each sphere's `wake_frame` is reached, the body is activated and angular
velocity is explicitly set. This is correct.

### E4: The Defect -- `is_sphere` Flag Causes Translation-Only Transform

File: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`

**Commit `9538da9`** (2026-02-19) introduced this optimization:

```cpp
// Line 112 (in commit 9538da9):
is_sphere[i] = dynamic_cast<const Sphere*>(original_shapes[i].get()) != nullptr;
```

This flag is used at two critical points:

**Initial transform capture** (lines 135-136):
```cpp
PhysicsTransform init_t = physics_->get_transform(body_ids[i]);
Matrix4x4 init_mat = build_transform(init_t, is_sphere[i]);
```

**Per-frame transform update** (lines 214-215):
```cpp
PhysicsTransform phys_transform = physics_->get_transform(body_ids[i]);
Matrix4x4 current = build_transform(phys_transform, is_sphere[i]);
```

The `build_transform` function (lines 78-83):
```cpp
Matrix4x4 build_transform(const PhysicsTransform& t, bool translation_only) {
    if (translation_only) {
        return Matrix4x4::from_translation(t.position);
    }
    return Matrix4x4::from_translation_rotation(t.position, t.rotation);
}
```

When `is_sphere[i]` was `true` (the committed code), `build_transform` received
`translation_only = true` and called `Matrix4x4::from_translation(t.position)`,
which constructs a transform with NO rotation component. The rotation quaternion
from physics was completely discarded.

### E5: Commit Message Reveals the Flawed Assumption

Commit `9538da9` message:
> "A perfect sphere is rotationally symmetric, so applying physics rotation
> to it is wasteful and physically meaningless for reflections."

This assumption is correct for untextured spheres but WRONG for textured spheres.
A textured sphere's appearance DEPENDS on its orientation because the texture UV
coordinates are derived from the object-space surface normal. When the rotation is
stripped, the object-space normal is always the same for a given viewing direction,
producing static (non-rotating) texture appearance.

### E6: GPU Pipeline Depends on Rotation for Texture UV

File: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal`

**Scene flattener** (`/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/gpu/scene_flattener.cpp`, lines 262-271):
The `TransformedShape`'s inverse transform matrix is stored in the GPU shape struct:
```cpp
if (transformed) {
    gpu_shape.has_transform = 1;
    const Matrix4x4& inv = transformed->inverse_matrix();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            gpu_shape.inverse_transform[c * 4 + r] = static_cast<float>(inv.m[r][c]);
        }
    }
}
```

**Shader ray transformation** (ray_trace.metal, lines 647-649):
```metal
if (shape.has_transform) {
    test_ray = transform_ray(ray, shape.inverse_transform);
}
```

**Object-space normal for UV** (ray_trace.metal, lines 668-694):
```metal
float3 object_space_normal = outward_normal;  // before world-space transform
// ...
if (shape.shape_type == SHAPE_SPHERE) {
    sphere_uv(object_space_normal, rec.u, rec.v);
}
```

The texture UV is computed from the object-space normal. The object-space normal
is computed by intersecting the inverse-transformed ray with the local-space sphere.
If the inverse transform contains no rotation component, the object-space normal
for a given world-space viewing direction is always the same, regardless of the
sphere's physical rotation. This is WHY the texture appears static.

When rotation IS included in the transform, the inverse-transformed ray lands at
different local-space positions as the sphere rotates, producing different
object-space normals and thus different UV coordinates per frame, making the
texture visually rotate.

### E7: Working Copy Fix (Applied but Uncommitted)

File: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`
Line 125 (current working copy):

```cpp
is_sphere[i] = false;  // always include rotation (needed for textured spheres)
```

This overrides the dynamic_cast check from commit `9538da9`, forcing all dynamic
bodies (including spheres) to use `from_translation_rotation()`, which includes
the rotation quaternion in the transform matrix.

---

## Toyota 5 Whys Analysis

### Branch A: Visual Rotation

**WHY 1 (Symptom)**: Why do textured spheres not visually rotate?
- Because the GPU shader computes the same UV coordinates for a given viewing
  direction regardless of the sphere's physical rotation.
- **Evidence**: Object-space normal (used for UV) is computed from the
  inverse-transformed ray (ray_trace.metal lines 668-690). If the inverse
  transform lacks rotation, the object-space normal is view-dependent only,
  not orientation-dependent.

**WHY 2 (Context)**: Why are the UV coordinates not changing with physical rotation?
- Because the inverse transform matrix sent to the GPU contains NO rotation
  component -- only translation.
- **Evidence**: `build_transform(t, true)` at animation_renderer.cpp line 215
  calls `Matrix4x4::from_translation(t.position)` (matrix4x4.h line 33-35),
  which produces an identity rotation with translation only. The rotation
  quaternion from `PhysicsTransform` is ignored.

**WHY 3 (System)**: Why does the animation renderer strip rotation from sphere transforms?
- Because commit `9538da9` introduced an optimization that detects sphere shapes
  and uses translation-only transforms, controlled by the `is_sphere` flag.
- **Evidence**: Git diff of commit `9538da9` shows:
  ```cpp
  is_sphere[i] = dynamic_cast<const Sphere*>(original_shapes[i].get()) != nullptr;
  ```
  This flag is passed to `build_transform()` as the `translation_only` parameter.

**WHY 4 (Design)**: Why was rotation stripped from sphere transforms?
- Because the optimization assumed spheres are rotationally symmetric and rotation
  is "physically meaningless" for their visual appearance.
- **Evidence**: Commit message of `9538da9`:
  > "A perfect sphere is rotationally symmetric, so applying physics rotation
  > to it is wasteful and physically meaningless for reflections."

**WHY 5 (Root Cause)**: Why did this assumption fail?
- Because the system now has TEXTURED spheres where visual appearance IS
  orientation-dependent. The optimization was introduced when all spheres had
  solid-color or procedural materials that did not use orientation-dependent
  UV mapping. When textured spheres were added (with equirectangular, cube-map,
  and dual-hemisphere texture sampling that all derive UVs from the object-space
  normal), the optimization became a correctness bug.
- **Evidence**: All 20 textured spheres use procedural_texture materials
  (nwave_bowling.yaml lines 74-93) whose patterns are sampled via object-space
  normals in the shader (ray_trace.metal lines 971-1019).

**ROOT CAUSE A**: Missing regression guard. The rotation-stripping optimization
in commit `9538da9` was applied unconditionally to all spheres without checking
whether the sphere has an orientation-dependent material (texture). No test
existed to verify that textured sphere rotation produces visually different
frames.

### Branch B: Late Detection

**WHY 1**: Why was this not caught before the textured spheres feature?
- Because the rotation-stripping commit (`9538da9`) was introduced on the same
  day as the textured sphere feature work, and no integration test verified
  visual rotation of textured spheres.

**WHY 2**: Why was there no integration test?
- Because the existing test suite for `AnimationRenderer` uses mock physics and
  verifies transform calls, not pixel-level visual output. The tests verified
  that `get_transform()` was called but not that the rotation component was
  included in the resulting matrix.

**WHY 3**: Why don't the unit tests catch rotation being stripped?
- Because the test added alongside commit `9538da9` likely VALIDATED the
  rotation-stripping behavior as correct, rather than testing that textured
  spheres need rotation.

**WHY 4**: Why was rotation-stripping considered correct?
- Same as Branch A WHY 4: the assumption that all spheres are visually
  rotationally symmetric.

**WHY 5 (Root Cause)**: The optimization was introduced without considering future
or concurrent features (textured spheres) that would break the assumption.

**ROOT CAUSE B**: The rotation-stripping optimization was introduced without a
feature flag or material-type check that would make it conditional on the sphere
actually being rotationally symmetric (i.e., having a solid-color material).

---

## Backwards Chain Validation

### Chain A: Rotation stripping causes static texture
1. Commit `9538da9` sets `is_sphere[i] = true` for Sphere shapes
2. `build_transform(t, true)` calls `from_translation(t.position)` -- rotation discarded
3. `TransformedShape::set_transform()` receives translation-only matrix
4. Scene flattener stores translation-only inverse transform in GPUShape
5. GPU shader inverse-transforms ray using translation-only matrix
6. `intersect_sphere()` computes object-space normal at the same local position for same view direction
7. `sphere_uv()` computes same UV coordinates every frame
8. Texture sampling returns same color every frame
9. **Sphere appears to not rotate** -- MATCHES OBSERVED SYMPTOM

### Chain B: Fix validation
1. Working copy changes `is_sphere[i] = false`
2. `build_transform(t, false)` calls `from_translation_rotation(t.position, t.rotation)`
3. Rotation quaternion from physics IS included in the transform
4. Inverse transform in GPU now includes rotation
5. Inverse-transformed ray hits different local-space point as rotation evolves
6. Object-space normal changes per frame
7. `sphere_uv()` computes different UV coordinates per frame
8. Texture sampling returns different colors
9. **Sphere appears to rotate** -- EXPECTED CORRECT BEHAVIOR

---

## Solution

### Immediate Mitigation (Already Applied in Working Copy)

File: `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`
Line 125

**Before** (committed in `9538da9`):
```cpp
is_sphere[i] = dynamic_cast<const Sphere*>(original_shapes[i].get()) != nullptr;
```

**After** (current working copy):
```cpp
is_sphere[i] = false;  // always include rotation (needed for textured spheres)
```

This forces all dynamic bodies to use the full rotation+translation transform,
ensuring textured spheres rotate visually.

### Permanent Fix (Recommended)

Remove the `is_sphere` vector and `translation_only` parameter entirely. The
performance cost of including rotation in sphere transforms is negligible (one
quaternion-to-matrix conversion per sphere per frame), and the correctness risk
of conditionally stripping it is high. The `build_transform` function should
always produce a full rotation+translation matrix.

**Proposed change** to `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp`:

1. Remove `std::vector<bool> is_sphere(shape_count, false);` (line 113)
2. Remove assignment `is_sphere[i] = false;` (line 125)
3. Simplify `build_transform` to always include rotation:

```cpp
Matrix4x4 build_transform(const PhysicsTransform& t) {
    return Matrix4x4::from_translation_rotation(t.position, t.rotation);
}
```

4. Update call sites to remove the `is_sphere` argument:

Line 136:
```cpp
Matrix4x4 init_mat = build_transform(init_t);
```

Line 215:
```cpp
Matrix4x4 current = build_transform(phys_transform);
```

### Early Detection: Recommended Test

Add an integration test that verifies textured sphere transforms include rotation:

```cpp
TEST(AnimationRendererRotationTest, TexturedSphereTransformIncludesRotation) {
    // Setup: sphere with textured material and angular velocity
    // Run: render 2+ frames after wake_frame
    // Assert: the TransformedShape's transform matrix has non-identity
    //         rotation components (upper-left 3x3 is not identity)
}
```

---

## Summary of Root Causes

| ID | Root Cause | Type | Fix |
|----|-----------|------|-----|
| A | Commit `9538da9` unconditionally strips rotation from sphere transforms, assuming all spheres are rotationally symmetric. This assumption is invalid for textured spheres. | Design flaw | Remove `is_sphere` / `translation_only` logic entirely; always include rotation |
| B | No test verifies that textured sphere rotation produces visually changing transforms. The test suite validates translation but not rotation of sphere transforms. | Missing test | Add integration test for textured sphere rotation |

---

## Key Code References

| File | Line(s) | Role |
|------|---------|------|
| `src/application/animation_renderer.cpp` | 78-83 | `build_transform()` -- the function that conditionally strips rotation |
| `src/application/animation_renderer.cpp` | 125 | `is_sphere[i]` assignment -- the flag controlling rotation stripping |
| `src/application/animation_renderer.cpp` | 135-136 | Initial transform capture -- uses `is_sphere` |
| `src/application/animation_renderer.cpp` | 214-215 | Per-frame transform update -- uses `is_sphere` |
| `src/application/animation_renderer.cpp` | 291-305 | Per-body wake with angular velocity setting |
| `src/core/matrix4x4.h` | 33-35 | `from_translation()` -- translation-only matrix (no rotation) |
| `src/core/matrix4x4.h` | 37-54 | `from_translation_rotation()` -- full transform with rotation |
| `src/infrastructure/jolt_physics_simulator.cpp` | 262-285 | `get_transform()` returns position + rotation quaternion |
| `src/infrastructure/gpu/scene_flattener.cpp` | 262-271 | Stores inverse transform in GPUShape for shader |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 162-167 | `sphere_uv()` -- computes UV from object-space normal |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 668-694 | Object-space normal saved for UV before world-space transform |

All file paths are relative to `/Users/andrealaforgia/dev/personal/nwave-raytracer/`.
