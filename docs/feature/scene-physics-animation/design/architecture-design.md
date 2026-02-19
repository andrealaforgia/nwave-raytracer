# Architecture Design: Scene Physics Animation

**Document ID**: ARCH-SPA-001
**Feature**: scene-physics-animation
**Date**: 2026-02-17
**Status**: Draft
**Extends**: ARCH-RAYTRACER-001 (existing ray tracer architecture)

---

## 1. System Overview

This architecture extends the existing nwave-raytracer with three capabilities: YAML scene loading, rigid body physics simulation, and animated frame rendering. The existing Clean Architecture four-ring model (Core, Domain, Application, Infrastructure) is preserved. New components slot into the appropriate rings without modifying existing interfaces.

### 1.1 New Business Capabilities

| Capability | Description | Stakeholder Value |
|---|---|---|
| YAML scene loading | Declarative scene authoring via text files | Eliminate recompile cycle for scene changes |
| Scene validation | Pre-render integrity checking with actionable errors | Catch errors before committing to multi-minute renders |
| Physics simulation | Gravity, collisions, bouncing, settling via Jolt Physics | Automatic object motion without per-frame hand-coding |
| Animation rendering | Multi-frame render pipeline producing frame sequences | Physics-driven animated videos from a single CLI command |
| CLI subcommands | `nwave validate` / `nwave render` with composable flags | Structured workflow, preview iterations |

### 1.2 Quality Attributes (Additions to ISO 25010 Targets)

| Attribute | Target | Strategy |
|---|---|---|
| **Performance** | Scene load < 1s for 500 objects; physics < 2s for 5s/60Hz/100 bodies | yaml-cpp streaming parse; Jolt multi-threaded solver |
| **Correctness** | Physics plausibility: sphere dropped 5m reaches ground in ~1s; restitution 0.6 bounces to ~3m | Unit-tested physics adapter with position assertions |
| **Maintainability** | Physics engine replaceable without touching Application or Domain rings | PhysicsSimulator interface in Ring 3; adapter in Ring 4 |
| **Usability** | Validation errors include object name, field, value, and suggested fix | Multi-pass validator with edit-distance material suggestions |
| **Extensibility** | New shape types loadable from YAML without modifying existing loader code | Shape factory registry pattern in SceneLoader |

---

## 2. Extended Architecture Ring Model

The existing four-ring model gains new components (shown with `[NEW]` markers). Existing components remain unchanged.

```
+------------------------------------------------------------------------+
|                        Infrastructure (Ring 4)                          |
|  [EXISTING] CLI  |  PPMWriter                                         |
|  [NEW] YamlSceneLoader  |  Validator  |  JoltPhysicsSimulator          |
|  [NEW] CLI Dispatcher (validate/render subcommands)                    |
|  +------------------------------------------------------------------+  |
|  |                     Application (Ring 3)                          |  |
|  |  [EXISTING] Renderer                                              |  |
|  |  [NEW] PhysicsSimulator (interface)                               |  |
|  |  [NEW] AnimationRenderer (orchestrator)                           |  |
|  |  +--------------------------------------------------------------+|  |
|  |  |                    Domain (Ring 2)                            ||  |
|  |  |  [EXISTING] Shape | Material | Light | Camera | Scene        ||  |
|  |  |  [NEW] PhysicsProperties | AnimationConfig                   ||  |
|  |  |  [NEW] TransformedShape (wraps Shape + Matrix4x4)            ||  |
|  |  |  +----------------------------------------------------------+||  |
|  |  |  |               Core / Math (Ring 1)                        |||  |
|  |  |  |  [EXISTING] Vec3 | Point3 | Color3 | Ray | AABB          |||  |
|  |  |  |  [NEW] Matrix4x4 | Quaternion                            |||  |
|  |  |  +----------------------------------------------------------+||  |
|  |  +--------------------------------------------------------------+|  |
|  +------------------------------------------------------------------+  |
+------------------------------------------------------------------------+
```

---

## 3. New Component Descriptions

### 3.1 Ring 1: Core / Math (New Components)

| Component | Responsibility |
|---|---|
| **Matrix4x4** | 4x4 transformation matrix. Construction from translation + quaternion. Inverse, inverse-transpose. Point/vector/normal transform operations. |
| **Quaternion** | Unit quaternion for 3D rotation. Construction from axis-angle. Conversion to Matrix4x4. slerp for interpolation. |

These are pure value types with no dependencies beyond the C++ standard library. They extend the existing math foundation (Vec3, Ray, AABB).

### 3.2 Ring 2: Domain (New Components)

| Component | Responsibility |
|---|---|
| **PhysicsProperties** | Data struct carried by scene objects: body_type (static/dynamic/kinematic), mass, initial_velocity, friction, restitution. No physics engine dependency. |
| **AnimationConfig** | Data struct for animation parameters: duration, physics_timestep, render_fps, output_directory. Computes derived values (total_frames, steps_per_frame). |
| **TransformedShape** | Decorator wrapping any Shape with a Matrix4x4 transform. On `hit()`, transforms the incoming ray to local space, delegates to inner shape, transforms result back. Normals transformed via inverse-transpose. |

TransformedShape is the architectural bridge between physics output and rendering input. It preserves the Open/Closed principle -- existing shapes (Sphere, Box, etc.) remain unmodified.

### 3.3 Ring 3: Application (New Components)

| Component | Responsibility |
|---|---|
| **PhysicsSimulator** | Abstract interface defining: `add_body()`, `step()`, `get_transform()`, `set_gravity()`. Declares WHAT a physics simulator does without specifying HOW. |
| **AnimationRenderer** | Orchestrator use case. Coordinates: initialize physics from scene -> for each frame: step physics, extract transforms, apply to TransformedShapes, render frame via existing Renderer, return pixel buffer. |

AnimationRenderer depends on the existing Renderer (composition, not inheritance). It adds the physics-driven loop around single-frame rendering.

### 3.4 Ring 4: Infrastructure (New Components)

| Component | Responsibility |
|---|---|
| **YamlSceneLoader** | Parses YAML files via yaml-cpp. Constructs Scene, Camera, Material, Shape, Light, PhysicsProperties, AnimationConfig domain objects. Resolves material references by name. |
| **Validator** | Inspects loaded Scene for integrity: material references, parameter ranges, structural completeness, animation config presence. Reports all errors in a single pass with object name, field, value, and suggestion. |
| **JoltPhysicsSimulator** | Implements PhysicsSimulator interface using Jolt Physics. Maps ray tracer shapes to Jolt collision shapes. Extracts per-body transforms (position + quaternion) after each step. |
| **CLI Dispatcher** | Parses subcommands (`validate`, `render`) and flags (`--width`, `--spp`, `--physics-animate`, `--output-dir`, `--fps`). Routes to appropriate pipeline. |

---

## 4. Data Flow

### 4.1 Static Render (YAML to Single Frame)

```
[scene.yaml]
    |
    v
YamlSceneLoader (Ring 4) ---> Scene + Camera + RenderSettings (Ring 2)
    |
    v
Validator (Ring 4) ---> ValidationResult
    |  (pass)
    v
Renderer.render(camera, scene, settings) (Ring 3) ---> pixel buffer
    |
    v
PPMWriter.write(filename, pixels) (Ring 4) ---> output.ppm
```

### 4.2 Physics Animation (YAML to Frame Sequence)

```
[scene.yaml]
    |
    v
YamlSceneLoader (Ring 4) ---> Scene + Camera + PhysicsProperties + AnimationConfig
    |
    v
Validator (Ring 4) ---> ValidationResult (includes animation checks)
    |  (pass)
    v
AnimationRenderer.render_animation(scene, camera, config, settings) (Ring 3):
    |
    |  1. Wrap dynamic shapes in TransformedShape (Ring 2)
    |  2. Initialize PhysicsSimulator with bodies (Ring 3 interface, Ring 4 impl)
    |
    |  For frame = 0 to total_frames:
    |    3. PhysicsSimulator.step(dt) x steps_per_frame
    |    4. Extract PhysicsTransform per body
    |    5. Update TransformedShape.set_transform(matrix)
    |    6. Renderer.render(camera, scene, settings) ---> pixel buffer
    |    7. PPMWriter.write(frame_NNNN.ppm, pixels)
    |
    v
[frames/frame_0000.ppm ... frame_0149.ppm]
    |
    v
(user runs ffmpeg externally)
```

### 4.3 Physics-to-Render Transform Pipeline (Detail)

```
Jolt PhysicsSystem
    |
    | body_interface.GetCenterOfMassPosition(id) -> JPH::RVec3
    | body_interface.GetRotation(id) -> JPH::Quat
    |
    v
JoltPhysicsSimulator.get_transform(id) (Ring 4)
    |
    | Converts JPH types to nwave types
    |
    v
PhysicsTransform { position: Point3, rotation: Quaternion } (Ring 3)
    |
    v
Matrix4x4::from_translation_rotation(position, rotation) (Ring 1)
    |
    v
TransformedShape.set_transform(matrix) (Ring 2)
    |
    | On next render: Ray -> inverse_transform -> inner Shape::hit() -> transform result
    |
    v
Rendered frame reflects physics-computed position and orientation
```

---

## 5. Key Interfaces

### 5.1 PhysicsSimulator (Ring 3 -- Application)

```
BodyId add_body(shape_type, position, rotation, physics_properties) -> uint32_t
void step(dt: double)
PhysicsTransform get_transform(id: BodyId) -> { position, rotation }
void set_gravity(gravity: Vec3)
```

The interface uses domain types (Vec3, Point3, Quaternion) not physics-engine types. The adapter in Ring 4 translates.

### 5.2 SceneLoader (Ring 4 -- Infrastructure)

```
SceneLoadResult load(yaml_path: string) -> { scene, camera, render_settings, animation_config, physics_properties_map, warnings }
```

Returns all domain objects fully constructed. Physics properties are associated with shapes via a map keyed by shape pointer or index.

### 5.3 AnimationRenderer (Ring 3 -- Application)

```
void render_animation(scene, camera, animation_config, render_settings, physics_simulator, image_writer, progress_callback)
```

Orchestrates the full pipeline. Receives a PhysicsSimulator (injected, Ring 4 provides the implementation) and an ImageWriter (injected). This preserves the dependency rule -- Ring 3 defines interfaces, Ring 4 provides implementations.

### 5.4 Validator (Ring 4 -- Infrastructure)

```
ValidationResult validate(scene, animation_config_optional) -> { valid, checks[] }
```

Each check has category, passed/failed, and error list. Errors include object name, field, value, and suggested fix.

---

## 6. Integration with Existing Components

### 6.1 Components That Do NOT Change

| Component | Ring | Reason |
|---|---|---|
| Vec3, Point3, Color3, Ray, AABB | 1 | Extended by new math types, not modified |
| Shape, Sphere, Plane, Box, Triangle, TriangleMesh, Cylinder | 2 | TransformedShape wraps them; they are not modified |
| Material, Lambertian, Metal, Dielectric, Emissive | 2 | YAML loader constructs them; they are not modified |
| Light, PointLight, DirectionalLight | 2 | YAML loader constructs them; they are not modified |
| Camera | 2 | YAML loader constructs it; not modified |
| HitRecord | 2 | No changes needed |
| Renderer | 3 | AnimationRenderer calls it per frame; Renderer is not modified |
| PPMWriter | 4 | Used as-is for frame output |

### 6.2 Components That Change Minimally

| Component | Ring | Change |
|---|---|---|
| Scene | 2 | May need `replace_shapes()` or mutable shape access to swap in TransformedShapes. Minimal change to allow shape list modification after construction. |
| main.cpp | 4 | Refactored to use CLI dispatcher. Existing hardcoded scene becomes fallback when no YAML is provided. |
| src/CMakeLists.txt | 4 | Add new source files and Jolt/yaml-cpp link targets |
| Root CMakeLists.txt | - | Add FetchContent for Jolt Physics and yaml-cpp |

---

## 7. Architectural Decisions (ADRs)

### ADR-SPA-001: TransformedShape Wrapper vs Mutable Shapes

**Status**: Accepted

**Context**: Physics simulation produces new positions and rotations for dynamic objects each frame. The renderer needs shapes at their updated positions. Current shapes (Sphere, Box) store geometry as immutable construction parameters.

**Decision**: Use a TransformedShape decorator that wraps existing shapes with a Matrix4x4 transform. On `hit()`, transform the ray to local space, delegate, transform results back.

**Alternatives Considered**:
- Mutable shapes (add `set_position()`/`set_rotation()` to Shape interface): Requires modifying all 6 existing shape classes. Violates Open/Closed principle. Rotation support would need per-shape implementation.
- Shape cloning per frame (create new shapes each frame): High allocation cost for 150+ frames with dozens of shapes.

**Consequences**:
- Positive: Existing shapes untouched. Single transform implementation works for all shapes. Matrix4x4 handles both translation and rotation uniformly.
- Negative: One level of indirection in ray intersection (ray transform + result transform per hit test). Performance impact is proportional to hit count, not object count.

### ADR-SPA-002: Jolt Physics via CMake FetchContent

**Status**: Accepted

**Context**: The system needs rigid body physics (gravity, collisions, bouncing). Building a physics engine from scratch would take 3-6 weeks for a subset of Jolt's capabilities.

**Decision**: Integrate Jolt Physics via CMake FetchContent with `DOUBLE_PRECISION` enabled.

**Alternatives Considered**:
- ReactPhysics3D: Simpler API but lacks CylinderShape (needed for cylinder physics mapping) and double precision mode. ZLib license.
- Bullet Physics: Mature but aging C++ style (not modern C++17). Heavier integration burden. ZLib license.
- Custom minimal physics: 3-6 weeks for gravity + sphere/plane/box collisions + impulse solver. Educational but delays the feature significantly.

**Consequences**:
- Positive: Zero license cost (MIT). C++17 compatible. STL-only dependencies. Double precision matches ray tracer's `double` usage. All collision shapes needed (Sphere, Box, Cylinder, Plane, Mesh). Multi-threaded solver.
- Negative: ~50MB source download via FetchContent. Longer initial build time. Must implement Jolt's layer/filter callbacks (BroadPhaseLayerInterface, ObjectVsBroadPhaseLayerFilter, ObjectLayerPairFilter).

### ADR-SPA-003: yaml-cpp for YAML Parsing

**Status**: Accepted (extends ADR-004 from ray tracer architecture)

**Context**: YAML was selected as the scene file format (ADR-004). A parsing library is needed.

**Decision**: yaml-cpp via CMake FetchContent.

**Alternatives Considered**:
- rapidyaml (ryml): Faster parsing but less mature API and documentation. Scene files are small (parsed once), so speed is not a differentiator.
- nlohmann/json with JSON format: JSON lacks comments and is more verbose. YAML was the user-selected format.

**Consequences**:
- Positive: MIT license. Well-maintained. Extensive documentation. Widely adopted in C++ projects.
- Negative: Adds ~2MB to build. yaml-cpp uses exceptions for parse errors (must be caught and converted to validation errors).

### ADR-SPA-004: Decoupled Physics-Render Pipeline

**Status**: Accepted

**Context**: Physics simulation and ray tracing must cooperate. Two integration models exist: interleaved (step physics, render, repeat) or pre-baked (run all physics, then render all frames).

**Decision**: Interleaved frame-by-frame pipeline. For each frame: step physics to frame time, update transforms, render. This avoids storing transforms for all frames in memory.

**Alternatives Considered**:
- Pre-bake all physics transforms, then render: Requires O(frames * bodies) memory for stored transforms. For 150 frames * 100 bodies, this is manageable but unnecessary.
- Full pre-bake matches Blender's pattern, but Blender does it for determinism with user-interactive timeline scrubbing, which does not apply here.

**Consequences**:
- Positive: O(bodies) memory for transforms (only current frame). Simpler implementation. Progress can be shown frame-by-frame.
- Negative: Physics and render are serialized. If a future version parallelizes frame rendering, transforms would need to be pre-baked. Acceptable trade-off for single-threaded frame pipeline.

### ADR-SPA-005: CLI Argument Parsing Without External Library

**Status**: Accepted

**Context**: The CLI needs subcommands (validate, render) and ~10 flags. Options range from hand-rolled parsing to libraries like CLI11 or cxxopts.

**Decision**: Hand-rolled argument parser. The flag set is small and stable.

**Alternatives Considered**:
- CLI11 (header-only, BSD): Full-featured but adds a dependency for ~10 flags. Over-engineered for this scope.
- cxxopts (header-only, MIT): Lighter than CLI11 but still an external dependency.

**Consequences**:
- Positive: Zero dependencies. Full control over error messages. Trivial to maintain for a small flag set.
- Negative: No automatic help generation or type validation from the library. Must be manually maintained as flags grow. Acceptable for ~10 flags.

---

## 8. Deployment Architecture

The system remains a single statically-linked executable. Two new external libraries are added at build time.

```
+--------------------------------------------+
|  nwave (single executable)                  |
|                                            |
|  Statically links:                         |
|  - yaml-cpp (YAML parsing)       [NEW]    |
|  - Jolt Physics (rigid body sim)  [NEW]    |
|  - stb_image_write (PNG output)            |
|  - GoogleTest (test binary only)           |
|                                            |
|  Reads: scene.yaml  [NEW input]            |
|  Writes: output.ppm, frames/*.ppm [NEW]    |
+--------------------------------------------+
```

---

## 9. Cross-Cutting Concerns

### 9.1 Error Handling

- **YAML parse errors**: Caught from yaml-cpp exceptions. Converted to structured error with file name and line number.
- **Validation errors**: Collected in a single pass. Each error includes object name, field, value, and suggested fix. Edit-distance matching for misspelled material names.
- **Physics initialization errors**: Shape types that cannot map to collision shapes (e.g., dynamic TriangleMesh) caught at validation, not at physics init.
- **Runtime errors**: NaN guards on physics transforms before applying to shapes.

### 9.2 Testing Strategy (New Components)

| Component | Ring | Test Type |
|---|---|---|
| Matrix4x4, Quaternion | 1 | Unit: identity, inverse, transform point/vector/normal, from_quaternion, composition |
| PhysicsProperties, AnimationConfig | 2 | Unit: default values, derived computation (total_frames) |
| TransformedShape | 2 | Unit: translated sphere hit/miss, rotated box hit, normal transformation |
| PhysicsSimulator (interface) | 3 | N/A (abstract interface) |
| AnimationRenderer | 3 | Integration: mock PhysicsSimulator, verify frame count and transform application |
| JoltPhysicsSimulator | 4 | Integration: sphere falls onto plane, sphere-box collision, static body immobility |
| YamlSceneLoader | 4 | Integration: parse valid YAML, verify domain objects; parse invalid YAML, verify error messages |
| Validator | 4 | Unit: each validation check individually; Integration: full scene validation |
| CLI | 4 | Integration: subcommand routing, flag parsing, override application |

---

## 10. Traceability Matrix (New Requirements)

| Requirement / Story | Architecture Component | Ring |
|---|---|---|
| US-SPA-000: Walking skeleton (YAML to render) | YamlSceneLoader, Validator, CLI Dispatcher | 4 |
| US-SPA-010: Full shape coverage in YAML | YamlSceneLoader shape parsing branches | 4 |
| US-SPA-011: Scene validation | Validator | 4 |
| US-SPA-013: CLI subcommands | CLI Dispatcher | 4 |
| US-SPA-020: Physics engine integration | JoltPhysicsSimulator, PhysicsSimulator | 3, 4 |
| US-SPA-021: Shape-to-physics mapping | JoltPhysicsSimulator (adapter logic) | 4 |
| US-SPA-022: TransformedShape + Matrix4x4 | TransformedShape, Matrix4x4, Quaternion | 1, 2 |
| US-SPA-023: Physics props in YAML | YamlSceneLoader, PhysicsProperties | 2, 4 |
| US-SPA-030: Animation rendering loop | AnimationRenderer | 3 |
| US-SPA-050: nWave bowling demo | scenes/nwave_bowling.yaml (data artifact) | - |

---

## 11. Handoff Notes for Acceptance Designer

1. **Extend, do not replace**: All existing components remain unchanged. New components slot into the ring model additively.
2. **TransformedShape is the key bridge**: It connects physics output (position + rotation) to rendering input (ray intersection). It is the most architecturally significant new component.
3. **Three independent vertical slices**: YAML loading (US-SPA-000/010/011/013), physics integration (US-SPA-020/021/022), and animation pipeline (US-SPA-023/030) can be developed in partial parallel. YAML loading delivers value independently of physics.
4. **Ring boundary enforcement**: The crafter must verify that JoltPhysicsSimulator (Ring 4) is the ONLY component that includes Jolt headers. PhysicsSimulator interface (Ring 3) uses only nwave types.
5. **PhysicsProperties is pure data**: It carries no behavior and has no dependencies beyond Vec3. It belongs in Ring 2 alongside Shape and Material.
