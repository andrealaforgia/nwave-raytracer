# Root Cause Analysis: Textured Spheres Not Rolling Visually

**Date**: 2026-02-22
**Investigator**: Rex (Root Cause Analysis Specialist)
**Methodology**: Toyota 5 Whys, multi-causal, evidence-required

---

## Problem Statement

Textured procedural spheres in the bowling animation slide across the marble floor surface
without visually rotating. The spheres translate correctly (they move from left to right as
expected), but the texture pattern on each sphere remains fixed relative to the sphere,
producing a "sliding" or "skating" appearance rather than a rolling one.

**Scope**: 20 procedural-textured spheres defined in `scenes/nwave_bowling.yaml`
(sphere_camouflage through sphere_zebra), frames 160-390 of the animation.

**Expected**: Spheres should roll, with their surface textures rotating to match the
translational motion across the floor.

**Observed**: Spheres glide without any visible texture rotation.

---

## Investigation Summary

The investigation identified **two independent root causes** (Branch A and Branch B) that
both contribute to the symptom. Either root cause alone would be sufficient to produce the
observed behavior. Both must be addressed for rolling to appear visually correct.

---

## Branch A: Physics Simulation Produces Zero Rotation

### WHY 1 (Symptom): The sphere textures do not rotate visually during the animation.

**Evidence**: Visual inspection of rendered frames shows the texture pattern on each sphere
remains in a fixed orientation throughout the animation, even as the sphere translates
horizontally across the floor.

### WHY 2 (Context): The physics engine returns identity (zero) rotation for each sphere.

**Evidence**: The Jolt physics simulator (`src/infrastructure/jolt_physics_simulator.cpp`,
line 271) calls `body_interface.GetRotation(jolt_id)` to retrieve the body's quaternion
rotation. For a sphere with zero torque applied, Jolt returns the identity quaternion
`(0, 0, 0, 1)`, meaning no rotation. The animation renderer (`src/application/animation_renderer.cpp`,
line 214) calls `physics_->get_transform(body_ids[i])` each frame and uses both position and
rotation to build the transform matrix (line 215, `build_transform` with `is_sphere[i]` which
is `false`, meaning rotation IS included). However, when the physics quaternion is always
identity, `Matrix4x4::from_translation_rotation(position, identity_quat)` produces a
translation-only matrix, so no visual rotation occurs.

### WHY 3 (System): Zero torque is applied to the spheres during simulation because the combined contact friction is zero.

**Evidence**: All 20 textured spheres are configured with `friction: 0.0` in the scene YAML
(`scenes/nwave_bowling.yaml`, lines 220-334). Example:

```yaml
physics: { body_type: dynamic, mass: 2.0, friction: 0.0, restitution: 0.3,
           initial_velocity: [5.0, 0.0, 0.0], start_asleep: true, wake_frame: 160 }
```

In Jolt Physics, the combined friction between two contacting bodies is computed as
`sqrt(friction_A * friction_B)`. The marble floor has default friction of 0.5
(`src/domain/physics_properties.h`, line 23: `double friction{0.5}`), but the sphere friction
is 0.0. Combined: `sqrt(0.0 * 0.5) = 0.0`. With zero contact friction, Jolt generates no
frictional force and therefore no torque on the spheres, so they never begin rotating.

### WHY 4 (Design): No initial angular velocity is provided to compensate for the lack of friction-driven rotation.

**Evidence**: The sphere YAML definitions do not include any `initial_angular_velocity`
property (confirmed by grep -- no `angular_velocity` matches in the scene file). The
`PhysicsProperties` struct defaults angular velocity to `(0, 0, 0)`
(`src/domain/physics_properties.h`, line 22). The wake handler in `animation_renderer.cpp`
(lines 292-305) does check for and apply `initial_angular_velocity`, but the value is always
`(0, 0, 0)` for these spheres.

### WHY 5 (Root Cause): The friction was intentionally set to 0.0 to achieve frictionless sliding motion (for aesthetic or physics reasons), without recognizing that this eliminates the physical mechanism responsible for rolling rotation, and no compensating angular velocity was provided.

**Evidence**: The scene YAML comment on line 214 states: "Rolling Procedural Spheres (20
total, single-file across the board)". The word "Rolling" indicates the design intent was for
the spheres to roll. The friction of 0.0 contradicts this intent. There is no documentation
explaining why friction was set to zero, suggesting it was either a default copy-paste error
from another body definition (the bowling ball at line 111 also has `friction: 0.0`) or
a misunderstanding that "rolling" would occur automatically with linear velocity alone.

**ROOT CAUSE A**: Sphere friction is set to 0.0 in the scene configuration, preventing the
physics engine from generating rotational torque through floor contact. No initial angular
velocity is provided as a workaround.

---

## Branch B: GPU Texture UV Not Incorporating Rotation Transform (Latent)

> **Note**: This branch describes a latent issue that would become visible IF Branch A is
> fixed (i.e., if the physics engine starts producing rotation). Currently this path works
> correctly in principle. Analysis is provided for completeness of the multi-causal
> investigation.

### WHY 1 (Symptom): Same as Branch A -- textures do not rotate visually.

### WHY 2 (Context): The GPU shader computes sphere UVs from `object_space_normal`, which DOES correctly account for the transform.

**Evidence**: In `ray_trace.metal` (lines 628-671), the intersection logic:

1. Transforms the ray into object space using the inverse transform (line 630):
   `test_ray = transform_ray(ray, shape.inverse_transform);`
2. Performs sphere intersection in object space (line 634):
   `intersect_sphere(test_ray, params, ...)` produces `outward_normal` in object space
3. Saves the object-space normal (line 651):
   `float3 object_space_normal = outward_normal;`
4. Computes UV from object-space normal (line 671):
   `sphere_uv(object_space_normal, rec.u, rec.v);`

When a rotation transform is present, the ray is transformed by the inverse rotation into the
sphere's local frame. The resulting `outward_normal` is relative to the sphere's local (body)
axes. As the sphere rotates frame-to-frame, a fixed world-space hit point maps to different
local-frame normals, producing different UVs. This means the texture correctly rotates with
the sphere transform.

**Verification**: The scene flattener (`src/infrastructure/gpu/scene_flattener.cpp`, lines
262-271) passes the inverse transform matrix to the GPU when `has_transform` is set. The
animation renderer sets `is_sphere[i] = false` (line 125), meaning `build_transform` uses
`Matrix4x4::from_translation_rotation` (line 82), which includes rotation. The relative
delta matrix (`current * initial_inv[i]`, line 216) correctly computes the frame-to-frame
change including rotation.

### WHY 3-5: N/A -- the GPU rendering pipeline correctly handles rotation transforms for textured spheres.

**CONCLUSION ON BRANCH B**: The GPU rendering pipeline is correctly wired to display texture
rotation when the physics transform contains a non-identity rotation quaternion. This is NOT
a contributing root cause -- it is verified as functioning correctly. The sole active root
cause is Branch A (zero physics friction producing zero rotation).

---

## Backwards Chain Validation

### Branch A validation (forward trace):

1. Sphere friction is 0.0 in YAML ->
2. Jolt computes combined friction with floor as sqrt(0.0 * 0.5) = 0.0 ->
3. Zero friction produces zero tangential contact force ->
4. Zero tangential force produces zero torque ->
5. Zero torque means angular velocity stays at initial value (0,0,0) ->
6. `GetRotation()` returns identity quaternion every frame ->
7. `build_transform` produces translation-only matrix (rotation component is identity) ->
8. GPU inverse transform has no rotational component ->
9. Object-space normal for same world hit point is same every frame ->
10. `sphere_uv` produces same UV every frame ->
11. Texture appears static -> Observed symptom confirmed.

### Branch B validation (forward trace, hypothetical if rotation were present):

1. Physics produces non-identity rotation quaternion ->
2. `build_transform` includes rotation in matrix ->
3. Scene flattener passes inverse (with rotation) to GPU ->
4. `transform_ray` rotates ray into sphere's local frame ->
5. `intersect_sphere` computes normal in local frame ->
6. `sphere_uv` on local normal produces rotation-dependent UV ->
7. Texture visually rotates -> This path is correct.

---

## Solutions

### Immediate Mitigation (restore visual rolling)

**Option M1: Add initial_angular_velocity to scene YAML**

For a sphere of radius `r` translating at velocity `v` along the X-axis, the rolling
condition is `omega = v / r` around the Z-axis (using right-hand rule, the sphere must rotate
clockwise when viewed from the positive Z direction, so angular velocity is negative Z).

For these spheres: `r = 0.2`, `v = 5.0 m/s`, so `omega = 5.0 / 0.2 = 25.0 rad/s`.

The rotation axis for rolling along +X on a horizontal floor is the **negative Z-axis** (the
sphere's contact point moves backward relative to the rolling direction).

```yaml
physics: { body_type: dynamic, mass: 2.0, friction: 0.0, restitution: 0.3,
           initial_velocity: [5.0, 0.0, 0.0],
           initial_angular_velocity: [0.0, 0.0, -25.0],
           start_asleep: true, wake_frame: 160 }
```

This produces visually correct rolling without changing the sliding physics behavior.

**Effort**: Low (YAML-only change, 20 lines).
**Risk**: Low. Does not affect collision or trajectory physics.

### Permanent Fix (address root cause)

**Option P1: Set sphere friction to a physically realistic value**

Change all 20 sphere friction values from `0.0` to a reasonable value such as `0.3`--`0.5`.
This allows Jolt to compute contact friction with the floor, generating torque that produces
natural rolling rotation. The angular velocity will be physically consistent with the
translational velocity.

```yaml
physics: { body_type: dynamic, mass: 2.0, friction: 0.3, restitution: 0.3,
           initial_velocity: [5.0, 0.0, 0.0],
           start_asleep: true, wake_frame: 160 }
```

**Effort**: Low (YAML-only change, 20 lines).
**Risk**: Medium. Adding friction changes the sphere trajectories -- they will decelerate
due to friction, and the timing/spacing of the sphere procession may change. Visual review
of the full animation is required after this change.

**Option P2: Combine friction with initial angular velocity**

Set friction to a moderate value (e.g., `0.3`) AND provide an initial angular velocity
matching the rolling condition. This ensures the sphere begins rolling immediately at
wake_frame without a spin-up period.

```yaml
physics: { body_type: dynamic, mass: 2.0, friction: 0.3, restitution: 0.3,
           initial_velocity: [5.0, 0.0, 0.0],
           initial_angular_velocity: [0.0, 0.0, -25.0],
           start_asleep: true, wake_frame: 160 }
```

**Effort**: Low (YAML-only change, 20 lines).
**Risk**: Low-Medium. Most physically accurate option. The friction will eventually
decelerate both linear and angular velocity together, maintaining the rolling condition.

### Early Detection

**Option D1: Add visual regression test for sphere rotation**

Create a test that renders 3-4 frames of a textured sphere with known physics and verifies
the UV mapping changes between frames. This catches future regressions where rotation stops
being applied.

**Effort**: Medium (requires a new integration test with frame comparison).

---

## Recommended Action

**Recommended**: Option P2 (friction + initial angular velocity) as the permanent fix.
This is the most physically correct approach: the spheres begin rolling immediately and
friction maintains the rolling condition throughout their traversal.

If the change in trajectory from adding friction is unacceptable for the animation timing,
fall back to Option M1 (angular velocity only, friction stays at 0.0), which provides visual
rolling without altering the translational physics.

---

## Files Investigated

| File | Path | Relevance |
|------|------|-----------|
| animation_renderer.cpp | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/animation_renderer.cpp` | Physics-to-visual transform pipeline |
| physics_simulator.h | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/application/physics_simulator.h` | PhysicsTransform struct (position + rotation) |
| jolt_physics_simulator.cpp | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/jolt_physics_simulator.cpp` | Jolt integration, friction application |
| nwave_bowling.yaml | `/Users/andrealaforgia/dev/personal/nwave-raytracer/scenes/nwave_bowling.yaml` | Scene config with friction: 0.0 |
| transformed_shape.cpp | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/shapes/transformed_shape.cpp` | Inverse-transform ray intersection |
| matrix4x4.h | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/core/matrix4x4.h` | from_translation_rotation builder |
| quaternion.h | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/core/quaternion.h` | Quaternion to rotation matrix |
| ray_trace.metal | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/metal/shaders/ray_trace.metal` | GPU sphere UV from object-space normal |
| scene_flattener.cpp | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/infrastructure/gpu/scene_flattener.cpp` | Transform pass-through to GPU |
| physics_properties.h | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/physics_properties.h` | Default friction = 0.5 |
| sphere.cpp | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/domain/shapes/sphere.cpp` | CPU sphere hit (no UV) |
| gpu_types.h | `/Users/andrealaforgia/dev/personal/nwave-raytracer/src/core/gpu_types.h` | GPUShape struct with inverse_transform |
