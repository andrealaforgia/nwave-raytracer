# User Stories: Soft Body Jelly Physics

**Feature**: Deformable jelly cube with physics-driven animation
**Date**: 2026-02-18
**Slicing technique**: Elephant Carpaccio -- each story is independently testable and delivers incremental value

---

## Story Map Overview

```
US-01  Soft Body Domain Types
  |
US-02  PhysicsSimulator Soft Body API
  |
US-03  JoltPhysicsSimulator Soft Body Creation
  |
US-04  JoltPhysicsSimulator Soft Body Mesh Extraction
  |
US-05  DeformableMesh Shape (Ray Intersection)
  |
US-06  DeformableMesh Vertex Update + Normal Recomputation
  |
US-07  AnimationRenderer Soft Body Loop
  |
US-08  YAML Parsing: soft_body_cube
  |
US-09  YAML Parsing: letter Object Type
  |
US-10  Font Glyph to 3D Mesh Generation
  |
US-11  Letter Convex Decomposition for Physics
  |
US-12  Scene Validation: Soft Body Parameters
  |
US-13  Demo Scene: Jelly Cube + Letter 'e'
```

### Dependency Graph

```
US-01 --> US-02 --> US-03 --> US-04
                                |
US-05 --> US-06 ----------------+--> US-07 --> US-08 --> US-13
                                                |
US-10 --> US-11 --> US-09 ----------------------+
                      |
                    US-12
```

### Priority Order (Implementation Sequence)

| Priority | Story | Rationale |
|---|---|---|
| P0 | US-01 | Foundation: domain types needed by all other stories |
| P0 | US-02 | Foundation: interface contract needed before implementation |
| P0 | US-05 | Foundation: rendering shape needed independently of physics |
| P1 | US-03 | Core physics: creates soft body in Jolt |
| P1 | US-04 | Core physics: extracts deformed mesh from Jolt |
| P1 | US-06 | Core rendering: vertex update enables per-frame deformation |
| P2 | US-07 | Integration: connects physics to rendering per frame |
| P2 | US-08 | Scene authoring: users can define soft bodies in YAML |
| P2 | US-10 | Letter mesh: generates 'e' shape for rendering |
| P3 | US-11 | Letter physics: convex decomposition for dynamic collision |
| P3 | US-09 | Scene authoring: users can define letters in YAML |
| P3 | US-12 | Quality: validates parameters before long renders |
| P4 | US-13 | Demo: end-to-end scene proving all capabilities work together |

---

## US-01: Soft Body Domain Types

### Problem (The Pain)

Marco Benedetti, a technical artist building physics-animated scenes, has no way to describe a soft body object in the nwave domain model. The existing `PhysicsProperties` type only captures rigid body attributes (mass, friction, restitution). The `BodyType` enum has `STATIC`, `DYNAMIC`, and `KINEMATIC` but no `SOFT`. When Marco imagines a jelly cube, there is no vocabulary in the codebase to express its softness, internal pressure, or vertex-level deformation data.

### Who (The User)

- Developer extending the nwave physics pipeline
- Needs domain types as the shared language between physics simulation and rendering
- Motivated by Clean Architecture: domain types belong in the Domain ring, independent of Jolt or YAML

### Solution (What We Build)

Add domain types that describe soft body creation parameters and soft body mesh data, plus extend the BodyType enum with a SOFT value.

### Domain Examples

#### Example 1: Marco's Jelly Cube Description
Marco wants a 5x5x5 jelly cube at position (0, 4, 0) with pressure 2000, edge compliance 0.0001, and zero volume compliance. The SoftBodyDesc captures all of this in domain language without referencing Jolt.

#### Example 2: Extracting Deformed Mesh After Collision
After the jelly cube hits the floor at frame 15, the physics engine reports 125 vertices at new positions and 192 triangular faces. The SoftBodyMeshData holds these world-space positions and constant face indices.

#### Example 3: Distinguishing Soft from Rigid Bodies
The AnimationRenderer iterates scene objects. For the floor (BodyType::STATIC) and the 'e' letter (BodyType::DYNAMIC), it reads rigid transforms. For the jelly cube (BodyType::SOFT), it reads deformed mesh data. The BodyType enum makes this distinction explicit.

### UAT Scenarios (BDD)

```gherkin
Scenario: SoftBodyDesc holds jelly cube parameters
  Given Marco defines a soft body with grid_resolution 5, pressure 2000.0,
        edge_compliance 0.0001, volume_compliance 0.0, damping 0.05,
        restitution 0.3, solver_iterations 5, position (0, 4, 0), and size 1.5
  When the SoftBodyDesc is constructed with these values
  Then all fields are accessible and match the provided values
  And grid_resolution defaults to 5 when not specified
  And pressure defaults to 2000.0 when not specified

Scenario: SoftBodyMeshData holds deformed mesh
  Given a soft body with 125 vertices and 576 face indices (192 triangles)
  When the SoftBodyMeshData is populated with vertex positions and face indices
  Then vertices contains 125 world-space Point3 values
  And face_indices contains 576 integer indices (3 per triangle)

Scenario: BodyType enum includes SOFT
  Given the existing BodyType enum with STATIC, DYNAMIC, KINEMATIC
  When a SOFT value is added
  Then BodyType::SOFT is a distinct value from all existing values
  And existing code using STATIC, DYNAMIC, KINEMATIC compiles without changes
```

### Acceptance Criteria

- [ ] SoftBodyDesc struct exists in domain ring with fields: grid_resolution, grid_spacing, position, size, pressure, restitution, damping, edge_compliance, volume_compliance, solver_iterations
- [ ] SoftBodyMeshData struct exists in domain ring with fields: vertices (vector of Point3), face_indices (vector of int)
- [ ] BodyType enum extended with SOFT value
- [ ] Sensible defaults: grid_resolution=5, pressure=2000.0, restitution=0.3, damping=0.05, edge_compliance=0.0001, volume_compliance=0.0, solver_iterations=5
- [ ] All existing tests pass (backward compatibility)

### Technical Notes

- SoftBodyDesc belongs in domain ring (e.g., `domain/soft_body_desc.h`)
- SoftBodyMeshData belongs in domain ring (e.g., `domain/soft_body_mesh_data.h`)
- BodyType extension is in existing `domain/physics_properties.h`
- No Jolt dependencies in domain types

**Size**: S (< 1 day)
**Dependencies**: None

---

## US-02: PhysicsSimulator Soft Body API

### Problem (The Pain)

Marco's scene has a jelly cube, but the PhysicsSimulator interface only knows how to `add_body()` for rigid bodies and `get_transform()` for position/rotation. There is no way to create a soft body or retrieve its deformed vertex data. The AnimationRenderer cannot update a deformable mesh because the physics abstraction has no vocabulary for it.

### Who (The User)

- Developer implementing the animation pipeline
- Needs a clean interface contract (Application ring) before building the Jolt implementation
- Motivated by the Dependency Inversion Principle: application code depends on the interface, not on Jolt

### Solution (What We Build)

Extend the PhysicsSimulator abstract interface with methods to add a soft body, query whether a body is soft, and retrieve deformed mesh data for a soft body.

### Domain Examples

#### Example 1: Adding Marco's Jelly Cube
Marco's scene loader creates a SoftBodyDesc and calls `physics->add_soft_body(desc)`. The method returns body ID 3 (after floor=0, letter_e=1, ground=2). This ID is stored alongside the DeformableMesh shape for later mesh updates.

#### Example 2: Querying Body Type in the Render Loop
The AnimationRenderer checks `physics->is_soft_body(3)` and gets true. For body 1 (the letter), it gets false. This determines whether to call `get_transform()` or `get_soft_body_mesh()`.

#### Example 3: Extracting Mesh at Frame 15
At frame 15, the AnimationRenderer calls `physics->get_soft_body_mesh(3)`. The returned SoftBodyMeshData has 125 vertices at their deformed world-space positions and 576 face indices.

### UAT Scenarios (BDD)

```gherkin
Scenario: add_soft_body returns a valid body ID
  Given a PhysicsSimulator instance
  When Marco's jelly cube SoftBodyDesc is passed to add_soft_body()
  Then a non-negative integer body ID is returned
  And the body ID is distinct from any previously returned ID

Scenario: is_soft_body distinguishes body types
  Given a PhysicsSimulator with rigid body ID 0 and soft body ID 1
  When is_soft_body(0) is called
  Then it returns false
  When is_soft_body(1) is called
  Then it returns true

Scenario: get_soft_body_mesh returns current vertex positions
  Given a PhysicsSimulator with soft body ID 1
  And the physics has been stepped forward by 0.5 seconds
  When get_soft_body_mesh(1) is called
  Then the returned SoftBodyMeshData has the expected vertex count
  And vertex positions reflect the deformed state (not initial positions)
  And face_indices are constant (same as at creation)
```

### Acceptance Criteria

- [ ] PhysicsSimulator has pure virtual method `add_soft_body(const SoftBodyDesc&) -> int`
- [ ] PhysicsSimulator has pure virtual method `is_soft_body(int body_id) const -> bool`
- [ ] PhysicsSimulator has pure virtual method `get_soft_body_mesh(int body_id) const -> SoftBodyMeshData`
- [ ] Existing `add_body()`, `step()`, `get_transform()` signatures unchanged
- [ ] All existing tests compile and pass

### Technical Notes

- These are pure virtual methods added to `application/physics_simulator.h`
- JoltPhysicsSimulator must implement them (see US-03, US-04)
- Any test mocks of PhysicsSimulator must be updated

**Size**: S (< 1 day)
**Dependencies**: US-01

---

## US-03: JoltPhysicsSimulator Soft Body Creation

### Problem (The Pain)

The PhysicsSimulator interface now accepts soft body descriptions, but the JoltPhysicsSimulator only knows how to create rigid bodies via `BodyInterface::CreateAndAddBody()`. It has never created a `SoftBodyCreationSettings`, never generated a vertex grid with tetrahedral constraints, and has no code path for `CreateAndAddSoftBody()`. Marco's jelly cube cannot exist in the physics world.

### Who (The User)

- Developer implementing Jolt integration
- Needs to translate domain-level SoftBodyDesc into Jolt's XPBD soft body system
- Motivated by getting the first soft body simulation running

### Solution (What We Build)

Implement `add_soft_body()` in JoltPhysicsSimulator. Generate a 3D vertex grid, edge constraints, tetrahedral volume constraints, and surface faces following Jolt's SoftBodyCreator::CreateCube pattern. Configure pressure, damping, restitution, compliance, and solver iterations from the SoftBodyDesc.

### Domain Examples

#### Example 1: Creating Marco's 5x5x5 Jelly Cube
Marco's SoftBodyDesc specifies grid_resolution=5, size=1.5, position=(0,4,0), pressure=2000. The implementation creates 125 vertices in a 5x5x5 grid with spacing=0.3 (1.5/5), adds 300 edge constraints, 384 tetrahedral volume constraints, and 192 surface faces. The Jolt SoftBodyCreationSettings has pressure=2000, restitution=0.3, linearDamping=0.05.

#### Example 2: Creating a Stiffer Cube for Sofia's Scene
Sofia Torres, a VFX student, wants a firmer gelatin cube: grid_resolution=8, pressure=3000, edge_compliance=0.00001, solver_iterations=10. The implementation handles the higher resolution (512 vertices, 2058 volume constraints) and tighter compliance values.

#### Example 3: Minimal Soft Body for Testing
A test creates a 3x3x3 soft body at origin with default parameters. The resulting Jolt body has 27 vertices, 54 edge constraints, 48 volume constraints, and 48 surface faces.

### UAT Scenarios (BDD)

```gherkin
Scenario: 5x5x5 jelly cube is created in Jolt
  Given a SoftBodyDesc with grid_resolution 5, size 1.5, position (0, 4, 0),
        pressure 2000.0, restitution 0.3, damping 0.05,
        edge_compliance 0.0001, volume_compliance 0.0, solver_iterations 5
  When add_soft_body() is called on the JoltPhysicsSimulator
  Then a valid body ID is returned
  And is_soft_body() returns true for that ID
  And the Jolt physics system contains a soft body at approximately (0, 4, 0)

Scenario: Soft body responds to gravity
  Given a soft body cube created at position (0, 4, 0)
  When the physics is stepped forward by 30 steps at 1/60s each (0.5 seconds)
  Then the soft body's center of mass y-position is less than 4.0

Scenario: Soft body collides with static floor
  Given a soft body cube at (0, 2, 0) and a static rigid floor plane at y=0
  When the physics is stepped until the cube reaches the floor
  Then the soft body vertices deform (some vertices are displaced from their rest positions)
  And no vertex passes below y=0 (floor collision respected)

Scenario: Soft body interacts with dynamic rigid body
  Given a soft body cube at (0, 4, 0) and a dynamic rigid box at (0, 1, 0)
  When the soft body falls onto the rigid box
  Then the rigid box receives a downward impulse (its velocity changes)
  And the soft body deforms on contact

Scenario: Soft body parameters affect behavior
  Given two soft bodies with identical position and size
  And soft body A has pressure 500 and soft body B has pressure 3000
  When both are dropped onto a static floor from the same height
  Then soft body B rebounds higher than soft body A (higher pressure = more bounce)
```

### Acceptance Criteria

- [ ] JoltPhysicsSimulator::add_soft_body() creates a Jolt soft body with N^3 vertices, edge constraints, tetrahedral volume constraints, and surface faces
- [ ] Soft body falls under gravity when physics is stepped
- [ ] Soft body collides with static rigid bodies (floor plane)
- [ ] Soft body collides with dynamic rigid bodies (impulse transfer)
- [ ] Pressure, restitution, damping, compliance, solver_iterations from SoftBodyDesc are applied to Jolt creation settings
- [ ] Returned body ID works with is_soft_body() (returns true)

### Technical Notes

- Reference implementation: Jolt's `SoftBodyCreator::CreateCube()` in samples
- Soft body and rigid body must share collision layers that allow interaction
- The existing collision layer setup in JoltPhysicsSimulator may need extension
- Grid spacing = size / grid_resolution

**Size**: M (2-3 days)
**Dependencies**: US-01, US-02

---

## US-04: JoltPhysicsSimulator Soft Body Mesh Extraction

### Problem (The Pain)

The jelly cube now exists in the Jolt physics world and deforms on collision, but the deformed vertex positions are locked inside Jolt's `SoftBodyMotionProperties`. The AnimationRenderer cannot read these positions to update the renderable mesh. The visual representation stays frozen at the initial shape while the physics body deforms invisibly.

### Who (The User)

- Developer connecting physics output to rendering input
- Needs world-space vertex positions extracted from Jolt each frame
- Motivated by making the deformation visible in rendered frames

### Solution (What We Build)

Implement `get_soft_body_mesh()` in JoltPhysicsSimulator. Lock the Jolt body, read vertex positions from SoftBodyMotionProperties, transform them to world space using the body's center-of-mass transform, and read face indices from the shared settings.

### Domain Examples

#### Example 1: Extracting Mesh at Rest
Marco's 5x5x5 jelly cube has just been created. `get_soft_body_mesh(body_id)` returns 125 vertices at their initial grid positions (transformed to world space) and 576 face indices (192 triangles x 3 vertices each).

#### Example 2: Extracting Mesh After Collision
At frame 15, the jelly cube has hit the floor. The bottom-layer vertices are pushed upward (deformation), while top vertices are slightly lower than initial. All 125 vertex positions differ from the initial configuration. The face indices remain identical to Example 1.

#### Example 3: Vertex Positions Are World-Space
The jelly cube was created at position (0, 4, 0). After falling for 0.5 seconds, the center of mass is at approximately (0, 2.8, 0). The extracted vertex positions are in world space (accounting for center-of-mass transform), not local to the body origin.

### UAT Scenarios (BDD)

```gherkin
Scenario: Mesh extraction returns correct vertex count
  Given a 5x5x5 soft body cube (body ID 1) in the JoltPhysicsSimulator
  When get_soft_body_mesh(1) is called
  Then the returned SoftBodyMeshData has exactly 125 vertices
  And the returned SoftBodyMeshData has exactly 576 face indices

Scenario: Mesh extraction returns world-space positions
  Given a soft body cube created at position (0, 4, 0) with no physics stepping
  When get_soft_body_mesh() is called
  Then vertex positions are centered around (0, 4, 0) in world space

Scenario: Mesh extraction reflects deformation
  Given a soft body cube that has been stepped through a floor collision
  When get_soft_body_mesh() is called before and after collision
  Then the post-collision vertex positions differ from pre-collision positions
  And the face indices are identical in both calls

Scenario: Face indices have consistent winding order
  Given a soft body cube with known initial vertex positions
  When get_soft_body_mesh() is called
  Then for each face (i0, i1, i2), the cross product (v1-v0) x (v2-v0) points outward
```

### Acceptance Criteria

- [ ] get_soft_body_mesh() returns SoftBodyMeshData with correct vertex count (grid_resolution^3)
- [ ] Vertex positions are in world space (center-of-mass transform applied)
- [ ] Face indices are returned as a flat vector (3 indices per triangle)
- [ ] Face indices remain constant across multiple calls (topology unchanged)
- [ ] Vertex positions change after physics stepping through a collision

### Technical Notes

- Must lock Jolt body via BodyLockRead before accessing SoftBodyMotionProperties
- Transform: `worldPos = body.GetCenterOfMassTransform() * vertex.mPosition`
- Face data comes from `SoftBodyMotionProperties::GetFaces()` (or cached from shared settings at creation)
- Surface face count for NxNxN cube: 6 * (N-1)^2 * 2 triangles

**Size**: S (1 day)
**Dependencies**: US-03

---

## US-05: DeformableMesh Shape (Ray Intersection)

### Problem (The Pain)

Marco's jelly cube deforms in the physics world, but there is no shape in the ray tracer that can represent deforming geometry. The existing TriangleMesh takes vertex data at construction time and never changes. The existing TransformedShape applies a rigid transform (translation + rotation) but cannot handle per-vertex deformation. Marco needs a shape whose surface changes every frame.

### Who (The User)

- Developer building the rendering side of soft body support
- Needs a Shape subclass that supports ray intersection against a changing mesh
- Motivated by seeing the first deformed frame render correctly

### Solution (What We Build)

Create a DeformableMesh class that implements the Shape interface. It stores mutable vertex positions, constant face indices, and a material pointer. It performs Moller-Trumbore ray-triangle intersection (same algorithm as TriangleMesh) against the current vertex positions.

### Domain Examples

#### Example 1: Intersecting a Ray with Marco's Jelly Cube at Rest
A DeformableMesh with 192 faces and 125 vertices (initial positions forming a cube). A ray from the camera hits face 47. The intersection returns the hit point, the interpolated smooth normal, and the jelly_pink material.

#### Example 2: Intersecting After Deformation
The same mesh after vertex update (cube deformed from floor impact). The same ray now hits face 52 (different face visible from the same angle) at a slightly different hit point. The normal at the hit point reflects the deformed surface.

#### Example 3: Ray Misses the Deformed Mesh
A ray that previously hit the cube now misses it because the cube's AABB changed after deformation. The bounding box check rejects the ray early without testing individual triangles.

### UAT Scenarios (BDD)

```gherkin
Scenario: Ray hits a face of the deformable mesh
  Given a DeformableMesh with 4 vertices forming a square (2 triangles) and a lambertian material
  And the vertices are at (0,0,0), (1,0,0), (1,1,0), (0,1,0)
  When a ray from (0.5, 0.5, -1) in direction (0, 0, 1) is tested against the mesh
  Then hit() returns true
  And the hit record has t approximately equal to 1.0
  And the hit record material is the lambertian material

Scenario: Ray misses the deformable mesh
  Given a DeformableMesh with vertices forming a unit square at z=0
  When a ray from (5, 5, -1) in direction (0, 0, 1) is tested against the mesh
  Then hit() returns false

Scenario: Bounding box rejects rays early
  Given a DeformableMesh with a bounding box from (0,0,0) to (1,1,0)
  When a ray from (5, 5, -1) in direction (0, 0, 1) is tested
  Then the bounding box check rejects the ray before individual face tests

Scenario: Smooth normals are interpolated at hit point
  Given a DeformableMesh with 4 vertices and known per-vertex normals
  When a ray hits the mesh at barycentric coordinates (0.3, 0.3, 0.4)
  Then the hit record normal is the barycentric interpolation of the 3 vertex normals
```

### Acceptance Criteria

- [ ] DeformableMesh implements Shape::hit() using Moller-Trumbore ray-triangle intersection
- [ ] Hit record includes hit point, normal, t-value, and material pointer
- [ ] Normals are interpolated using barycentric coordinates (smooth shading)
- [ ] AABB bounding box is computed from vertex positions and used for early rejection
- [ ] Material pointer is stored and returned in hit records

### Technical Notes

- Follow the same intersection algorithm as TriangleMesh::hit_face()
- DeformableMesh belongs in domain ring: `domain/shapes/deformable_mesh.h`
- Face indices are stored as a flat vector of ints (3 per triangle)
- Constructor takes face indices and material; vertices are set via update method

**Size**: M (1-2 days)
**Dependencies**: None (can be developed in parallel with physics stories)

---

## US-06: DeformableMesh Vertex Update and Normal Recomputation

### Problem (The Pain)

The DeformableMesh can intersect rays against a static set of vertices, but Marco's jelly cube deforms every frame. The mesh needs to accept new vertex positions from the physics engine and recompute its smooth normals and bounding box so that the next frame renders the current deformed shape, not the previous one.

### Who (The User)

- Developer connecting per-frame physics output to the rendering mesh
- Needs a clean update method that recalculates all derived data (normals, AABB) from new positions
- Motivated by seeing smooth, artifact-free rendering of the deforming surface

### Solution (What We Build)

Add an `update_vertices()` method to DeformableMesh that replaces vertex positions, recomputes area-weighted smooth normals for all vertices, and recomputes the AABB.

### Domain Examples

#### Example 1: Normal Recomputation After Floor Impact
Marco's 5x5x5 cube hits the floor. The bottom face squishes flat while the top bulges. After `update_vertices()`, the normals on the bottom face point straight up (flattened), while the normals on the bulging top curve outward. This produces smooth shading that follows the deformation.

#### Example 2: AABB Shrinks After Compression
The cube was initially 1.5 units tall (AABB y from 3.25 to 4.75). After compression against the floor, the AABB y-range is 0.0 to 1.2. Rays that were aimed at the original position now correctly miss the compressed mesh.

#### Example 3: Consistent Winding After Deformation
After extreme deformation, some faces might invert their winding order (normals flip inward). The normal recomputation must handle this by using area-weighted averaging that naturally produces outward-pointing normals for well-formed meshes.

### UAT Scenarios (BDD)

```gherkin
Scenario: Vertices are replaced with new positions
  Given a DeformableMesh with 4 vertices at known initial positions
  When update_vertices() is called with 4 new positions
  Then subsequent hit() calls use the new vertex positions
  And hit points reflect the new geometry

Scenario: Smooth normals are recomputed from deformed geometry
  Given a DeformableMesh representing a flat quad (normal pointing in +z)
  When update_vertices() moves two vertices forward, tilting the quad
  Then the recomputed vertex normals reflect the tilted surface orientation

Scenario: AABB is recomputed from deformed vertices
  Given a DeformableMesh with initial AABB from (0,0,0) to (1,1,1)
  When update_vertices() moves all vertices to the range (2,2,2) to (3,3,3)
  Then the new AABB is from (2,2,2) to (3,3,3)
  And rays outside this AABB are rejected without face intersection tests

Scenario: Area-weighted normal averaging produces smooth shading
  Given a DeformableMesh with a vertex shared by 4 faces of different sizes
  When normals are recomputed
  Then the vertex normal is the normalized sum of face normals weighted by face area
  And larger faces contribute more to the vertex normal than smaller faces
```

### Acceptance Criteria

- [ ] update_vertices() replaces internal vertex positions with the provided vector
- [ ] Smooth normals recomputed using area-weighted face normal averaging after each update
- [ ] AABB recomputed from new vertex positions after each update
- [ ] Subsequent hit() calls use the updated vertex positions and normals
- [ ] update_vertices() validates that the new vertex count matches the face index expectations

### Technical Notes

- Area-weighted normal: accumulate `cross(v1-v0, v2-v0)` per face (magnitude = 2x face area), sum into vertex normals, then normalize. No explicit area calculation needed.
- The face indices never change, so adjacency information can be precomputed at construction.
- This method is called once per frame per soft body -- performance is important but not critical for 125 vertices.

**Size**: S (1 day)
**Dependencies**: US-05

---

## US-07: AnimationRenderer Soft Body Loop

### Problem (The Pain)

The AnimationRenderer currently updates rigid bodies each frame by reading transforms from the PhysicsSimulator and applying them via TransformedShape::set_transform(). But Marco's jelly cube is a DeformableMesh, not a TransformedShape. There is no code path that extracts deformed mesh data from physics and feeds it into a DeformableMesh. The jelly cube renders at its initial shape in every frame, frozen while the physics simulation deforms it invisibly.

### Who (The User)

- Technical artist running `nwave render --physics-animate`
- Expects the rendered frames to show the jelly deforming over time
- Motivated by seeing the complete animation: fall, impact, deform, bounce, wobble, settle

### Solution (What We Build)

Extend the AnimationRenderer's per-frame loop to detect soft body shapes and update their DeformableMesh with deformed vertex data from the PhysicsSimulator, in addition to the existing rigid body transform updates.

### Domain Examples

#### Example 1: Frame 15 of Marco's Scene
Physics has stepped 450 times (30 steps/frame x 15 frames). The jelly cube has fallen and is about to hit the 'e'. The AnimationRenderer calls `physics_->get_soft_body_mesh(jelly_body_id)` and passes the 125 vertices to `deformable_mesh->update_vertices()`. The renderer then ray-traces the scene with the deformed jelly and the rigid 'e' at its current transform.

#### Example 2: Mixed Scene with Rigid and Soft Bodies
Marco's scene has 3 objects: floor (static, no update), letter_e (dynamic rigid, transform update via TransformedShape), jelly_cube (soft, vertex update via DeformableMesh). The AnimationRenderer handles each type correctly in the same frame loop.

#### Example 3: 150 Frames of Animation
Over 150 frames (5 seconds at 30fps), the AnimationRenderer writes frame_0000.ppm through frame_0149.ppm. Each frame shows the jelly at its current deformed state. The CLI prints the ffmpeg command after completion.

### UAT Scenarios (BDD)

```gherkin
Scenario: Soft body mesh is updated each frame
  Given a scene with one soft body cube and one static floor
  And an AnimationRenderer configured for 10 frames
  When render() is called
  Then the DeformableMesh update_vertices() is called 10 times
  And each call uses vertex data from the physics simulation at that frame's time

Scenario: Rigid and soft bodies are both updated in the same frame
  Given a scene with a dynamic rigid box and a soft body cube
  When one frame is rendered
  Then the rigid box's TransformedShape has its transform updated from physics
  And the soft body's DeformableMesh has its vertices updated from physics
  And both updates occur before ray tracing begins

Scenario: AnimationRenderer identifies soft body shapes
  Given a scene with shapes at indices [0: floor, 1: letter_e, 2: jelly_cube]
  And shape_physics entries mark shape 2 as body_type SOFT
  When the AnimationRenderer processes a frame
  Then shape 0 receives no physics update (static)
  And shape 1 receives a rigid transform update
  And shape 2 receives a soft body vertex update

Scenario: Output frames show progressive deformation
  Given a scene with a jelly cube dropping from height 4 to a floor at y=0
  And 30 frames at 30fps (1 second of simulation)
  When render() completes
  Then frame_0000.ppm shows the cube at its initial position
  And frame_0014.ppm shows the cube mid-fall (lower y)
  And frame_0029.ppm shows the cube deformed against the floor
```

### Acceptance Criteria

- [ ] AnimationRenderer detects which shapes are soft bodies (via BodyType::SOFT in shape_physics)
- [ ] Per frame: physics step completes, then soft body meshes are extracted and DeformableMesh updated, then rigid transforms updated, then frame is rendered
- [ ] Existing rigid body update behavior is unchanged
- [ ] Output frames are written in the existing frame_NNNN.ppm format
- [ ] The AnimationRenderer tracks the mapping from scene shape index to physics body ID for both rigid and soft bodies

### Technical Notes

- AnimationRenderer needs to know which scene shape indices correspond to soft bodies. This comes from the shape_physics vector's body_type field.
- The AnimationRenderer needs access to DeformableMesh pointers (via dynamic_cast or a separate tracking structure) to call update_vertices().
- The sequential pipeline (step -> update -> render) is preserved; no threading changes.

**Size**: M (1-2 days)
**Dependencies**: US-02, US-04, US-06

---

## US-08: YAML Parsing for soft_body_cube Object Type

### Problem (The Pain)

Marco wants to define his jelly cube in a YAML scene file, the same way he defines spheres, boxes, and the bowling ball. But the YamlSceneLoader does not recognize `type: soft_body_cube` or `body_type: soft`. Marco cannot author a soft body scene without modifying C++ code.

### Who (The User)

- Technical artist authoring scenes in YAML
- Expects new object types to follow the same patterns as existing ones (center, material, physics block)
- Motivated by scene authoring without touching C++ code

### Solution (What We Build)

Extend YamlSceneLoader to parse `type: soft_body_cube` objects with their specific properties (center, size, grid_resolution) and soft body physics properties (pressure, damping, edge_compliance, volume_compliance, solver_iterations). Extend BodyType parsing to recognize "soft".

### Domain Examples

#### Example 1: Marco's Jelly Cube YAML
```yaml
- name: jelly_cube
  type: soft_body_cube
  center: [0, 4, 0]
  size: 1.5
  grid_resolution: 5
  material: jelly_pink
  physics:
    body_type: soft
    pressure: 2000.0
    restitution: 0.3
    damping: 0.05
    edge_compliance: 0.0001
    volume_compliance: 0.0
    solver_iterations: 5
```
This parses into a DeformableMesh shape (initial cube vertices), a PhysicsProperties with body_type SOFT, and a SoftBodyDesc for physics registration.

#### Example 2: Sofia's Default-Parameter Jelly
```yaml
- name: simple_jelly
  type: soft_body_cube
  center: [0, 3, 0]
  size: 1.0
  material: jelly_material
  physics:
    body_type: soft
```
All soft body parameters default (grid_resolution=5, pressure=2000, etc.). Sofia does not need to specify every parameter.

#### Example 3: Invalid Configuration Caught at Parse
```yaml
- name: broken_jelly
  type: soft_body_cube
  center: [0, 3, 0]
  material: some_material
  physics:
    body_type: dynamic  # Wrong! Should be "soft"
```
The parser rejects this with an error: "soft_body_cube requires body_type: soft".

### UAT Scenarios (BDD)

```gherkin
Scenario: soft_body_cube with all parameters parses correctly
  Given a YAML scene with a soft_body_cube object specifying center, size,
        grid_resolution, material, and all physics parameters
  When the YamlSceneLoader loads the scene
  Then the resulting scene contains a DeformableMesh shape with initial cube geometry
  And the shape_physics entry has body_type SOFT
  And a SoftBodyDesc is available with all specified parameter values

Scenario: soft_body_cube with defaults parses correctly
  Given a YAML scene with a soft_body_cube specifying only center, size, material,
        and body_type soft
  When the YamlSceneLoader loads the scene
  Then grid_resolution defaults to 5
  And pressure defaults to 2000.0
  And volume_compliance defaults to 0.0

Scenario: body_type soft is parsed from YAML
  Given a YAML physics block with "body_type: soft"
  When parsed by the scene loader
  Then the PhysicsProperties has body_type BodyType::SOFT

Scenario: Existing scene files continue to load
  Given the nwave_bowling.yaml scene file (no soft body objects)
  When loaded by the updated YamlSceneLoader
  Then it loads successfully with no errors
  And all objects, materials, and physics are identical to before

Scenario: Missing size produces a parse error
  Given a YAML soft_body_cube without a "size" property
  When the YamlSceneLoader attempts to load it
  Then an error is returned indicating size is required for soft_body_cube
```

### Acceptance Criteria

- [ ] YamlSceneLoader recognizes `type: soft_body_cube` and creates a DeformableMesh + SoftBodyDesc
- [ ] Physics block `body_type: soft` maps to BodyType::SOFT
- [ ] Soft body physics properties (pressure, damping, edge_compliance, volume_compliance, solver_iterations) are parsed with defaults
- [ ] Existing YAML scenes parse and render without changes (backward compatible)
- [ ] Missing required fields (center, size, material) produce clear parse errors
- [ ] SceneLoadResult carries SoftBodyDesc data alongside shape and physics properties

### Technical Notes

- SceneLoadResult may need an additional field for soft body descriptions, or they can be carried in a new vector parallel to shape_physics
- The initial DeformableMesh geometry is a cube of the specified size at origin (the AnimationRenderer positions it via physics)
- grid_resolution, size are soft_body_cube-specific; they do not apply to other object types

**Size**: M (1-2 days)
**Dependencies**: US-01, US-05

---

## US-09: YAML Parsing for letter Object Type

### Problem (The Pain)

Marco wants to place a 3D 'e' letter in his scene by specifying `type: letter` with a character, height, and depth. Today, the only way to get a letter shape is to manually compose it from tiny boxes (as in nwave_bowling.yaml, where the 'e' is 12 separate box objects). This is tedious, produces blocky results, and does not give a smooth 3D mesh suitable for physics interaction with a soft body.

### Who (The User)

- Technical artist authoring scenes
- Wants a single YAML entry to produce a smooth 3D letter, not 12 manually positioned boxes
- Motivated by clean scene files and smooth rendering

### Solution (What We Build)

Extend YamlSceneLoader to parse `type: letter` objects. The parser invokes font-to-mesh generation to create a TriangleMesh for rendering and prepares convex hull data for physics registration.

### Domain Examples

#### Example 1: Marco's 'e' Letter
```yaml
- name: letter_e
  type: letter
  character: "e"
  font: "default"
  height: 1.2
  depth: 0.4
  center: [0, 0.6, 0]
  material: letter_stone
  physics:
    body_type: dynamic
    mass: 2.0
    friction: 0.5
    restitution: 0.2
```
Produces a 3D 'e' mesh with approximately 478 triangles, positioned at (0, 0.6, 0), with dynamic rigid body physics using convex-decomposed collision shape.

#### Example 2: Sofia's Static 'S' Letter
```yaml
- name: letter_s
  type: letter
  character: "S"
  font: "default"
  height: 2.0
  depth: 0.6
  center: [3, 1, 0]
  material: stone_gray
```
No physics block -- the letter is a static decorative element, rendered as a TriangleMesh but not registered with the physics engine.

#### Example 3: Custom Font Path
```yaml
- name: letter_a
  type: letter
  character: "A"
  font: "/path/to/CustomFont.ttf"
  height: 1.0
  depth: 0.3
  center: [0, 0, 0]
  material: gold_metal
```
Uses a specific TTF file instead of the bundled default font.

### UAT Scenarios (BDD)

```gherkin
Scenario: Letter 'e' with default font parses and generates mesh
  Given a YAML scene with a letter object: character "e", font "default",
        height 1.2, depth 0.4, center (0, 0.6, 0)
  When the YamlSceneLoader loads the scene
  Then the scene contains a TriangleMesh shape representing the 3D 'e'
  And the mesh has more than 100 triangles (smooth, not blocky)
  And the mesh is positioned at center (0, 0.6, 0)

Scenario: Letter with physics creates dynamic rigid body
  Given a letter object with physics body_type dynamic and mass 2.0
  When the scene is loaded and physics is initialized
  Then a rigid dynamic body is created for the letter
  And the physics collision shape is a compound of convex hulls

Scenario: Letter without physics is static decoration
  Given a letter object with no physics block
  When the scene is loaded
  Then a TriangleMesh is added to the scene for rendering
  And no physics body is created for the letter

Scenario: Missing character field produces error
  Given a letter object without a "character" field
  When the YamlSceneLoader attempts to load it
  Then a parse error is returned indicating character is required

Scenario: Unsupported glyph produces descriptive error
  Given a letter object with character set to an emoji or unsupported glyph
  When the YamlSceneLoader attempts to load it
  Then a descriptive error is returned at load time (not during rendering)
```

### Acceptance Criteria

- [ ] YamlSceneLoader recognizes `type: letter` and generates a 3D triangle mesh from the specified character
- [ ] Generated mesh has front face, back face, and side walls (closed, water-tight)
- [ ] Glyphs with interior contours (counters) are handled correctly (e.g., 'e', 'a', 'o')
- [ ] Physics block is optional; when present, creates a dynamic rigid body with convex-decomposed collision shape
- [ ] Missing required fields (character, height, depth) produce clear error messages
- [ ] Default font is bundled and used when font is "default"

### Technical Notes

- Depends on font-to-mesh generation (US-10) and convex decomposition (US-11)
- The letter mesh is generated once at scene load time, not per frame
- For rendering, the mesh is wrapped in TransformedShape for rigid body movement
- The character field should accept ASCII printable characters; non-ASCII is out of scope for v1

**Size**: M (1-2 days)
**Dependencies**: US-10, US-11

---

## US-10: Font Glyph to 3D Mesh Generation

### Problem (The Pain)

Marco wants to place a smooth 3D 'e' letter in his scene, but the nwave ray tracer has no capability to generate 3D geometry from font glyphs. Today, letters are built from manually positioned boxes (12 boxes for the 'e' in nwave_bowling.yaml), resulting in blocky, low-fidelity shapes. There is no automated pipeline from a font file to a renderable 3D mesh.

### Who (The User)

- Developer building the font-to-mesh pipeline
- Needs to convert a TrueType glyph outline into a triangulated, extruded 3D mesh
- Motivated by replacing the manual box-letter workflow with automated smooth mesh generation

### Solution (What We Build)

Integrate a font parsing library (ttf2mesh or FreeType + earcut.hpp) to extract a glyph's 2D outline, triangulate it, extrude it to a specified depth, and produce a closed 3D triangle mesh with vertices, face indices, and normals.

### Domain Examples

#### Example 1: Generating the 'e' Letter
Given the default sans-serif font and character 'e', the pipeline produces a 3D mesh with height 1.2 world units and depth 0.4. The mesh has approximately 400-600 triangles: the front face (triangulated 'e' outline with hole), the back face (same but offset), and side walls connecting front to back along the contour.

#### Example 2: Generating the 'W' Letter (No Holes)
Character 'W' has no interior contour. The pipeline produces a simpler mesh with a single outer contour, triangulated front and back, and side walls. Approximately 200-300 triangles.

#### Example 3: Generating the 'o' Letter (Circular Hole)
Character 'o' has a large circular interior contour. The triangulation must correctly identify the hole and exclude it from the front/back faces. The side walls must include both the outer contour edges and the inner contour edges (the interior wall of the hole).

### UAT Scenarios (BDD)

```gherkin
Scenario: Generate 3D mesh for character 'e' from default font
  Given the default bundled TrueType font
  When a 3D mesh is generated for character 'e' with height 1.2 and depth 0.4
  Then the result contains a valid triangle mesh with more than 100 triangles
  And the mesh bounding box has height approximately 1.2 and depth approximately 0.4
  And the mesh is a closed solid (no open edges except at the designed contour)

Scenario: Generated mesh handles interior contour (hole in 'e')
  Given the character 'e' which has an interior counter (hole)
  When a 3D mesh is generated
  Then the front and back faces have a hole where the counter is
  And side walls are generated for both the outer and inner contours
  And rays passing through the counter do not intersect the mesh

Scenario: Mesh vertices are centered at origin
  Given a 3D mesh generated for character 'e' with height 1.2
  When the mesh center is computed
  Then the mesh is approximately centered at origin (0, 0, 0)
  And the caller can position it via TransformedShape or center parameter

Scenario: Normals are correct for rendering
  Given a generated 3D letter mesh
  When normals are computed
  Then front face normals point in -z direction
  And back face normals point in +z direction
  And side wall normals point outward from the letter contour
```

### Acceptance Criteria

- [ ] Given a character, font file, height, and depth, produces a vector of vertices and face indices forming a closed 3D mesh
- [ ] Characters with interior contours ('e', 'a', 'o', 'd', 'p', 'b', etc.) produce meshes with correct holes
- [ ] The generated mesh has outward-facing normals suitable for rendering
- [ ] A default font is bundled with the project (open-source, sans-serif)
- [ ] The mesh generation runs at scene load time and completes in under 1 second for any single character

### Technical Notes

- Primary candidate: ttf2mesh (2-file C99 library, no external deps)
- Fallback: FreeType + earcut.hpp for triangulation
- The generated mesh is returned as vectors of Point3 and int suitable for constructing a TriangleMesh
- This belongs in the Infrastructure ring (external library integration)
- CMake integration needed for the font library dependency

**Size**: L (2-3 days)
**Dependencies**: None (can be developed in parallel)

---

## US-11: Letter Convex Decomposition for Physics

### Problem (The Pain)

Marco's 'e' letter needs to be a dynamic rigid body (to be knocked over by the jelly cube), but Jolt requires dynamic bodies to use convex shapes or compound shapes of convex hulls. The 'e' mesh is concave and has a hole. Jolt's MeshShape (which supports concave geometry) is restricted to static bodies. Without convex decomposition, the 'e' cannot participate in dynamic physics.

### Who (The User)

- Developer connecting the letter mesh to Jolt physics
- Needs to decompose a concave mesh into approximate convex hulls
- Motivated by enabling the jelly cube to knock over the 'e' letter

### Solution (What We Build)

Integrate a convex decomposition library (V-HACD or CoACD) to decompose the letter mesh into a set of convex hulls. Use these hulls to create a Jolt StaticCompoundShape for the dynamic rigid body.

### Domain Examples

#### Example 1: Decomposing the 'e' Mesh
The 'e' letter mesh (478 triangles) is decomposed into approximately 8-15 convex hulls. These hulls approximate the concave shape closely enough for convincing collision behavior.

#### Example 2: Simple Letters Need Few Hulls
The letter 'I' (a simple rectangle when extruded) decomposes into 1 convex hull (it is already convex). The letter 'L' decomposes into 2-3 hulls.

#### Example 3: Collision Shape vs. Render Shape
The convex hulls are used only for physics collision. The full concave triangle mesh is used for rendering. The physics shape is a lower-fidelity approximation of the visual shape.

### UAT Scenarios (BDD)

```gherkin
Scenario: 'e' mesh decomposes into convex hulls
  Given the 3D triangle mesh for character 'e'
  When convex decomposition is performed
  Then the result is a set of 5 or more convex hulls
  And each hull is a valid convex polyhedron
  And the union of hulls approximately covers the original mesh volume

Scenario: Convex hulls are used as Jolt compound shape
  Given a set of convex hulls from decomposing the 'e' mesh
  When a Jolt StaticCompoundShape is created from these hulls
  Then the compound shape is accepted as a dynamic rigid body collision shape
  And the body responds to collisions (soft body impact, floor contact)

Scenario: Already-convex shapes produce a single hull
  Given a 3D mesh for a simple rectangular letter like 'I'
  When convex decomposition is performed
  Then the result contains 1 convex hull

Scenario: Decomposition completes in reasonable time
  Given a letter mesh with fewer than 1000 triangles
  When convex decomposition is performed
  Then it completes in under 5 seconds
```

### Acceptance Criteria

- [ ] Given a triangle mesh (vertices + face indices), produces a set of convex hulls (each hull as a set of vertices)
- [ ] Convex hulls are suitable for Jolt's ConvexHullShape / StaticCompoundShape
- [ ] Decomposition of a typical letter mesh (< 1000 triangles) completes in under 5 seconds
- [ ] The decomposition produces a reasonable number of hulls (fewer than 30 for typical letters)

### Technical Notes

- V-HACD is the most commonly used library; CoACD produces tighter fits but is newer
- The decomposition runs once at scene load time, not per frame
- This belongs in the Infrastructure ring
- CMake integration needed for the decomposition library
- The convex hulls are passed to JoltPhysicsSimulator which creates ConvexHullShapes and combines them into a StaticCompoundShape

**Size**: M (2 days)
**Dependencies**: US-10

---

## US-12: Scene Validation for Soft Body Parameters

### Problem (The Pain)

Marco configures his jelly cube with pressure=50000 and solver_iterations=0. The simulation explodes -- vertices diverge to infinity, rendering produces NaN pixels, and 150 frames of garbage are written to disk. Marco wasted 20 minutes of render time before discovering the problem. If the validator had caught these extreme values before rendering started, Marco could have fixed the scene file immediately.

### Who (The User)

- Technical artist authoring YAML scenes
- Wants fast feedback on configuration errors before committing to a long render
- Motivated by avoiding wasted compute time on doomed simulations

### Solution (What We Build)

Extend the SceneValidator to check soft body parameter ranges and consistency. Produce errors for invalid configurations and warnings for risky values.

### Domain Examples

#### Example 1: Marco's Extreme Pressure Caught
Marco sets pressure=50000. The validator warns: "pressure 50000.0 exceeds recommended maximum of 10000; simulation may become unstable."

#### Example 2: Zero Solver Iterations Blocked
Marco sets solver_iterations=0. The validator errors: "solver_iterations must be at least 1; 0 iterations means no constraint solving."

#### Example 3: Negative Compliance Blocked
Marco sets edge_compliance=-0.001. The validator errors: "edge_compliance must be non-negative; got -0.001."

### UAT Scenarios (BDD)

```gherkin
Scenario: Valid soft body parameters pass validation
  Given a YAML scene with a soft_body_cube having grid_resolution 5,
        pressure 2000.0, solver_iterations 5, edge_compliance 0.0001,
        volume_compliance 0.0
  When the scene is validated
  Then validation passes with no errors and no warnings

Scenario: grid_resolution below minimum produces error
  Given a soft_body_cube with grid_resolution 1
  When the scene is validated
  Then validation fails with error: grid_resolution must be between 2 and 15

Scenario: grid_resolution above maximum produces error
  Given a soft_body_cube with grid_resolution 20
  When the scene is validated
  Then validation fails with error: grid_resolution must be between 2 and 15

Scenario: Excessive pressure produces warning
  Given a soft_body_cube with pressure 15000.0
  When the scene is validated
  Then validation passes with a warning about potential instability

Scenario: Negative compliance produces error
  Given a soft_body_cube with edge_compliance -0.001
  When the scene is validated
  Then validation fails with error: edge_compliance must be non-negative

Scenario: solver_iterations below 1 produces error
  Given a soft_body_cube with solver_iterations 0
  When the scene is validated
  Then validation fails with error: solver_iterations must be at least 1

Scenario: soft_body_cube with body_type dynamic produces error
  Given a soft_body_cube with physics body_type set to "dynamic"
  When the scene is validated
  Then validation fails with error: soft_body_cube requires body_type soft
```

### Acceptance Criteria

- [ ] grid_resolution validated: must be 2-15 (error outside range)
- [ ] pressure validated: non-negative (error if negative), warning if > 10000
- [ ] solver_iterations validated: must be >= 1 (error if 0), warning if < 3
- [ ] edge_compliance and volume_compliance validated: must be >= 0.0
- [ ] soft_body_cube must have body_type: soft (error if mismatch)
- [ ] letter objects validated: character must be a single printable ASCII character
- [ ] letter objects validated: font must be "default" or a path to an existing file
- [ ] Existing validation rules unchanged; new rules additive

### Technical Notes

- Extend existing SceneValidator::validate() method
- Validation runs before physics initialization (fail fast)
- Use the same ValidationResult / ValidationError pattern as existing validation

**Size**: S (1 day)
**Dependencies**: US-08, US-09

---

## US-13: Demo Scene - Jelly Cube Hits Letter 'e'

### Problem (The Pain)

All the individual components are built, but nobody has seen them work together. Marco has no example scene to start from. Without a reference scene, early adopters must figure out the YAML syntax, parameter tuning, and camera placement from scratch. A working demo scene proves the feature is complete and provides a starting template.

### Who (The User)

- Technical artist wanting to try the new soft body feature
- Needs a copy-paste-and-run example scene
- Motivated by seeing the jelly cube + 'e' letter animation with zero configuration effort

### Solution (What We Build)

Create a complete YAML demo scene file with a pink translucent jelly cube, a stone-colored 'e' letter, a floor, lighting, and camera positioned to capture the impact and bounce. Include an animation block for 5 seconds at 30fps.

### Domain Examples

#### Example 1: Marco Runs the Demo
Marco runs `nwave render scenes/jelly_e.yaml --physics-animate`. 150 frames render to `frames/jelly_e/`. He assembles them with ffmpeg and watches the jelly cube fall, squish against the 'e', knock it over, and bounce back with wobble.

#### Example 2: Sofia Modifies the Demo
Sofia opens jelly_e.yaml, changes pressure from 2000 to 500 (softer jelly), and re-renders. The jelly now deforms more on impact and bounces less. The scene file serves as a tuning starting point.

#### Example 3: Validation Before Render
Marco runs `nwave validate scenes/jelly_e.yaml`. Validation passes, confirming all parameters are within range, materials are valid, and the letter mesh generates correctly.

### UAT Scenarios (BDD)

```gherkin
Scenario: Demo scene loads and validates
  Given the jelly_e.yaml demo scene file
  When it is validated with "nwave validate jelly_e.yaml"
  Then validation passes with no errors

Scenario: Demo scene renders 150 frames
  Given the jelly_e.yaml demo scene file
  When rendered with "nwave render jelly_e.yaml --physics-animate"
  Then 150 frames are written to the configured output directory
  And the CLI prints the ffmpeg assembly command

Scenario: Jelly cube deforms visibly in the animation
  Given the rendered frame sequence from jelly_e.yaml
  When frame 0 and frame 15 are compared
  Then the jelly cube is at a different position in frame 15 (has fallen)
  And the jelly cube's silhouette in frame 15 differs from the initial cube shape (deformation visible)

Scenario: Letter 'e' topples from jelly impact
  Given the rendered frame sequence
  When frame 20 and frame 40 are compared
  Then the letter 'e' has rotated from its initial upright orientation (toppling)

Scenario: Jelly material is translucent pink
  Given the demo scene materials
  When the jelly_cube is rendered
  Then it uses a dielectric material with pink tint
  And specular highlights are visible on the jelly surface
```

### Acceptance Criteria

- [ ] A YAML scene file exists at `scenes/jelly_e.yaml` (or similar path)
- [ ] Scene includes: static floor, soft_body_cube (pink dielectric), letter 'e' (dynamic rigid, lambertian)
- [ ] Camera is positioned to capture the impact from a 3/4 view
- [ ] Animation configured for 5.0 seconds at 30fps with output directory
- [ ] Scene validates without errors using `nwave validate`
- [ ] Scene renders without crashes for all 150 frames
- [ ] The jelly cube shows visible deformation and bounce
- [ ] The letter 'e' receives impulse and topples

### Technical Notes

- Jelly parameters: pressure=2000, edge_compliance=0.0001, volume_compliance=0.0, damping=0.05, grid_resolution=5, restitution=0.3
- Letter parameters: height=1.2, depth=0.4, mass=2.0, friction=0.5
- Camera should be positioned slightly above and to the side for a dynamic view
- Lighting: single point light for clear shadows and specular highlights

**Size**: S (< 1 day)
**Dependencies**: US-07, US-08, US-09, US-12

---

## Summary Table

| Story | Title | Size | Priority | Dependencies |
|---|---|---|---|---|
| US-01 | Soft Body Domain Types | S | P0 | None |
| US-02 | PhysicsSimulator Soft Body API | S | P0 | US-01 |
| US-03 | JoltPhysicsSimulator Soft Body Creation | M | P1 | US-01, US-02 |
| US-04 | JoltPhysicsSimulator Soft Body Mesh Extraction | S | P1 | US-03 |
| US-05 | DeformableMesh Shape (Ray Intersection) | M | P0 | None |
| US-06 | DeformableMesh Vertex Update + Normal Recomputation | S | P1 | US-05 |
| US-07 | AnimationRenderer Soft Body Loop | M | P2 | US-02, US-04, US-06 |
| US-08 | YAML Parsing: soft_body_cube | M | P2 | US-01, US-05 |
| US-09 | YAML Parsing: letter Object Type | M | P3 | US-10, US-11 |
| US-10 | Font Glyph to 3D Mesh Generation | L | P2 | None |
| US-11 | Letter Convex Decomposition for Physics | M | P3 | US-10 |
| US-12 | Scene Validation: Soft Body Parameters | S | P3 | US-08, US-09 |
| US-13 | Demo Scene: Jelly Cube + Letter 'e' | S | P4 | US-07, US-08, US-09, US-12 |

**Total estimated effort**: ~18-22 days
