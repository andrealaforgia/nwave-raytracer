# UX Journey: Soft Body Jelly Physics

**Feature**: Deformable jelly cube with physics-driven animation
**Persona**: Developer/artist using the nwave CLI to create physics-animated ray-traced scenes
**Scope**: Scene authoring (YAML) -> CLI execution -> rendered output (frames/video)

---

## Journey Overview

```
  [Author Scene]     [Run CLI]     [Physics Sim]     [Render Loop]     [Output]
       |                 |               |                 |               |
  Write YAML with   nwave render    Jelly falls,     Each frame:      Frame sequence
  soft_body type,   --physics-     deforms on hit,   extract mesh,    ready for
  'e' as rigid      animate        'e' topples,      recompute        ffmpeg -> mp4
  dynamic                          jelly bounces      normals, raytrace
```

**Emotional arc**: Curiosity (new YAML syntax) -> Confidence (validation passes) -> Anticipation (frames rendering) -> Satisfaction (wobbly jelly in output video)

---

## Step 1: Author the Scene YAML

**User intent**: Define a scene with a pink jelly cube above a rigid 'e' letter on a floor.

**What they type**: Edit a `.yaml` file, adding a new `soft_body` object type alongside familiar rigid objects.

**Emotional target**: Curious but not overwhelmed. The new syntax should feel like a natural extension of existing patterns, not a separate system.

### Proposed YAML Schema Extension

The user already knows this pattern for rigid physics objects:

```yaml
# Existing rigid body pattern (familiar)
- name: bowling_ball
  type: sphere
  center: [0, 5, 0]
  radius: 0.3
  material: ball_metal
  physics:
    body_type: dynamic
    mass: 6.0
    restitution: 0.4
```

The soft body extends this naturally:

```yaml
materials:
  - name: jelly_pink
    type: dielectric
    ior: 1.35
    tint: [0.95, 0.4, 0.6]       # Pink translucent jelly

  - name: letter_stone
    type: lambertian
    albedo: [0.6, 0.6, 0.65]

objects:
  # Ground plane
  - name: floor
    type: plane
    point: [0, 0, 0]
    normal: [0, 1, 0]
    material: ground

  # Soft body jelly cube -- new object type
  - name: jelly_cube
    type: soft_body_cube               # NEW: soft body primitive
    center: [0, 4, 0]                  # Starting position (above the 'e')
    size: 1.5                          # Cube edge length
    grid_resolution: 5                 # Vertex grid (5x5x5 = 125 vertices)
    material: jelly_pink
    physics:
      body_type: soft                  # NEW: soft body type
      pressure: 2000.0                 # Internal inflation pressure
      restitution: 0.3                 # Bounce coefficient
      damping: 0.05                    # Low = more wobble
      edge_compliance: 0.0001         # Surface spring softness
      volume_compliance: 0.0           # 0 = incompressible (jelly-like)
      solver_iterations: 5             # XPBD constraint iterations

  # Rigid 'e' letter
  - name: letter_e
    type: letter                       # NEW: font-generated 3D mesh
    character: "e"
    font: "default"                    # Built-in font or path to .ttf
    height: 1.2                        # Letter height in world units
    depth: 0.4                         # Extrusion depth
    center: [0, 0.6, 0]               # Positioned on floor
    material: letter_stone
    physics:
      body_type: dynamic
      mass: 2.0
      friction: 0.5
      restitution: 0.2
```

### Key Design Decisions in YAML

| Decision | Rationale |
|----------|-----------|
| `type: soft_body_cube` not `type: mesh` with soft body flag | Clarity: the user declares intent, not implementation. A soft body cube is a distinct concept. |
| `grid_resolution` exposed | The user controls fidelity vs. performance. Default 5 is safe. |
| `physics.body_type: soft` | Follows existing `dynamic`/`static`/`kinematic` pattern. Extends rather than replaces. |
| Jelly params grouped under `physics:` | Keeps material (visual) separate from physics (simulation). The user already knows the `physics:` block. |
| `type: letter` as a shape type | Treats font-derived meshes as first-class objects. The alternative (pre-generating OBJ files) adds friction to the authoring flow. |
| `volume_compliance: 0.0` as default | Jelly is nearly incompressible. This default produces correct behavior without tuning. |

### Validation Feedback

When the user runs `nwave validate jelly_scene.yaml`:

```
$ nwave validate jelly_scene.yaml
Scene validation passed:
  [x] Camera present
  [x] Objects present (3 objects: 1 plane, 1 soft body, 1 letter mesh)
  [x] Lights present
  [x] Material references valid
  [x] Parameter ranges valid
  [x] Soft body parameters valid (grid_resolution: 5, pressure: 2000.0)
  [x] Letter mesh generation: 'e' (478 triangles, 12 convex hulls for physics)
```

**Emotional note**: The validation explicitly confirms soft body and letter mesh details. This builds confidence before committing to a long render. The user sees that the system understood their intent.

---

## Step 2: Run the CLI

**User intent**: Start the physics animation render.

**What they type**:

```
$ nwave render jelly_scene.yaml --physics-animate --output-dir frames/jelly/
```

**Emotional target**: Confidence. The CLI output confirms what will happen before heavy computation begins.

### CLI Output (Start)

```
$ nwave render jelly_scene.yaml --physics-animate --output-dir frames/jelly/
Loading scene: jelly_scene.yaml
  Objects: 3 (1 plane, 1 soft body [125 vertices, 192 faces], 1 letter mesh [478 faces])
  Materials: 3
  Lights: 1
Physics: 3 bodies (1 static, 1 dynamic rigid, 1 soft body), 4500 total steps
Animation: 150 frames at 30fps (5.0s), output -> frames/jelly/
Rendering frame 1/150...
```

**Key information surfaced**:
- Soft body vertex/face count confirms resolution was understood
- Total physics steps sets expectations for duration
- Frame count and output directory confirmed before work begins

### CLI Output (Progress)

```
Rendering frame 42/150 [====>                    ] 28%  ~3m remaining
```

Standard progress bar. No change from existing behavior. Consistency matters.

### CLI Output (Completion)

```
Rendered 150 frames to frames/jelly/
Run: ffmpeg -framerate 30 -i frames/jelly/frame_%04d.ppm -c:v libx264 -pix_fmt yuv420p jelly_bounce.mp4
```

**Emotional note**: The ffmpeg command is copy-pasteable. The user does not have to remember the incantation. This is an established pattern from the existing bowling animation -- preserving it for soft body scenes maintains trust.

---

## Step 3: Physics Simulation (Internal)

This step is not directly visible to the user, but it determines the quality of the output. It runs inside the render loop.

**What happens per frame**:

```
For each frame:
  1. Step physics N times (physics_timestep * steps_per_frame)
     - Jolt XPBD solver: update soft body vertices (edge, volume, pressure constraints)
     - Soft body vertices collide with rigid 'e' letter -> impulse transfer
     - Rigid 'e' receives impulse, rotates/translates (topples)
     - Jelly deforms on contact, rebounds
  2. For rigid bodies: read transform (position + rotation)
  3. For soft bodies: extract deformed vertex positions + recompute normals
  4. Update scene (TransformedShape for rigid, DeformableMesh for soft)
  5. Ray trace the frame
  6. Write frame_NNNN.ppm
```

### Expected Physical Behavior (What the User Should See)

| Time | Event | Visual |
|------|-------|--------|
| 0.0s | Scene at rest | Jelly cube floating above, 'e' standing on floor |
| 0.0-0.5s | Jelly falls under gravity | Cube drops, slight wobble from air resistance |
| ~0.5s | Impact | Jelly hits top of 'e', deforms (squishes flat on bottom), 'e' starts to tip |
| 0.5-1.0s | Rebound + 'e' topple | Jelly bounces up, wobbling visibly. 'e' falls over with angular momentum |
| 1.0-2.0s | Secondary bounce | Jelly lands on floor/fallen 'e', deforms again, smaller bounce |
| 2.0-5.0s | Settling | Jelly wobbles with decreasing amplitude, comes to rest. 'e' lies flat. |

### Error Paths

| Error | When | User Sees | Recovery |
|-------|------|-----------|----------|
| Soft body explodes (vertices diverge) | Bad params (extreme pressure, zero iterations) | Distorted frames, potential NaN in render | Validate params: warn if pressure > 10000 or iterations < 2 |
| 'e' letter passes through floor | Collision layer misconfiguration | 'e' falls forever | Validation: check all dynamic bodies have collision with static floor |
| No visible deformation | Compliance too low (too stiff) | Jelly looks rigid, no wobble | Tuning guide in docs; defaults chosen for visible jelly behavior |
| Render is black/corrupt | Deformed mesh normals flipped | Dark patches on jelly surface | Normal recomputation must handle winding order consistently |

---

## Step 4: Output and Review

**User intent**: Convert frames to video and review the animation.

**What they type**:

```
$ ffmpeg -framerate 30 -i frames/jelly/frame_%04d.ppm -c:v libx264 -pix_fmt yuv420p jelly_bounce.mp4
$ open jelly_bounce.mp4
```

**Emotional target**: Satisfaction. The jelly wobbles convincingly, the 'e' topples, and the translucent pink material catches light.

**What success looks like**:
- Jelly cube deforms visibly on impact (not rigid)
- Deformation is smooth, not jagged (smooth normals working)
- Jelly bounces back toward original shape (volume preservation working)
- Wobble continues for several bounces (low damping working)
- 'e' letter receives the impact and falls over (rigid-soft interaction working)
- Pink translucent material refracts light through the jelly body
- No visual artifacts at deformation boundaries

---

## Shared Artifacts Registry

These data items cross component boundaries and must have a single source of truth.

| Artifact | Source | Consumed By | Notes |
|----------|--------|-------------|-------|
| `grid_resolution` | YAML scene file | JoltPhysicsSimulator (creates NxNxN grid), DeformableMesh (face count) | Single parse in YamlSceneLoader. Determines vertex count (N^3) and surface face count. |
| `soft_body_vertex_positions` | JoltPhysicsSimulator (per frame) | DeformableMesh::update_vertices() | Extracted from SoftBodyMotionProperties. World-space coordinates. Must be synchronized: physics writes, renderer reads. |
| `face_indices` | JoltPhysicsSimulator (once at creation) | DeformableMesh (constant topology) | Set once, never changes. The surface triangulation of the soft body cube. |
| `smooth_normals` | DeformableMesh::recompute_normals() | Ray tracer intersection | Recomputed each frame from deformed vertices. Area-weighted average of adjacent face normals. |
| `letter_mesh_vertices` | Font glyph -> ttf2mesh/FreeType -> extrusion | TriangleMesh (rendering), V-HACD -> CompoundShape (physics) | Generated once at scene load. Rendering mesh and physics collision shape derived from same source geometry. |
| `letter_convex_hulls` | V-HACD decomposition of letter mesh | JoltPhysicsSimulator (StaticCompoundShape for dynamic rigid body) | The 'e' letter physics body uses convex decomposition because concave MeshShape cannot be dynamic in Jolt. |
| `physics_timestep` | YAML animation config | AnimationRenderer loop, JoltPhysicsSimulator::step() | Controls simulation fidelity. Soft bodies need stable timestep (1/60s typical). |
| `output_directory` | YAML animation config / CLI --output-dir override | AnimationRenderer (frame file paths) | CLI override takes precedence over YAML value. |
| `material` (per soft body) | YAML scene file, parsed by YamlSceneLoader | DeformableMesh (used during ray-surface intersection) | Material is visual only; physics properties are separate under `physics:` block. |

---

## Integration Checkpoints

These are the points where components hand off data. Each is a potential failure point.

### Checkpoint 1: YAML Parse -> Scene Construction

**What flows**: Soft body description (center, size, grid_resolution, physics params) and letter description (character, height, depth) are parsed from YAML into domain objects.

**Failure modes**:
- Unknown `type: soft_body_cube` -> parser error. Must extend YamlSceneLoader to recognize new types.
- Unknown `physics.body_type: soft` -> parser error. Must extend BodyType enum.
- Letter mesh generation fails (unsupported glyph, missing font) -> scene load fails with clear error.

### Checkpoint 2: Scene Construction -> Physics Registration

**What flows**: Soft body description -> JoltPhysicsSimulator::add_soft_body(). Letter mesh -> convex decomposition -> JoltPhysicsSimulator::add_body() with compound shape.

**Failure modes**:
- Soft body and rigid body on different collision layers -> no interaction. Must ensure both are on interacting layers.
- Dynamic compound shape (letter) collision with soft body unvalidated in Jolt. Research flagged this as HIGH risk. Must test early.

### Checkpoint 3: Physics Step -> Mesh Extraction (Per Frame)

**What flows**: Deformed vertex positions from SoftBodyMotionProperties -> DeformableMesh::update_vertices().

**Failure modes**:
- Thread safety: if physics update and render read overlap. Research flagged this. Solution: frame-locked sequential (physics step completes before render reads, matching existing AnimationRenderer pattern).
- Vertex positions in local space vs. world space mismatch. Must apply CenterOfMassTransform.

### Checkpoint 4: DeformableMesh -> Ray Tracer

**What flows**: Updated vertices, recomputed normals, constant face indices -> ray-triangle intersection.

**Failure modes**:
- Normals point inward after deformation -> dark patches. Must validate winding order consistency.
- AABB not updated -> rays miss the deformed mesh. DeformableMesh must recompute AABB on vertex update.
- Material not assigned -> crash or default material. Must propagate material from YAML through to DeformableMesh.

### Checkpoint 5: Frame Output -> User

**What flows**: PPM frame files written to output directory. User assembles with ffmpeg.

**Failure modes**:
- No change from existing pipeline. Same frame_NNNN.ppm pattern. Same ffmpeg command suggestion.

---

## CLI Vocabulary Consistency

New terms introduced by this feature and how they align with existing CLI vocabulary.

| New Term | Context | Aligns With |
|----------|---------|-------------|
| `soft_body_cube` | YAML object type | Extends existing `sphere`, `box`, `cylinder`, `plane` pattern |
| `soft` | physics.body_type value | Extends existing `dynamic`, `static`, `kinematic` enum |
| `letter` | YAML object type | New shape type, follows same material/physics pattern as others |
| `grid_resolution` | soft body YAML property | New, no precedent. Name chosen for clarity over `grid_size` (avoids confusion with spatial size). |
| `pressure` | soft body physics property | New physics param, grouped under existing `physics:` block |
| `edge_compliance` | soft body physics property | New, specific to soft body solver |
| `volume_compliance` | soft body physics property | New, specific to soft body solver |
| `solver_iterations` | soft body physics property | New, specific to soft body solver |
| `damping` | soft body physics property | Analogous concept to existing `friction`/`restitution` |
| `--physics-animate` | CLI flag | Already exists, no change |

No new CLI flags required. The existing `--physics-animate` flag triggers the animation pipeline, which now also handles soft bodies. This is intentional: soft bodies are not a separate mode, they are objects in a physics scene.

---

## Gherkin Scenarios (Happy Path)

```gherkin
Feature: Soft body jelly physics animation

  Background:
    Given a YAML scene file with:
      | object       | type            | physics_type |
      | floor        | plane           | static       |
      | jelly_cube   | soft_body_cube  | soft         |
      | letter_e     | letter          | dynamic      |
    And the jelly_cube is positioned above letter_e
    And animation config has duration 5.0s at 30fps

  Scenario: Scene validates with soft body objects
    When the user runs "nwave validate jelly_scene.yaml"
    Then validation passes
    And output includes "1 soft body"
    And output includes "1 letter mesh"

  Scenario: Soft body cube falls and deforms on impact
    When the user runs "nwave render jelly_scene.yaml --physics-animate"
    Then 150 frames are rendered
    And at frame 15 the jelly_cube vertices differ from their initial positions
    And the jelly_cube volume at frame 15 is within 5% of initial volume

  Scenario: Rigid letter receives impulse from soft body
    When the physics simulation runs for 1.0 seconds
    Then the letter_e body has non-zero angular velocity
    And the letter_e y-position is lower than its initial position

  Scenario: Jelly bounces back after impact
    When the physics simulation runs for 1.5 seconds
    Then the jelly_cube center of mass y-position is higher than at impact frame
    And the jelly_cube vertices show oscillation (wobble)

  Scenario: Deformed mesh renders with smooth normals
    Given a soft body cube at rest on a flat surface (slightly deformed)
    When a frame is rendered
    Then the rendered image has no hard edges on the jelly surface
    And the jelly material shows specular highlights and refraction

  Scenario: Output follows existing frame convention
    When the animation completes
    Then frames are written as frame_0000.ppm through frame_0149.ppm
    And the CLI prints the ffmpeg assembly command
```

---

## Open Questions for Implementation

These are not blockers for the journey design but will need resolution during development.

1. **Font bundling**: Should a default font be bundled with the binary, or should the user always provide a `.ttf` path? Bundling a small sans-serif font (e.g., an open-source one) reduces friction for the `type: letter` feature.

2. **GPU Metal path for deformable meshes**: The existing Metal backend uploads vertex buffers once. Deformable meshes require per-frame uploads. The research recommends starting CPU-only for soft body scenes. Is that acceptable, or must Metal be supported from the start?

3. **Soft body parameter presets**: Should the YAML support named presets like `physics_preset: jelly` that expand to known-good parameter sets? This would reduce the learning curve but adds YAML complexity.

4. **Alternative to `type: letter`**: If font-to-mesh generation is too complex for v1, the 'e' could be authored as a pre-generated OBJ mesh file referenced in YAML (`type: mesh, file: e_letter.obj`). This is less elegant but decouples the features.
