# Requirements: Scene Physics Animation

**Document ID**: REQ-SPA-001
**Feature**: scene-physics-animation
**Date**: 2026-02-17
**Status**: Ready for DESIGN wave
**Source**: UX Journey UXJ-SPA-001, Experience Map XM-SPA-001, Research RES-PHYSICS-001

---

## 1. Problem Statement

Andrea, a C++ developer building a personal ray tracer, currently has all scenes hardcoded in `main.cpp`. Adding or modifying a scene requires editing C++ source code, recompiling, and relinking. The existing animation capability is limited to a fixed 360-degree camera orbit -- objects themselves never move. Andrea wants to describe physics-driven animated scenes in YAML files where objects roll, collide, and fall under gravity, producing frame sequences that become videos. Today, achieving this would require manually coding every object position for every frame -- hundreds of lines of C++ for even a simple ball-hits-blocks scenario.

---

## 2. Stakeholders

| Stakeholder | Role | Key Interest |
|---|---|---|
| Andrea (primary) | Developer and sole user | Describe scenes declaratively in YAML; see physics-based animations rendered without manual per-frame coding |
| Future contributors | Potential collaborators | Clear scene format and clean architecture boundaries so new primitives/materials integrate without rearchitecting |

---

## 3. Business Goals

1. **Eliminate recompilation for scene changes**: Scenes load from YAML files at runtime.
2. **Enable physics-driven object motion**: Objects move under gravity, collide, bounce, and settle -- driven by a physics engine, not hand-coded keyframes.
3. **Produce animation frame sequences**: The system renders one image per frame, suitable for assembly into video via ffmpeg.
4. **Maintain architectural integrity**: New capabilities integrate into the existing Clean Architecture (Core, Domain, Application, Infrastructure) without violating the dependency rule.

---

## 4. Scope

### 4.1 In Scope

| Capability | Description |
|---|---|
| YAML scene loading | Parse YAML files into existing Scene, Camera, Material, Shape, Light objects |
| Scene validation | Validate material references, parameter ranges, structural completeness with actionable error messages |
| Physics properties on objects | Optional `physics:` block per object specifying body_type, mass, velocity, friction, restitution |
| Rigid body simulation | Gravity, collisions, bouncing, resting via an integrated physics engine (Jolt Physics recommended by research) |
| Physics-to-shape transform pipeline | Extract per-frame position and rotation from physics engine; apply to ray tracer shapes via TransformedShape wrapper |
| Animation rendering loop | Step physics, update scene, render frame, write image -- repeated for each frame |
| CLI integration | `nwave render <scene.yaml>` for static render; `nwave render <scene.yaml> --physics-animate` for animation |
| Progress reporting | Frame counter, progress bar, ETA during multi-frame render |

### 4.2 Out of Scope

| Item | Rationale |
|---|---|
| Built-in video encoding (ffmpeg integration) | External dependency; provide copy-paste ffmpeg command instead |
| Kinematic animation paths (scripted keyframes) | Separate feature; requires YAML format extension beyond physics |
| Scene includes (`!include`) | Separate feature for scene composition |
| Parallel frame rendering | Performance optimization for later |
| BVH acceleration rebuild per frame | Optimization; start without BVH for animated scenes, measure first |
| Soft body / cloth / fluid physics | Only rigid body dynamics in this feature |

---

## 5. Functional Requirements

### FR-1: YAML Scene File Parsing

The system shall parse a YAML file containing `scene.materials`, `scene.objects`, `scene.lights`, and `scene.camera` sections into the existing domain objects (Material, Shape, Light, Camera).

**Domain examples**:
- Andrea writes a YAML file with a red Lambertian sphere (center [-3, 0.5, 0], radius 0.5), a white metallic ground plane, and a point light at [0, 10, 5]. The system constructs a Scene with one Sphere, one Plane, one PointLight, and a Camera.
- Andrea references material name `"green_glass"` in an object's `material:` field. The system resolves the reference to the Dielectric material defined in the `materials:` section.
- Andrea misspells a material name as `"geen_glass"`. The system reports the error with the object name, line number, and suggests `"green_glass"`.

### FR-2: Scene Validation

The system shall validate a parsed scene before rendering, checking: material reference resolution, parameter ranges (FOV 1-179, mass > 0 for dynamic bodies, friction/restitution in [0,1]), structural completeness (at least one object, one light, camera present), and animation config when `--physics-animate` is used.

**Domain examples**:
- Andrea's YAML has a sphere with `physics.mass: -2.0`. Validation reports: `Object "ball" (line 22): physics.mass must be positive, got -2.0`.
- Andrea runs `nwave render scene.yaml --physics-animate` but the YAML has no `animation:` section. Validation reports: `Animation config required when --physics-animate is used. Add an animation: section with duration, physics_timestep, and render_fps.`
- Andrea's scene has 3 materials, 26 objects (1 static, 25 dynamic), 1 light, a camera, and valid animation config. Validation passes with summary: `Scene is valid. Ready to render.`

### FR-3: Physics Properties on Objects

Each object in YAML may have an optional `physics:` block. Objects without a `physics:` block default to static (no motion). Dynamic objects have mass, optional initial velocity, friction, and restitution.

**Domain examples**:
- The ground plane has `physics: { body_type: static }`. It never moves during simulation.
- A red rubber ball has `physics: { body_type: dynamic, mass: 2.0, initial_velocity: [8, 0, 0], friction: 0.3, restitution: 0.4 }`. It starts rolling rightward at 8 m/s.
- A glass box composing the letter W has `physics: { body_type: dynamic, mass: 0.3, friction: 0.5, restitution: 0.2 }`. It starts at rest, waiting to be hit.

### FR-4: Rigid Body Physics Simulation

The system shall simulate rigid body dynamics: gravity pulls dynamic objects downward, objects collide with each other and with static surfaces, collisions produce realistic bouncing (governed by restitution) and friction. Simulation uses a fixed timestep.

**Domain examples**:
- A sphere (mass 2.0 kg) starts at [-5, 0.5, 0] with initial velocity [8, 0, 0]. After 0.625 seconds of simulation at 60 Hz (37 steps), the sphere reaches approximately [0, 0.5, 0] -- the location of the W letter blocks.
- The sphere collides with 25 glass blocks composing the letter W. Blocks scatter outward and fall under gravity. Blocks that reach the floor bounce briefly (restitution 0.2) and settle.
- A sphere dropped from height 5.0 m with restitution 0.7 bounces to approximately 0.7 * 5.0 = 3.5 m. Each successive bounce is lower until the sphere comes to rest.

### FR-5: Transform Pipeline (Physics to Shapes)

The system shall extract position and rotation from the physics engine for each dynamic body after each physics step, and apply those transforms to ray tracer shapes so they render at their updated positions.

**Domain examples**:
- After 100 physics steps, the ball's center has moved from [-5, 0.5, 0] to [1.2, 0.3, 0.1]. The Sphere shape used by the renderer reflects this new position.
- A box that was upright at simulation start has rotated 45 degrees after being struck. The Box shape renders at the rotated orientation (requires Matrix4x4 transform applied to rays).
- A block that fell on its edge settles at an angle. The renderer shows the block resting at that angle, not snapping to axis-aligned orientation.

### FR-6: Animation Rendering Loop

The system shall render one frame per `1.0 / render_fps` seconds of simulation time. For each frame: advance physics to the frame's timestamp, update all shape transforms, render the scene, write the frame image to the output directory.

**Domain examples**:
- A 5-second animation at 30 fps produces 150 frames: `frame_0000.ppm` through `frame_0149.ppm`.
- Physics runs at 60 Hz, render at 30 fps. The system steps physics twice between each rendered frame.
- After all 150 frames are written, the system prints the ffmpeg command: `ffmpeg -framerate 30 -i frames/frame_%04d.ppm -c:v libx264 -pix_fmt yuv420p output.mp4`.

### FR-7: CLI Commands

- `nwave render <scene.yaml>` -- Load scene from YAML, render a single static frame.
- `nwave render <scene.yaml> --physics-animate` -- Load scene, simulate physics, render all animation frames.
- `nwave validate <scene.yaml>` -- Validate scene without rendering.
- CLI flags `--width`, `--spp`, `--output-dir`, `--fps` override YAML values.

**Domain examples**:
- Andrea runs `nwave render bouncing_ball.yaml` and gets a single PPM of the scene at its initial state.
- Andrea runs `nwave render nwave_bowling.yaml --physics-animate --width 400 --spp 4` for a fast preview iteration (low resolution, low samples).
- Andrea runs `nwave validate nwave_bowling.yaml` to check for errors before committing to a long render.

---

## 6. Non-Functional Requirements

| ID | Requirement | Target | Rationale |
|---|---|---|---|
| NFR-1 | Scene loading time | < 1 second for scenes with up to 500 objects | User should not wait noticeably for scene parsing |
| NFR-2 | Validation time | < 0.5 seconds | Instantaneous feedback feel |
| NFR-3 | Physics simulation speed | < 2 seconds for 5s at 60 Hz with 100 dynamic bodies | Physics should not be the bottleneck; rendering is |
| NFR-4 | Frame rendering time | Comparable to current renderer (proportional to resolution and SPP) | Physics integration should not degrade render performance |
| NFR-5 | Memory usage | < 500 MB for scenes with 500 objects and 300 animation frames | Should run on a standard development laptop |
| NFR-6 | Clean Architecture compliance | All new code follows the four-ring dependency rule | Maintainability and testability preserved |
| NFR-7 | Error messages | Include object name, line number (when available), field name, and suggested fix | Actionable errors reduce iteration time |

---

## 7. Constraints

| Constraint | Source | Impact |
|---|---|---|
| C++17 standard | Existing codebase | Physics engine must support C++17 (Jolt Physics does) |
| CMake 3.16+ build system | Existing codebase | Physics engine integrated via FetchContent |
| Clean Architecture four-ring model | Existing architecture (ARCH-RAYTRACER-001) | Physics interface in Ring 3, implementation in Ring 4, properties in Ring 2 |
| No Matrix4x4 implementation exists yet | Current codebase gap (see research gap 12.1) | Rotation support requires implementing Matrix4x4 with quaternion construction |
| Shape::hit() interface is immutable-looking | Current design (Shape has no set_position) | Use TransformedShape wrapper pattern (Open/Closed principle) |
| Concave meshes must be static in physics | Physics engine limitation | TriangleMesh objects with physics must be body_type: static |

---

## 8. Dependencies

| Dependency | Type | Status | Impact if Unresolved |
|---|---|---|---|
| yaml-cpp library | External library | Specified in architecture (ADR-004) but not yet integrated in CMakeLists.txt | Blocks all YAML loading stories |
| Jolt Physics library | External library | Recommended by research; not yet integrated | Blocks all physics simulation stories |
| Matrix4x4 implementation | Internal code | Not yet implemented (identified in architecture doc and research gap) | Blocks rotation support; translation-only works without it |
| TransformedShape wrapper | Internal code | Not yet implemented | Blocks physics-driven shape repositioning |
| Quaternion type | Internal code | Not yet implemented | Blocks rotation extraction from physics engine |

---

## 9. Risks

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| Jolt Physics integration complexity exceeds estimate | Medium | Delays physics stories | Walking skeleton uses no physics; YAML loading delivers value independently. ReactPhysics3D as simpler fallback. |
| Matrix4x4/rotation pipeline introduces numerical artifacts | Medium | Visual glitches in rotated objects | Unit test transform pipeline with known rotations; compare against reference renders |
| Per-frame BVH rebuild creates performance bottleneck | Low | Slow animation rendering | Start without BVH for animated scenes (brute-force); measure before optimizing |
| YAML parsing edge cases (indentation, type coercion) | Low | Scene loading errors | Strict validation layer catches malformed input before it reaches domain objects |
| Physics timestep/render FPS mismatch causes temporal jitter | Low | Slightly jerky animation | Use accumulator pattern with interpolation between physics states |

---

## 10. Glossary

| Term | Definition |
|---|---|
| Body type | Classification of physics behavior: static (never moves), dynamic (fully simulated), kinematic (scripted motion) |
| Restitution | Coefficient of bounciness [0,1]. 0 = no bounce (clay), 1 = perfect bounce (superball) |
| Friction | Coefficient of surface resistance [0,1]. 0 = frictionless (ice), 1 = maximum grip (rubber) |
| Fixed timestep | Constant time increment per physics simulation step (typically 1/60 second) for stability and determinism |
| TransformedShape | A wrapper that applies a position/rotation transform to an existing Shape without modifying the original |
| SPP | Samples per pixel -- number of rays traced per pixel for anti-aliasing |
| Walking skeleton | The thinnest possible vertical slice through the entire system that validates the end-to-end pipeline |
