# Requirements: Soft Body Jelly Physics

**Feature**: Deformable jelly cube with physics-driven animation in the nwave ray tracer
**Date**: 2026-02-18
**Status**: Draft - Pending DESIGN wave handoff

---

## 1. Problem Statement

Marco Benedetti, a technical artist, wants to create a physics-animated scene where a translucent pink jelly cube drops from above, hits a solid 'e' letter shape, knocks it over, and bounces back with visible wobbly deformation. Today, the nwave ray tracer supports only rigid body physics -- objects move and collide but never change shape. Marco cannot create deformable, jelly-like objects because the physics engine integration only exposes rigid body transforms (position + rotation), and the rendering pipeline has no concept of a shape whose vertices change each frame. Marco must resort to external tools or abandon the creative idea entirely.

## 2. Stakeholders

| Stakeholder | Role | Interest |
|---|---|---|
| Technical artist (e.g., Marco Benedetti) | Scene author | Wants to define soft body objects in YAML and render physics-animated deformation |
| Developer extending nwave | Maintainer | Needs clean interfaces that follow existing Clean Architecture patterns |
| End viewer | Consumer of rendered video | Expects visually convincing jelly behavior: wobble, bounce, volume preservation |

## 3. Scope

### In Scope

- Soft body physics simulation using Jolt XPBD (already integrated at v5.2.0)
- DeformableMesh shape for ray tracing geometry that changes each frame
- Font glyph to 3D mesh generation for the 'e' letter (and other characters)
- YAML scene format extensions for soft body cube objects and letter shapes
- Scene validation for soft body configurations
- AnimationRenderer integration to update deformable meshes each frame
- Demo scene: pink jelly cube + rigid 'e' letter on a floor

### Out of Scope

- Soft-to-soft body collision (not supported by Jolt)
- GPU Metal rendering of deformable meshes (CPU rendering path only for soft body scenes in v1)
- Subsurface scattering material model (use existing dielectric material with tint)
- Arbitrary soft body shapes (only soft body cube primitive in v1)
- User-defined constraint topologies (auto-generated from grid)

## 4. Functional Requirements

### FR-1: Soft Body Physics API

The PhysicsSimulator interface (Application ring) must be extended so that callers can create soft body objects and retrieve their deformed mesh data each frame.

**FR-1.1**: A new domain type must describe soft body creation parameters: grid resolution, grid spacing, initial position, pressure, restitution, damping, edge compliance, volume compliance, and solver iteration count.

**FR-1.2**: The PhysicsSimulator must accept a soft body description and return a body identifier, following the same pattern as the existing `add_body()` method.

**FR-1.3**: The PhysicsSimulator must expose a method to retrieve the deformed mesh of a soft body at the current simulation time. The mesh data includes world-space vertex positions and face indices (constant topology).

**FR-1.4**: The PhysicsSimulator must expose a method to query whether a given body identifier refers to a soft body or a rigid body.

**FR-1.5**: The existing `step()` method must advance both rigid and soft bodies together in the same simulation tick, preserving bidirectional collision response (soft body deforms on rigid body contact; dynamic rigid body receives impulse from soft body contact).

### FR-2: Jolt Soft Body Implementation

The JoltPhysicsSimulator (Infrastructure ring) must implement the soft body API using Jolt's XPBD soft body system.

**FR-2.1**: Soft body creation must generate a 3D vertex grid (N x N x N) with edge constraints along all three axes, tetrahedral volume constraints (6 tetrahedra per grid cell), and surface faces on the outer shell of the cube.

**FR-2.2**: The implementation must apply the configured pressure, restitution, damping, edge compliance, volume compliance, and solver iterations to the Jolt `SoftBodyCreationSettings`.

**FR-2.3**: Mesh extraction must transform vertex positions from soft body local space to world space using the body's center-of-mass transform before returning them.

**FR-2.4**: Soft bodies and rigid bodies must be on collision layers that allow mutual interaction. A soft body cube must collide with static planes, dynamic rigid bodies, and static rigid bodies.

### FR-3: DeformableMesh Shape

A new shape class must allow the ray tracer to intersect rays with geometry whose vertices change each frame.

**FR-3.1**: DeformableMesh must implement the Shape interface (`hit()` method) using Moller-Trumbore ray-triangle intersection, consistent with the existing TriangleMesh.

**FR-3.2**: DeformableMesh must accept updated vertex positions each frame while keeping face connectivity (indices) constant.

**FR-3.3**: When vertices are updated, DeformableMesh must recompute area-weighted smooth normals by averaging the face normals of all faces adjacent to each vertex.

**FR-3.4**: When vertices are updated, DeformableMesh must recompute its axis-aligned bounding box from the new vertex positions.

**FR-3.5**: DeformableMesh must store a material pointer and apply it during intersection, following the same pattern as TriangleMesh.

### FR-4: Font-to-3D-Mesh Generation

The system must generate a 3D triangle mesh from a font glyph character for use as both a renderable shape and a physics collision body.

**FR-4.1**: Given a character (e.g., 'e'), a target height in world units, and an extrusion depth, the system must produce a closed 3D triangle mesh with front face, back face, and side walls.

**FR-4.2**: The generated mesh must correctly handle glyphs with interior contours (counters), such as the holes in 'e', 'a', 'o', 'd'.

**FR-4.3**: The generated mesh must be suitable for rendering as a TriangleMesh shape with smooth normals on curved surfaces and flat normals on planar faces.

**FR-4.4**: For physics collision, the concave letter mesh must be decomposed into a set of approximate convex hulls so it can be used as a dynamic rigid body in Jolt (since Jolt's MeshShape is static-only).

### FR-5: YAML Scene Format Extensions

The YAML scene file format must support defining soft body objects and letter shapes alongside existing object types.

**FR-5.1**: A new object type `soft_body_cube` must be recognized, with properties: `center` (3D position), `size` (edge length), `grid_resolution` (integer, default 5), and `material` (reference).

**FR-5.2**: The `physics` block for a soft body must accept `body_type: soft` and the following properties: `pressure` (default 2000.0), `restitution` (default 0.3), `damping` (default 0.05), `edge_compliance` (default 0.0001), `volume_compliance` (default 0.0), `solver_iterations` (default 5).

**FR-5.3**: A new object type `letter` must be recognized, with properties: `character` (single character), `font` (string, "default" or path to .ttf file), `height` (world units), `depth` (extrusion depth), `center` (3D position), and `material` (reference).

**FR-5.4**: The `physics` block for a letter must accept the existing rigid body properties: `body_type` (dynamic/static/kinematic), `mass`, `friction`, `restitution`.

**FR-5.5**: The `BodyType` enum must be extended with a `SOFT` value to distinguish soft bodies from rigid bodies during scene construction and physics registration.

### FR-6: Scene Validation for Soft Bodies

The SceneValidator must validate soft body configurations before simulation begins.

**FR-6.1**: Validation must check that `grid_resolution` is between 2 and 15 (inclusive). Values outside this range must produce an error with guidance.

**FR-6.2**: Validation must check that `pressure` is non-negative and warn if it exceeds 10000 (risk of instability).

**FR-6.3**: Validation must check that `solver_iterations` is at least 1 and warn if below 3 (risk of floppy behavior).

**FR-6.4**: Validation must check that soft body objects have `body_type: soft` and that non-soft-body objects do not use `body_type: soft`.

**FR-6.5**: Validation must check that `edge_compliance` and `volume_compliance` are non-negative.

**FR-6.6**: Validation must verify that letter objects reference a valid font (either "default" or an existing .ttf file path).

### FR-7: AnimationRenderer Soft Body Integration

The AnimationRenderer must update deformable meshes each frame from physics simulation data.

**FR-7.1**: During scene construction, soft body objects must be registered with the PhysicsSimulator and their corresponding DeformableMesh shapes must be created and added to the Scene.

**FR-7.2**: Each frame, after the physics step completes, the AnimationRenderer must extract the deformed mesh from the PhysicsSimulator for each soft body and call the DeformableMesh update method with the new vertex positions.

**FR-7.3**: Rigid body transform updates (existing behavior) and soft body mesh updates must both occur before frame rendering begins. The existing sequential pipeline (physics step -> update scene -> render frame) must be preserved.

**FR-7.4**: The AnimationRenderer must track which shapes are soft bodies and which are rigid bodies so it applies the correct update strategy (transform update vs. vertex update) per object.

### FR-8: Demo Scene

A complete demo scene YAML file must demonstrate the jelly cube + 'e' letter interaction.

**FR-8.1**: The scene must contain: a static floor plane, a pink translucent jelly cube positioned above the 'e' letter, and a stone-colored rigid dynamic 'e' letter standing on the floor.

**FR-8.2**: When animated, the jelly cube must fall under gravity, deform on impact with the 'e' letter, transfer impulse to knock the 'e' over, bounce back with visible wobble, and eventually settle.

**FR-8.3**: The scene must use existing material types (dielectric for jelly, lambertian for letter) without requiring new material implementations.

## 5. Non-Functional Requirements

### NFR-1: Architectural Consistency

All new code must follow the existing Clean Architecture ring structure:
- Domain ring: SoftBodyDesc, SoftBodyMeshData, DeformableMesh, BodyType::SOFT
- Application ring: PhysicsSimulator interface extensions
- Infrastructure ring: JoltPhysicsSimulator implementation, YAML parsing extensions, font mesh generation

### NFR-2: Performance

- Physics simulation of a 5x5x5 soft body cube must complete within 50ms per physics step on the development machine (offline renderer, not real-time).
- DeformableMesh vertex update and normal recomputation for 125 vertices and 192 faces must complete within 1ms.
- Ray-triangle intersection against DeformableMesh must use the same linear scan approach as the existing TriangleMesh (no BVH required for v1).

### NFR-3: Thread Safety

The AnimationRenderer uses a sequential pipeline (physics step completes before render reads mesh data). This sequential ordering must be maintained for soft body mesh extraction. No concurrent access to DeformableMesh vertex data during rendering.

### NFR-4: Backward Compatibility

- Existing YAML scene files without soft body objects must continue to parse and render without any changes.
- The PhysicsSimulator interface extension must not break existing JoltPhysicsSimulator rigid body behavior.
- Existing CLI flags and output format must remain unchanged. No new CLI flags required.

### NFR-5: Error Handling

- If soft body simulation produces NaN vertex positions (divergence), the system must detect this and halt with a clear error message rather than producing corrupt frame output.
- If font file is not found or glyph is unsupported, the system must fail at scene load time with a descriptive error, not during rendering.

## 6. Constraints

| Constraint | Source | Impact |
|---|---|---|
| Jolt v5.2.0 soft body API | Existing dependency | Soft body implementation must use Jolt's XPBD system, not a custom solver |
| No soft-to-soft collision | Jolt limitation | Only one object in the scene can be a soft body if mutual collision is needed; the 'e' must be rigid |
| MeshShape is static-only in Jolt | Jolt limitation | The 'e' letter must use convex decomposition for dynamic physics body |
| CPU rendering only for soft bodies | GPU pipeline complexity | Metal GPU backend does not support per-frame vertex buffer updates in v1 |
| C++17 standard | Project constraint | All new code must compile with C++17 |

## 7. Dependencies

| Dependency | Status | Risk |
|---|---|---|
| Jolt Physics v5.2.0 soft body headers | Available (already integrated) | Low |
| ttf2mesh or FreeType library | New dependency, not yet integrated | Medium -- needs CMake integration and glyph counter handling verification |
| V-HACD or CoACD for convex decomposition | New dependency, not yet integrated | Medium -- needed for dynamic letter physics body |
| Existing TriangleMesh implementation | Available | Low -- DeformableMesh follows same intersection pattern |
| Existing AnimationRenderer pipeline | Available | Low -- sequential pipeline preserved |

## 8. Acceptance Summary

This feature is accepted when:

1. A YAML scene with `type: soft_body_cube` and `type: letter` objects loads, validates, and renders a physics-animated frame sequence.
2. The jelly cube visibly deforms on impact (vertices move), preserves volume (within 5% of initial), bounces back, and wobbles before settling.
3. The rigid 'e' letter receives impulse from the jelly cube and topples.
4. Smooth normals on the deforming jelly surface produce artifact-free rendering (no hard edges, no dark patches from flipped normals).
5. Output frames follow existing frame_NNNN.ppm naming convention and the CLI prints the ffmpeg assembly command.
6. Existing scenes (e.g., nwave_bowling.yaml) continue to work without modification.
