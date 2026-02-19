# Acceptance Criteria: Soft Body Jelly Physics

**Feature**: Deformable jelly cube with physics-driven animation
**Date**: 2026-02-18
**Format**: BDD Given/When/Then with testable assertions

---

## US-01: Soft Body Domain Types

### AC-01.1: SoftBodyDesc default construction

```gherkin
Scenario: SoftBodyDesc has sensible defaults
  Given a default-constructed SoftBodyDesc
  Then grid_resolution is 5
  And grid_spacing is 0.5
  And pressure is 2000.0
  And restitution is 0.3
  And damping is 0.05
  And edge_compliance is 0.0001
  And volume_compliance is 0.0
  And solver_iterations is 5
  And position is (0, 0, 0)
  And size is 1.0
```

### AC-01.2: SoftBodyDesc custom construction

```gherkin
Scenario: SoftBodyDesc accepts custom values
  Given a SoftBodyDesc with grid_resolution 8, pressure 3000.0,
        edge_compliance 0.001, volume_compliance 1e-5, damping 0.1,
        restitution 0.5, solver_iterations 10, position (2, 5, -1), size 2.0
  Then all fields return the specified values
```

### AC-01.3: SoftBodyMeshData holds mesh

```gherkin
Scenario: SoftBodyMeshData stores vertices and face indices
  Given a SoftBodyMeshData populated with 27 vertices and 144 face indices
  Then vertices.size() is 27
  And face_indices.size() is 144
  And each vertex is a Point3 value
  And each face index is a non-negative integer less than 27
```

### AC-01.4: BodyType SOFT is distinct

```gherkin
Scenario: BodyType::SOFT is a new enum value
  Given the BodyType enum
  Then BodyType::SOFT is not equal to BodyType::STATIC
  And BodyType::SOFT is not equal to BodyType::DYNAMIC
  And BodyType::SOFT is not equal to BodyType::KINEMATIC
```

### AC-01.5: Backward compatibility

```gherkin
Scenario: Existing PhysicsProperties construction is unchanged
  Given a PhysicsProperties with body_type DYNAMIC, mass 6.0, friction 0.3, restitution 0.4
  Then all fields match the specified values
  And the struct compiles identically to before the SOFT value was added
```

---

## US-02: PhysicsSimulator Soft Body API

### AC-02.1: Interface has add_soft_body

```gherkin
Scenario: PhysicsSimulator declares add_soft_body
  Given the PhysicsSimulator abstract class
  Then it has a pure virtual method add_soft_body(const SoftBodyDesc&) returning int
  And subclasses must implement it to compile
```

### AC-02.2: Interface has is_soft_body

```gherkin
Scenario: PhysicsSimulator declares is_soft_body
  Given the PhysicsSimulator abstract class
  Then it has a pure virtual method is_soft_body(int body_id) returning bool
```

### AC-02.3: Interface has get_soft_body_mesh

```gherkin
Scenario: PhysicsSimulator declares get_soft_body_mesh
  Given the PhysicsSimulator abstract class
  Then it has a pure virtual method get_soft_body_mesh(int body_id) returning SoftBodyMeshData
```

### AC-02.4: Existing interface unchanged

```gherkin
Scenario: Existing PhysicsSimulator methods are unchanged
  Given the PhysicsSimulator abstract class
  Then add_body, step, get_transform, set_gravity, and wake_all have the same signatures as before
```

---

## US-03: JoltPhysicsSimulator Soft Body Creation

### AC-03.1: Soft body created with correct vertex count

```gherkin
Scenario: 5x5x5 soft body has 125 vertices
  Given a SoftBodyDesc with grid_resolution 5 and size 1.5
  When add_soft_body() is called on the JoltPhysicsSimulator
  Then the returned body ID is valid (non-negative)
  And get_soft_body_mesh(body_id) returns 125 vertices

Scenario: 3x3x3 soft body has 27 vertices
  Given a SoftBodyDesc with grid_resolution 3 and size 1.0
  When add_soft_body() is called
  Then get_soft_body_mesh(body_id) returns 27 vertices
```

### AC-03.2: Soft body responds to gravity

```gherkin
Scenario: Soft body falls under gravity
  Given a JoltPhysicsSimulator with gravity (0, -9.81, 0)
  And a soft body cube created at position (0, 4, 0)
  When physics is stepped 30 times at dt=1/60
  Then the soft body mesh vertices have moved downward
  And the average y-position of vertices is less than 4.0
```

### AC-03.3: Soft body collides with static floor

```gherkin
Scenario: Soft body does not pass through floor
  Given a static rigid floor plane at y=0
  And a soft body cube created at position (0, 1, 0) with size 1.0
  When physics is stepped until the cube reaches the floor (approximately 60 steps)
  Then no vertex in get_soft_body_mesh() has y-position less than -0.01
  And some vertices have y-position near 0.0 (resting on floor)
```

### AC-03.4: Soft body deforms on contact

```gherkin
Scenario: Vertices are displaced from rest positions on collision
  Given a soft body cube at (0, 2, 0) and a static floor at y=0
  When the cube has fallen and made contact with the floor
  Then the vertex positions differ from a uniform grid pattern
  And the bottom-layer vertices are closer together (compressed)
  And the top-layer vertices are spread further apart (bulging)
```

### AC-03.5: Soft body impulse transfers to dynamic rigid body

```gherkin
Scenario: Dynamic rigid body receives impulse from soft body
  Given a soft body cube at (0, 4, 0) falling under gravity
  And a dynamic rigid box (mass 2.0) at (0, 1, 0)
  And a static floor at y=0
  When the soft body falls onto the rigid box
  And physics is stepped past the collision point
  Then the rigid box's velocity has a nonzero downward component
  And get_transform(rigid_body_id).position.y is less than 1.0
```

### AC-03.6: is_soft_body returns correct values

```gherkin
Scenario: is_soft_body distinguishes rigid from soft
  Given a rigid body added via add_body() returning ID 0
  And a soft body added via add_soft_body() returning ID 1
  Then is_soft_body(0) is false
  And is_soft_body(1) is true
```

---

## US-04: JoltPhysicsSimulator Soft Body Mesh Extraction

### AC-04.1: Vertex count matches grid

```gherkin
Scenario: Extracted mesh has correct counts
  Given a 5x5x5 soft body (125 vertices, 192 surface triangles)
  When get_soft_body_mesh() is called
  Then vertices has size 125
  And face_indices has size 576 (192 * 3)
```

### AC-04.2: Positions are world-space

```gherkin
Scenario: Vertex positions account for body position
  Given a soft body created at position (3, 7, -2) with size 1.0
  And no physics stepping (body at rest at initial position)
  When get_soft_body_mesh() is called
  Then vertex positions are centered around (3, 7, -2)
  And the bounding box of vertices approximately spans
      (2.5, 6.5, -2.5) to (3.5, 7.5, -1.5)
```

### AC-04.3: Topology is constant

```gherkin
Scenario: Face indices do not change across frames
  Given a soft body that has been through 100 physics steps
  When get_soft_body_mesh() is called at step 0 and step 100
  Then face_indices at step 0 equals face_indices at step 100
  And only vertex positions differ between the two calls
```

### AC-04.4: Deformed positions differ from initial

```gherkin
Scenario: Mesh extraction reflects deformation after collision
  Given a soft body that has collided with a floor
  When get_soft_body_mesh() is called
  Then at least 50% of vertex positions differ from the initial grid positions by more than 0.01 units
```

---

## US-05: DeformableMesh Shape (Ray Intersection)

### AC-05.1: Ray hits deformable mesh face

```gherkin
Scenario: Ray intersects a triangle face
  Given a DeformableMesh with a single quad (2 triangles):
      vertices [(0,0,0), (1,0,0), (1,1,0), (0,1,0)]
      face_indices [0,1,2, 0,2,3]
      material is lambertian red
  When a ray from (0.3, 0.3, -1) in direction (0, 0, 1) is tested
  Then hit() returns true
  And the hit record t is approximately 1.0
  And the hit record point is approximately (0.3, 0.3, 0.0)
  And the hit record material pointer is the lambertian red material
```

### AC-05.2: Ray misses deformable mesh

```gherkin
Scenario: Ray does not intersect any face
  Given a DeformableMesh with a quad at z=0 spanning (0,0) to (1,1)
  When a ray from (5, 5, -1) in direction (0, 0, 1) is tested
  Then hit() returns false
```

### AC-05.3: Smooth normal interpolation

```gherkin
Scenario: Hit normal is interpolated from vertex normals
  Given a DeformableMesh with a quad where vertex normals are set to
      [(0,0,-1), (0.2,0,-0.98), (0.2,0.2,-0.96), (0,0.2,-0.98)]
  When a ray hits the mesh at barycentric (0.5, 0.5, 0)
  Then the hit normal is the normalized interpolation of the 3 face vertex normals
  And the normal is a unit vector (length approximately 1.0)
```

### AC-05.4: AABB early rejection

```gherkin
Scenario: Bounding box rejects rays efficiently
  Given a DeformableMesh with all vertices in the region (0,0,0) to (1,1,1)
  When a ray from (5, 0.5, -1) in direction (0, 0, 1) is tested
  Then hit() returns false
  And the AABB check prevents iteration over individual faces
```

### AC-05.5: t_min and t_max respected

```gherkin
Scenario: Intersection outside t-range is rejected
  Given a DeformableMesh with a quad at z=0
  And a ray from (0.5, 0.5, -1) in direction (0, 0, 1)
  When hit() is called with t_min=0.0 and t_max=0.5
  Then hit() returns false (intersection at t=1.0 is outside range)
```

---

## US-06: DeformableMesh Vertex Update and Normal Recomputation

### AC-06.1: Vertices are replaced

```gherkin
Scenario: update_vertices replaces positions
  Given a DeformableMesh with 4 vertices at [(0,0,0), (1,0,0), (1,1,0), (0,1,0)]
  When update_vertices is called with [(0,0,1), (1,0,1), (1,1,1), (0,1,1)]
  Then a ray from (0.5, 0.5, 0) in direction (0, 0, 1) hits at t approximately 1.0
  And a ray from (0.5, 0.5, -1) in direction (0, 0, 1) no longer hits at t=1.0
```

### AC-06.2: Normals recomputed after update

```gherkin
Scenario: Normals reflect new geometry
  Given a DeformableMesh with a flat quad at z=0 (normal pointing in -z)
  When update_vertices tilts the quad (moving top vertices to z=0.5)
  Then the recomputed normals are no longer parallel to -z
  And the normals point away from the tilted surface
```

### AC-06.3: AABB recomputed after update

```gherkin
Scenario: Bounding box matches new positions
  Given a DeformableMesh with initial AABB (0,0,0)-(1,1,1)
  When update_vertices moves all vertices to (5,5,5)-(6,6,6) range
  Then a ray aimed at (0.5, 0.5, -1) misses (old AABB)
  And a ray aimed at (5.5, 5.5, 4) hits (new AABB)
```

### AC-06.4: Vertex count validated

```gherkin
Scenario: Mismatched vertex count is rejected
  Given a DeformableMesh constructed with 4 vertices and face indices referencing 0-3
  When update_vertices is called with 3 vertices (wrong count)
  Then the update is rejected or an error is raised
  And the mesh retains its previous valid state
```

### AC-06.5: Area-weighted normal averaging

```gherkin
Scenario: Larger faces contribute more to vertex normals
  Given a DeformableMesh where vertex V is shared by face A (area 1.0, normal +y)
      and face B (area 0.01, normal +x)
  When normals are recomputed
  Then vertex V's normal is nearly +y (dominated by the larger face A)
```

---

## US-07: AnimationRenderer Soft Body Loop

### AC-07.1: Soft body mesh updated each frame

```gherkin
Scenario: DeformableMesh receives new vertices every frame
  Given a scene with 1 soft body cube and a mock PhysicsSimulator
  And an AnimationRenderer configured for 5 frames
  When render() is called
  Then get_soft_body_mesh() is called 5 times (once per frame)
  And DeformableMesh::update_vertices() is called 5 times
  And each call uses distinct vertex data (positions change over time)
```

### AC-07.2: Mixed rigid and soft body updates

```gherkin
Scenario: Rigid and soft bodies updated in same frame loop
  Given a scene with shape 0 (static floor), shape 1 (dynamic rigid letter),
      shape 2 (soft body cube)
  And an AnimationRenderer configured for 1 frame
  When render() processes the frame
  Then shape 1 (rigid) has its TransformedShape transform updated via get_transform()
  And shape 2 (soft) has its DeformableMesh vertices updated via get_soft_body_mesh()
  And shape 0 (static) receives no physics update
```

### AC-07.3: Physics step before mesh extraction

```gherkin
Scenario: Physics advances before mesh is read
  Given a scene with a soft body cube at (0, 4, 0)
  And an AnimationRenderer stepping physics at 1/60s with 1 step per frame
  When the first frame is processed
  Then physics step() is called before get_soft_body_mesh()
  And the mesh data reflects the physics state after stepping
```

### AC-07.4: Frame output follows existing convention

```gherkin
Scenario: Output frames use existing naming pattern
  Given an AnimationRenderer rendering 150 frames to "frames/jelly/"
  When render() completes
  Then files frame_0000.ppm through frame_0149.ppm exist in "frames/jelly/"
```

### AC-07.5: Existing rigid-body-only scenes unaffected

```gherkin
Scenario: Scene with no soft bodies renders as before
  Given the nwave_bowling.yaml scene (all rigid bodies)
  When rendered with the updated AnimationRenderer
  Then output is identical to rendering with the previous AnimationRenderer
  And no soft body code paths are executed
```

---

## US-08: YAML Parsing for soft_body_cube

### AC-08.1: Full soft_body_cube parsed

```gherkin
Scenario: All soft body parameters parsed from YAML
  Given YAML:
    - name: jelly
      type: soft_body_cube
      center: [0, 4, 0]
      size: 1.5
      grid_resolution: 5
      material: pink_jelly
      physics:
        body_type: soft
        pressure: 2000.0
        restitution: 0.3
        damping: 0.05
        edge_compliance: 0.0001
        volume_compliance: 0.0
        solver_iterations: 5
  When YamlSceneLoader loads this scene
  Then a DeformableMesh shape is created with initial cube geometry
  And shape_physics has body_type SOFT
  And a SoftBodyDesc is produced with all parameters matching the YAML values
```

### AC-08.2: Defaults applied for optional parameters

```gherkin
Scenario: Soft body cube with minimal YAML uses defaults
  Given YAML:
    - name: simple_jelly
      type: soft_body_cube
      center: [0, 3, 0]
      size: 1.0
      material: jelly_mat
      physics:
        body_type: soft
  When loaded
  Then grid_resolution is 5 (default)
  And pressure is 2000.0 (default)
  And damping is 0.05 (default)
  And edge_compliance is 0.0001 (default)
  And volume_compliance is 0.0 (default)
  And solver_iterations is 5 (default)
```

### AC-08.3: body_type soft parsed

```gherkin
Scenario: body_type soft maps to BodyType::SOFT
  Given YAML with physics block containing "body_type: soft"
  When parsed
  Then PhysicsProperties.body_type is BodyType::SOFT
```

### AC-08.4: Backward compatibility

```gherkin
Scenario: Existing bowling scene loads unchanged
  Given the file nwave_bowling.yaml with no soft body objects
  When loaded by the updated YamlSceneLoader
  Then loading succeeds without errors
  And the scene, camera, materials, physics, and animation config are identical to before
```

### AC-08.5: Missing required field

```gherkin
Scenario: Missing size field causes error
  Given a soft_body_cube object without the "size" field
  When YamlSceneLoader attempts to load
  Then an error is raised mentioning that "size" is required for soft_body_cube

Scenario: Missing center field causes error
  Given a soft_body_cube object without the "center" field
  When YamlSceneLoader attempts to load
  Then an error is raised mentioning that "center" is required for soft_body_cube
```

---

## US-09: YAML Parsing for letter Object Type

### AC-09.1: Letter 'e' parsed and mesh generated

```gherkin
Scenario: Letter object produces triangle mesh
  Given YAML:
    - name: letter_e
      type: letter
      character: "e"
      font: "default"
      height: 1.2
      depth: 0.4
      center: [0, 0.6, 0]
      material: stone
      physics:
        body_type: dynamic
        mass: 2.0
  When loaded
  Then the scene contains a TriangleMesh (or TransformedShape wrapping a TriangleMesh)
  And the mesh has more than 100 triangles
  And shape_physics has body_type DYNAMIC with mass 2.0
```

### AC-09.2: Letter without physics

```gherkin
Scenario: Static decorative letter has no physics body
  Given a letter object with no physics block
  When loaded
  Then a TriangleMesh is added to the scene
  And no physics body is registered for this object
  And shape_physics entry has body_type STATIC (default)
```

### AC-09.3: Missing character field

```gherkin
Scenario: Missing character produces clear error
  Given a letter object without the "character" field
  When YamlSceneLoader attempts to load
  Then an error is raised: "character is required for letter object"
```

### AC-09.4: Custom font path

```gherkin
Scenario: Custom font file path is used
  Given a letter object with font set to "/path/to/CustomFont.ttf"
  And the file exists at that path
  When loaded
  Then the mesh is generated from the specified font file
```

---

## US-10: Font Glyph to 3D Mesh Generation

### AC-10.1: Mesh for 'e' with correct dimensions

```gherkin
Scenario: 'e' mesh has specified height and depth
  Given the default font and character 'e', height 1.2, depth 0.4
  When the 3D mesh is generated
  Then the mesh bounding box y-extent is approximately 1.2 (within 10%)
  And the mesh bounding box z-extent is approximately 0.4 (within 10%)
  And the mesh has more than 100 triangles
```

### AC-10.2: Interior contour handled

```gherkin
Scenario: 'e' mesh has a hole (counter)
  Given character 'e' which has an interior contour
  When the 3D mesh is generated
  Then a ray passing through the center of the counter (the hole in 'e')
      in the z-direction does not intersect the front or back faces
  And side walls exist around both the outer contour and the inner contour
```

### AC-10.3: Closed mesh (watertight)

```gherkin
Scenario: Generated mesh is closed
  Given a generated 3D letter mesh
  Then every edge in the mesh is shared by exactly 2 triangles (manifold condition)
  Or the mesh has no naked edges (all boundaries are between front/back/sides)
```

### AC-10.4: Mesh centered at origin

```gherkin
Scenario: Mesh is approximately centered
  Given a generated 3D letter mesh
  Then the centroid of the bounding box is within 0.1 units of origin (0, 0, 0)
```

### AC-10.5: Simple character with no holes

```gherkin
Scenario: Character 'T' produces a valid mesh without holes
  Given character 'T' (no interior contour), height 1.0, depth 0.3
  When the 3D mesh is generated
  Then the mesh is a closed solid
  And it has more than 50 triangles
  And no interior contour processing is needed
```

---

## US-11: Letter Convex Decomposition for Physics

### AC-11.1: Decomposition produces multiple hulls

```gherkin
Scenario: 'e' mesh decomposes into convex hulls
  Given the 3D triangle mesh for character 'e'
  When convex decomposition is performed
  Then the result contains between 5 and 30 convex hulls
  And each hull has at least 4 vertices (minimum for a 3D convex shape)
```

### AC-11.2: Hulls work as Jolt compound shape

```gherkin
Scenario: Compound shape from hulls is accepted by Jolt
  Given convex hulls from decomposing the 'e' mesh
  When a Jolt StaticCompoundShape is created from ConvexHullShapes
  And a dynamic rigid body is created using this compound shape
  Then the body is valid and active in the Jolt physics system
  And the body responds to gravity (falls under simulation)
```

### AC-11.3: Completion time

```gherkin
Scenario: Decomposition completes quickly
  Given a letter mesh with fewer than 1000 triangles
  When convex decomposition is performed
  Then it completes in under 5 seconds
```

---

## US-12: Scene Validation for Soft Body Parameters

### AC-12.1: Valid parameters pass

```gherkin
Scenario: Well-configured soft body passes validation
  Given a soft_body_cube with grid_resolution 5, pressure 2000.0,
        solver_iterations 5, edge_compliance 0.0001, volume_compliance 0.0
  When validated
  Then validation passes with no errors
```

### AC-12.2: Parameter range errors

```gherkin
Scenario Outline: Invalid parameter produces error
  Given a soft_body_cube with <parameter> set to <value>
  When validated
  Then validation fails with an error containing "<message>"

  Examples:
    | parameter         | value  | message                                    |
    | grid_resolution   | 1      | grid_resolution must be between 2 and 15   |
    | grid_resolution   | 20     | grid_resolution must be between 2 and 15   |
    | pressure          | -100   | pressure must be non-negative              |
    | solver_iterations | 0      | solver_iterations must be at least 1       |
    | edge_compliance   | -0.001 | edge_compliance must be non-negative       |
    | volume_compliance | -1e-5  | volume_compliance must be non-negative     |
```

### AC-12.3: Parameter warnings

```gherkin
Scenario: Excessive pressure produces warning
  Given a soft_body_cube with pressure 15000.0
  When validated
  Then validation passes (no error)
  But a warning is produced about potential instability

Scenario: Low solver iterations produces warning
  Given a soft_body_cube with solver_iterations 1
  When validated
  Then validation passes (no error)
  But a warning is produced about potential floppy behavior
```

### AC-12.4: Body type mismatch

```gherkin
Scenario: soft_body_cube with body_type dynamic fails
  Given a soft_body_cube with physics body_type "dynamic"
  When validated
  Then validation fails with error: "soft_body_cube requires body_type: soft"
```

### AC-12.5: Letter validation

```gherkin
Scenario: Letter with non-existent font path fails
  Given a letter object with font "/nonexistent/path/font.ttf"
  When validated
  Then validation fails with error about font file not found

Scenario: Letter with empty character fails
  Given a letter object with character ""
  When validated
  Then validation fails with error: character must be a single printable ASCII character
```

---

## US-13: Demo Scene

### AC-13.1: Scene file exists and validates

```gherkin
Scenario: Demo scene validates
  Given the demo scene file jelly_e.yaml
  When validated with nwave validate
  Then validation passes with no errors
```

### AC-13.2: Scene renders all frames

```gherkin
Scenario: 150 frames render without crash
  Given the demo scene file jelly_e.yaml
  When rendered with --physics-animate
  Then 150 frame files are produced
  And no frame contains NaN pixel values
  And the CLI prints the ffmpeg command
```

### AC-13.3: Visual behavior verification

```gherkin
Scenario: Jelly cube deforms in rendered output
  Given the rendered frame sequence
  When comparing frame 0 (initial) to frame 15 (mid-fall)
  Then the jelly cube occupies a different region of the image
  When comparing frame 15 to frame 20 (impact)
  Then the jelly cube silhouette is wider and shorter (compression visible)
```

### AC-13.4: Letter topples

```gherkin
Scenario: Letter 'e' receives impulse
  Given the rendered frame sequence
  When comparing frame 15 (pre-impact) to frame 30 (post-impact)
  Then the letter 'e' is no longer in its initial upright orientation
```

### AC-13.5: Materials and lighting

```gherkin
Scenario: Jelly material shows translucency
  Given the demo scene's jelly cube uses a dielectric material with pink tint
  When a frame is rendered with the cube in the scene
  Then the jelly cube shows specular highlights
  And the pink tint is visible in the rendered pixels
```
