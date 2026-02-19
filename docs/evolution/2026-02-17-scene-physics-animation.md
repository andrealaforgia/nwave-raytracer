# Evolution Record: scene-physics-animation

**Date**: 2026-02-17
**Feature ID**: scene-physics-animation
**Status**: Complete

---

## 1. Project Summary

This feature extended the nwave-raytracer with three capabilities: YAML-based scene loading, rigid body physics simulation via Jolt Physics, and animated frame rendering. A single CLI command (`nwave render scene.yaml --physics-animate`) now produces a sequence of PPM frames showing physics-driven object motion, suitable for assembly into video via ffmpeg.

### Scope Delivered

| Capability | Description |
|---|---|
| YAML scene loading | Declarative scene authoring with materials, shapes, lights, camera, physics, and animation sections |
| Scene validation | Pre-render integrity checking with edit-distance suggestions for misspelled material names |
| Jolt Physics integration | Rigid body dynamics (gravity, collisions, bouncing) with DOUBLE_PRECISION enabled |
| TransformedShape decorator | Physics-to-render bridge wrapping shapes with Matrix4x4 transforms |
| AnimationRenderer | Frame loop orchestrator stepping physics and rendering per frame |
| CLI subcommands | `nwave render` and `nwave validate` with `--physics-animate`, `--fps`, `--output-dir`, `--width`, `--spp` flags |
| Progress reporter | Frame counter, percentage, text progress bar, ETA display |
| Bowling demo scene | `scenes/nwave_bowling.yaml` with nWave letters, bowling ball, and physics animation |

### Deferred

| Item | Reason |
|---|---|
| US-SPA-024: Cylinder and TriangleMesh physics shape mapping | P2 priority. Not required for bowling demo. Sphere and Box mapping sufficient for all demo scenarios. |

---

## 2. Phases Completed

| Phase | Name | Steps | Description |
|---|---|---|---|
| 01 | Core Math Extensions | 01-01, 01-02 | Quaternion and Matrix4x4 types in Ring 1 |
| 02 | YAML Scene Loading (Walking Skeleton) | 02-01 to 02-04 | yaml-cpp integration, materials/shapes/lights/camera parsing, CLI render subcommand |
| 03 | Full Scene Loading | 03-01 to 03-04 | Box, cylinder, triangle, triangle_mesh parsing; directional light and emissive material; scene validator; CLI validate subcommand |
| 04 | Physics Foundation | 04-01 to 04-05 | PhysicsProperties struct, PhysicsSimulator interface, Jolt FetchContent, JoltPhysicsSimulator (sphere + box), sphere-box collision |
| 05 | Transform Pipeline | 05-01, 05-02 | TransformedShape with translation and rotation; normal transformation via inverse-transpose |
| 06 | Physics Scene Loading | 06-01 to 06-03 | AnimationConfig struct, YAML physics/animation parsing, validator physics checks |
| 07 | Animation Pipeline | 07-01 to 07-04 | AnimationRenderer frame loop, CLI --physics-animate wiring, multi-step physics per frame, --fps/--output-dir flag overrides |
| 08 | Bowling Demo and Polish | 08-01 to 08-03 | nwave_bowling.yaml demo scene, simple.yaml test scene, progress reporter with ETA |

**Total steps executed**: 27 (across 8 phases)
**All steps passed**: Yes

---

## 3. Key Architectural Decisions

### ADR-SPA-001: TransformedShape Wrapper vs Mutable Shapes

**Decision**: Use a TransformedShape decorator that wraps existing shapes with a Matrix4x4 transform. On `hit()`, the incoming ray is transformed to local space, delegated to the inner shape, and the result is transformed back to world space.

**Rationale**: Existing shapes (Sphere, Box, Plane, Cylinder, Triangle, TriangleMesh) remain unmodified, preserving the Open/Closed principle. A single transform implementation handles all shape types uniformly. The alternative of adding `set_position()`/`set_rotation()` to the Shape interface would have required modifying all 6 existing shape classes.

### ADR-SPA-002: Jolt Physics via CMake FetchContent with DOUBLE_PRECISION

**Decision**: Integrate Jolt Physics v5.2.0 via FetchContent with DOUBLE_PRECISION enabled.

**Rationale**: Jolt is C++17 compatible with zero external dependencies (STL only), matching the project's minimal dependency philosophy. Double precision aligns with the ray tracer's use of `double` throughout, avoiding float-to-double conversion noise. MIT license. Production-proven (Horizon Forbidden West, Death Stranding 2). All needed collision shapes available (Sphere, Box, Cylinder, Plane, Mesh).

**Rejected alternatives**: ReactPhysics3D (no CylinderShape, no double precision), Bullet Physics (aging C++ style), custom physics engine (3-6 week delay).

### ADR-SPA-003: yaml-cpp for YAML Parsing

**Decision**: yaml-cpp 0.8.x via FetchContent. MIT license, well-maintained, no external dependencies.

**Rejected alternatives**: rapidyaml (less mature API, speed irrelevant for single-parse scene files), nlohmann/json (JSON lacks comments, more verbose than YAML).

### ADR-SPA-004: Interleaved Physics-Render Pipeline

**Decision**: Frame-by-frame interleaved pipeline. For each frame: step physics to frame time, update TransformedShape transforms, render, write.

**Rationale**: O(bodies) memory for transforms (current frame only). Simpler implementation. Pre-baking all transforms would require O(frames x bodies) storage with no benefit for single-threaded frame rendering.

### ADR-SPA-005: Hand-rolled CLI Argument Parser

**Decision**: No external CLI library. The flag set is small (~10 flags) and stable.

**Rejected alternatives**: CLI11 and cxxopts (both add a dependency for trivial scope).

### Ring Boundary Enforcement

All Jolt Physics headers are confined to Ring 4 (`src/infrastructure/jolt_physics_simulator.h/.cpp`). The PhysicsSimulator interface in Ring 3 uses only nwave types (Vec3, Point3, Quaternion). AnimationRenderer in Ring 3 depends on the interface, not the implementation. If Jolt were replaced with another physics engine, only the Ring 4 adapter would change.

---

## 4. Test Statistics

| Category | Count |
|---|---|
| **Total tests** | **243** |
| All passing | Yes |
| Pre-existing tests (before feature) | 131 |
| New tests (this feature) | 112 |

### New Test Breakdown

| Component | Ring | Tests |
|---|---|---|
| Quaternion (`tests/core/quaternion_test.cpp`) | 1 | 18 |
| Matrix4x4 (`tests/core/matrix4x4_test.cpp`) | 1 | (included in 18 above -- combined core math) |
| PhysicsProperties (`tests/domain/physics_properties_test.cpp`) | 2 | 8 |
| AnimationConfig (`tests/domain/animation_config_test.cpp`) | 2 | (included in 8 above) |
| TransformedShape (`tests/domain/transformed_shape_test.cpp`) | 2 | 6 |
| AnimationRenderer (`tests/application/animation_renderer_test.cpp`) | 3 | 14 |
| YamlSceneLoader (`tests/infrastructure/yaml_scene_loader_test.cpp`) | 4 | 24 |
| SceneValidator (`tests/infrastructure/validator_test.cpp`) | 4 | 16 |
| CliDispatcher (`tests/infrastructure/cli_dispatcher_test.cpp`) | 4 | 15 |
| JoltPhysicsSimulator (`tests/infrastructure/jolt_physics_simulator_test.cpp`) | 4 | 7 |
| ProgressReporter (`tests/infrastructure/progress_reporter_test.cpp`) | 4 | 4 |

### Mutation Testing

Skipped. No C++ mutation testing tool (mull, dextool mutate) is installed or configured. Per quality gate policy, this is an accepted skip condition for C++ projects. See `docs/feature/scene-physics-animation/mutation/mutation-report.md` for details and future recommendations.

---

## 5. TDD Execution Summary

All 27 steps followed the TDD cycle: PREPARE, RED_ACCEPTANCE, RED_UNIT, GREEN, REVIEW, REFACTOR_CONTINUOUS, COMMIT.

Notable variations from the standard cycle:

| Step | Variation | Reason |
|---|---|---|
| 02-01 (yaml-cpp FetchContent) | RED phases skipped | Build-only step, no test code |
| 04-02 (PhysicsSimulator interface) | RED phases skipped | Interface definition, no implementation to test |
| 04-03 (Jolt FetchContent) | RED phases skipped | Build-only step, no test code |
| 05-02 (TransformedShape rotation) | RED_UNIT skipped | Implementation from 05-01 was already general (Matrix4x4 handles rotation natively) |
| 07-03 (Multi-step physics) | GREEN skipped | Implementation already existed from 07-01 (AnimationRenderer already computed steps_per_frame) |
| 08-01 (Bowling demo YAML) | RED phases skipped | Data file, no test code |
| 08-02 (Simple test scene YAML) | RED phases skipped | Data file, no test code |

L1-L4 refactoring was performed across all steps during the REFACTOR_CONTINUOUS phase.

---

## 6. Files Produced

### Production Code (New)

| File | Ring | Purpose |
|---|---|---|
| `src/core/quaternion.h` | 1 | Unit quaternion for 3D rotation |
| `src/core/matrix4x4.h` | 1 | 4x4 transformation matrix |
| `src/domain/physics_properties.h` | 2 | Body type, mass, friction, restitution struct |
| `src/domain/animation_config.h` | 2 | Duration, timestep, FPS, derived frame counts |
| `src/domain/shapes/transformed_shape.h` | 2 | Shape decorator with Matrix4x4 transform |
| `src/domain/shapes/transformed_shape.cpp` | 2 | TransformedShape hit/AABB implementation |
| `src/application/physics_simulator.h` | 3 | Abstract physics simulator interface |
| `src/application/animation_renderer.h` | 3 | Animation orchestrator interface |
| `src/application/animation_renderer.cpp` | 3 | Frame loop implementation |
| `src/infrastructure/yaml_scene_loader.h` | 4 | YAML scene file parser |
| `src/infrastructure/yaml_scene_loader.cpp` | 4 | YAML parsing implementation |
| `src/infrastructure/validator.h` | 4 | Scene validation engine |
| `src/infrastructure/validator.cpp` | 4 | Validation checks implementation |
| `src/infrastructure/jolt_physics_simulator.h` | 4 | Jolt Physics adapter |
| `src/infrastructure/jolt_physics_simulator.cpp` | 4 | Jolt initialization, body management, stepping |
| `src/infrastructure/cli_dispatcher.h` | 4 | CLI subcommand/flag parser |
| `src/infrastructure/cli_dispatcher.cpp` | 4 | Subcommand routing implementation |
| `src/infrastructure/progress_reporter.h` | 4 | Render progress display |
| `src/infrastructure/progress_reporter.cpp` | 4 | Progress bar, ETA, frame counter |

### Scene Files (New)

| File | Purpose |
|---|---|
| `scenes/nwave_bowling.yaml` | Full bowling demo with physics animation |
| `scenes/simple.yaml` | Minimal scene for fast integration testing |

### Modified Files

| File | Change |
|---|---|
| `CMakeLists.txt` | FetchContent for Jolt Physics and yaml-cpp |
| `src/CMakeLists.txt` | New source files and link targets |
| `src/main.cpp` | Refactored to use CLI dispatcher |

---

## 7. Lessons Learned

### What went well

1. **Walking skeleton first (Phase 02)** delivered end-to-end value early. YAML-to-render worked before physics was introduced, providing a stable foundation to build on.

2. **TransformedShape as architectural bridge** proved to be the correct abstraction. A single decorator handled all shape types for both translation and rotation without modifying any existing shapes.

3. **PhysicsSimulator interface isolation** kept Jolt Physics fully encapsulated in Ring 4. The Ring 3 AnimationRenderer has zero knowledge of Jolt. This was validated when 05-02 (rotation) discovered that the Matrix4x4 implementation from 05-01 already handled rotation generically -- no Jolt-specific code was needed in the transform pipeline.

4. **Dependency-ordered execution** allowed steps to be executed out of phase order when dependencies permitted (e.g., 06-01 and 08-02 were executed before Phase 04 completion because they had no physics dependencies).

### What could be improved

1. **CMakeLists.txt updates were implicit**. The roadmap review (R6, severity HIGH) identified that individual steps did not explicitly list `src/CMakeLists.txt` as a modified file. In practice, CMake updates were bundled with each step's GREEN phase, but the roadmap should have made this explicit to avoid ambiguity.

2. **Execution log entries lost during context compaction**. Steps 03-04, 04-05, 05-01, 06-03, and 08-03 were executed successfully but their log entries were lost when conversation context was compacted. For long multi-session features, consider persisting the execution log to disk after each step commit rather than only at session boundaries.

3. **Mutation testing gap**. No C++ mutation testing tool was available. For future C++ features, evaluating mull (LLVM-based) or dextool mutate during project setup would close this quality gap.

---

## 8. User Stories Covered

| Story ID | Title | Status |
|---|---|---|
| US-SPA-000 | Walking skeleton (YAML to render) | Delivered |
| US-SPA-010 | Full shape coverage in YAML | Delivered |
| US-SPA-011 | Scene validation with actionable errors | Delivered |
| US-SPA-012 | Directional light and emissive material | Delivered |
| US-SPA-013 | CLI subcommands (render, validate) | Delivered |
| US-SPA-020 | Jolt Physics engine integration | Delivered |
| US-SPA-021 | Shape-to-physics mapping (sphere, box) | Delivered |
| US-SPA-022 | TransformedShape with Matrix4x4/Quaternion | Delivered |
| US-SPA-023 | Physics properties in YAML | Delivered |
| US-SPA-030 | Animation rendering loop | Delivered |
| US-SPA-031 | Multiple physics steps per render frame | Delivered |
| US-SPA-032 | CLI flag overrides for animation | Delivered |
| US-SPA-050 | nWave bowling demo scene | Delivered |
| US-SPA-051 | Progress reporter with ETA | Delivered |
| US-SPA-024 | Cylinder/TriangleMesh physics mapping | Deferred (P2) |

**14 of 15 user stories delivered. 1 deferred (P2).**

---

## 9. Reference Documents

All feature documents are preserved in `docs/feature/scene-physics-animation/` for reference:

- `roadmap.yaml` -- 27-step execution roadmap with dependency graph
- `execution-log.yaml` -- TDD cycle log (22 of 27 steps logged; 5 lost to context compaction)
- `mutation/mutation-report.md` -- Mutation testing skip justification
- `discuss/` -- Requirements, user stories, acceptance criteria, UX journey, DOR checklist
- `design/` -- Architecture design, technology stack, component boundaries, data models, sequence/flow diagrams
