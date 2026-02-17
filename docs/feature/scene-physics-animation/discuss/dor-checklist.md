# Definition of Ready Checklist: Scene Physics Animation

**Document ID**: DOR-SPA-001
**Feature**: scene-physics-animation
**Date**: 2026-02-17
**Status**: All stories PASS -- Ready for DESIGN wave

---

## Checklist Summary

| # | DoR Item | US-000 | US-010 | US-011 | US-012 | US-013 | US-020 | US-021 | US-022 | US-023 | US-024 | US-030 | US-031 | US-032 | US-050 | US-051 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Problem statement clear | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| 2 | User/persona identified | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| 3 | 3+ domain examples | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| 4 | UAT scenarios (3-7) | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| 5 | AC derived from UAT | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| 6 | Right-sized (1-3 days) | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| 7 | Technical notes | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| 8 | Dependencies resolved/tracked | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |

**Result**: All 15 stories pass all 8 DoR items. Ready for handoff to DESIGN wave.

---

## Per-Story Evidence

### US-SPA-000: Walking Skeleton -- YAML Scene to Rendered Frame

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Every scene is hardcoded in main.cpp; must edit C++, recompile, and relink to see any scene change -- 30-60 second recompile cycle kills creative iteration." Domain language, no technical solution prescribed. |
| 2 | User/persona identified | PASS | Andrea, C++ developer, works from CLI on macOS, wants fast iteration via text file changes. |
| 3 | 3+ domain examples | PASS | 3 examples: (1) minimal scene renders correctly, (2) material reference resolution, (3) missing material reference caught. All use real data values and specific material names. |
| 4 | UAT scenarios (3-7) | PASS | 5 scenarios: minimal render, material resolution, unknown material error, camera parameters, CLI width override. All Given/When/Then. |
| 5 | AC derived from UAT | PASS | 5 acceptance criteria map to the 5 UAT scenarios. Each is checkable. |
| 6 | Right-sized (1-3 days) | PASS | Scope: YAML parser for 3 shape types + 3 material types + 1 light type + camera. yaml-cpp integration. Estimated 2-3 days. |
| 7 | Technical notes | PASS | Notes: yaml-cpp via FetchContent, SceneLoader in Ring 4, minimum shape/material types listed, no physics dependency. |
| 8 | Dependencies resolved/tracked | PASS | Dependency: yaml-cpp library. Tracked -- must be added to CMakeLists.txt via FetchContent. No blocking unresolved dependencies. |

### US-SPA-010: Full Shape Type Coverage in YAML Loader

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "YAML loader only handles spheres and planes but the ray tracer supports six shape types. Box-based nWave letters cannot load from YAML." |
| 2 | User/persona identified | PASS | Same persona (Andrea) with specific need: nWave letter blocks require boxes. |
| 3 | 3+ domain examples | PASS | 3 examples: box for nWave letters, cylinder for pillar, unsupported type error. Real dimensions and material names. |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: box loads, cylinder loads, triangle loads, unknown type error. |
| 5 | AC derived from UAT | PASS | 3 AC items covering all shape types, rendering correctness, and error handling. |
| 6 | Right-sized (1-3 days) | PASS | Adds parsing branches to existing SceneLoader. No new domain types. Estimated 1 day. |
| 7 | Technical notes | PASS | Extends SceneLoader only (Ring 4). All shape types already exist in Ring 2. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-000 (SceneLoader exists). No external dependencies. |

### US-SPA-011: Scene Validation with Actionable Error Messages

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Complex YAML with 26 objects; misspelled material and negative mass surface as crashes deep in pipeline, wasting minutes." |
| 2 | User/persona identified | PASS | Developer authoring complex scenes, needs pre-render error checking. |
| 3 | 3+ domain examples | PASS | 3 examples: checklist pass, multiple errors at once, suggested fix for typo. All with specific object names and values. |
| 4 | UAT scenarios (3-7) | PASS | 5 scenarios: valid pass, negative mass, multiple errors, name suggestion, missing camera. |
| 5 | AC derived from UAT | PASS | 6 AC items covering all validation aspects. |
| 6 | Right-sized (1-3 days) | PASS | Validation logic + edit distance for suggestions. Estimated 1-2 days. |
| 7 | Technical notes | PASS | Validator in Ring 4, edit distance for suggestions, runs before render implicitly. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-000 (scene loading). No external dependencies. |

### US-SPA-012: Directional Light and Emissive Material in YAML

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "YAML loader handles point lights only; scenes needing sunlight or glowing objects still require C++ edits." |
| 2 | User/persona identified | PASS | Developer creating outdoor scenes (sunlight) or scenes with neon/glowing elements. |
| 3 | 3+ domain examples | PASS | 3 examples: directional light, emissive material, mixed light types. Real YAML values. |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: directional light loads, emissive loads, mixed types render. |
| 5 | AC derived from UAT | PASS | 3 AC items mapping to scenarios. |
| 6 | Right-sized (1-3 days) | PASS | Two additional parsing branches in SceneLoader. Estimated half a day. |
| 7 | Technical notes | PASS | SceneLoader extension only; domain types already exist. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-000. No external dependencies. |

### US-SPA-013: CLI Subcommand Structure

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Current executable has a single code path with a simple boolean flag. YAML loading and physics need clear subcommands." |
| 2 | User/persona identified | PASS | Developer wanting validate-before-render workflow and CLI parameter overrides. |
| 3 | 3+ domain examples | PASS | 3 examples: validate subcommand, render with overrides, help text. |
| 4 | UAT scenarios (3-7) | PASS | 5 scenarios: validate dispatches, render produces image, width override, unknown subcommand, help flag. |
| 5 | AC derived from UAT | PASS | 6 AC items covering all CLI behaviors. |
| 6 | Right-sized (1-3 days) | PASS | CLI argument parser with ~10 flags and 2 subcommands. Estimated 1 day. |
| 7 | Technical notes | PASS | Ring 4, hand-rolled or lightweight parser, --physics-animate parsed but wired later. |
| 8 | Dependencies resolved/tracked | PASS | No external dependencies. Parallel with US-SPA-000. |

### US-SPA-020: Physics Engine Integration -- Sphere Falls onto Plane

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Every object is fixed at its initial position forever. To simulate a ball falling, Andrea would need to hand-compute positions for each frame." |
| 2 | User/persona identified | PASS | Developer wanting automatic physics: define initial conditions, engine does the rest. |
| 3 | 3+ domain examples | PASS | 3 examples: sphere falls under gravity (with computed position), sphere bounces with restitution, static plane doesn't move. Real numbers. |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: falls under gravity, bounces, static stays, simulator resets. |
| 5 | AC derived from UAT | PASS | 6 AC items covering compilation, interface, gravity, bounce, static, and reset. |
| 6 | Right-sized (1-3 days) | PASS | Jolt FetchContent + interface + adapter + sphere/plane test. Estimated 2-3 days. |
| 7 | Technical notes | PASS | Jolt via FetchContent with JPH_DOUBLE_PRECISION. PhysicsProperties in Ring 2. Interface in Ring 3. Adapter in Ring 4. No rendering in this story. |
| 8 | Dependencies resolved/tracked | PASS | Dependency: Jolt Physics library. Tracked -- added via FetchContent. Research confirms compatibility (C++17, MIT, STL-only). |

### US-SPA-021: Shape-to-Physics-Body Mapping

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Only handles spheres with hardcoded collision shapes. Scenes with boxes need box-to-BoxShape mapping." |
| 2 | User/persona identified | PASS | Developer with mixed shape scenes needing physical interaction. |
| 3 | 3+ domain examples | PASS | 3 examples: sphere to SphereShape, box min/max to center+half-extents, plane to static. Real coordinates. |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: sphere shape, box conversion, sphere-box collision. |
| 5 | AC derived from UAT | PASS | 4 AC items covering sphere, box, plane, and collision plausibility. |
| 6 | Right-sized (1-3 days) | PASS | Three shape mappings in adapter code. Estimated 1-2 days. |
| 7 | Technical notes | PASS | All in JoltPhysicsSimulator (Ring 4). Cylinder/mesh deferred to US-SPA-024. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-020 (physics engine integrated). |

### US-SPA-022: TransformedShape -- Apply Physics Transforms to Render Shapes

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Shape classes store geometry as immutable construction parameters. No way to move a shape after construction." |
| 2 | User/persona identified | PASS | Developer needing rendered frames to show objects at physics-computed positions and rotations. |
| 3 | 3+ domain examples | PASS | 3 examples: translation-only, rotation on box, normal transformation. Specific coordinates and angles. |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: translated hit, original miss, rotated intersection, normal correctness. |
| 5 | AC derived from UAT | PASS | 6 AC items covering Matrix4x4, TransformedShape, ray transform, normals. |
| 6 | Right-sized (1-3 days) | PASS | Matrix4x4 (Ring 1) + TransformedShape (Ring 2). Architecturally complex but bounded scope. Estimated 2-3 days. |
| 7 | Technical notes | PASS | Matrix4x4 and Quaternion in Ring 1. TransformedShape in Ring 2 (implements Shape). Existing shapes unmodified. |
| 8 | Dependencies resolved/tracked | PASS | No external dependencies. Knowledge gap (Matrix4x4 implementation) addressed as implementation scope within this story. |

### US-SPA-023: YAML Physics Properties Parsing

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Physics properties only settable in C++ test code. Andrea needs them in YAML for text-file iteration." |
| 2 | User/persona identified | PASS | Developer iterating on physics parameters by editing YAML. |
| 3 | 3+ domain examples | PASS | 3 examples: dynamic ball with velocity, default static without block, animation section. Real YAML with real values. |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: parse physics, default static, animation config, invalid values. |
| 5 | AC derived from UAT | PASS | 5 AC items mapping to scenarios. |
| 6 | Right-sized (1-3 days) | PASS | Extends SceneLoader with physics block and animation section parsing. Estimated 1 day. |
| 7 | Technical notes | PASS | Extends SceneLoader (Ring 4). PhysicsProperties struct from US-SPA-020. AnimationConfig struct. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-000 (SceneLoader) and US-SPA-020 (PhysicsProperties struct). |

### US-SPA-024: Cylinder and TriangleMesh Physics Mapping

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Sphere/box/plane mapping doesn't cover cylinders and meshes; these pass through physics unaffected." |
| 2 | User/persona identified | PASS | Developer with cylindrical objects or imported mesh geometry in physics scenes. |
| 3 | 3+ domain examples | PASS | 3 examples: rolling cylinder, static mesh ramp, dynamic mesh rejection. |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: cylinder body, static mesh, dynamic mesh error. |
| 5 | AC derived from UAT | PASS | 3 AC items mapping to scenarios. |
| 6 | Right-sized (1-3 days) | PASS | Two additional shape mapping branches. Estimated 1 day. |
| 7 | Technical notes | PASS | Extends JoltPhysicsSimulator (Ring 4). Concave mesh static-only constraint documented. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-021 (base shape mapping). |

### US-SPA-030: Animation Rendering Loop

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "All pieces exist but no orchestration connecting them. Andrea would need to manually wire steps in main.cpp for each scene." |
| 2 | User/persona identified | PASS | Developer wanting single-command animation: `nwave render scene.yaml --physics-animate`. |
| 3 | 3+ domain examples | PASS | 3 examples: bouncing ball 3s animation, progress feedback, physics summary. Real frame counts and durations. |
| 4 | UAT scenarios (3-7) | PASS | 5 scenarios: frame files produced, objects move, frame count matches, physics summary, ffmpeg command. |
| 5 | AC derived from UAT | PASS | 7 AC items covering the complete pipeline. |
| 6 | Right-sized (1-3 days) | PASS | AnimationRenderer orchestration class wiring existing components. Estimated 2-3 days. |
| 7 | Technical notes | PASS | AnimationRenderer in Ring 3. Auto-create output directory. Steps per frame formula documented. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-021 (shape mapping), US-SPA-022 (TransformedShape), US-SPA-023 (YAML physics). All tracked. |

### US-SPA-031: Multiple Physics Steps Per Render Frame

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Physics at 60 Hz but rendering at 30 fps. Single step per frame at 30 Hz causes tunneling and inaccurate collisions." |
| 2 | User/persona identified | PASS | Developer wanting accurate 60 Hz physics with 30 fps output. |
| 3 | 3+ domain examples | PASS | 3 examples: 2 steps/frame, 4 steps/frame, non-integer ratio with interpolation. Specific Hz and fps values. |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: 2 steps/frame, total time matches, higher rate same frame count. |
| 5 | AC derived from UAT | PASS | 3 AC items mapping to scenarios. |
| 6 | Right-sized (1-3 days) | PASS | Loop modification in AnimationRenderer. Estimated 1 day. |
| 7 | Technical notes | PASS | Accumulator pattern reference. Interpolation for non-integer ratios. Integer ratios sufficient for MVP. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-030 (animation loop). |

### US-SPA-032: CLI Flag Overrides for Animation Parameters

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "Must edit YAML each time to change width, SPP, or fps. Wants CLI overrides for rapid iteration." |
| 2 | User/persona identified | PASS | Developer switching between fast preview and final quality. |
| 3 | 3+ domain examples | PASS | 3 examples: fast preview (400px, 4 SPP), high quality (1920px, 48 SPP), output dir override. |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: width override, fps override, output dir override. |
| 5 | AC derived from UAT | PASS | 5 AC items covering all override flags and precedence. |
| 6 | Right-sized (1-3 days) | PASS | Extends CLI parsing and adds merge logic. Estimated half a day. |
| 7 | Technical notes | PASS | Extends US-SPA-013 CLI. Merge logic: CLI > YAML > default. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-013 (CLI) and US-SPA-030 (animation loop). |

### US-SPA-050: NWave Bowling Demo Scene

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "No showcase scene demonstrates the full capability." |
| 2 | User/persona identified | PASS | Developer wanting a visually impressive demo exercising all capabilities. |
| 3 | 3+ domain examples | PASS | 3 examples: full scene description, rendered video timeline, clean validation. Real block counts and physics values. |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: validates clean, produces 150 frames, correct dynamic/static split. |
| 5 | AC derived from UAT | PASS | 3 AC items mapping to scenarios. |
| 6 | Right-sized (1-3 days) | PASS | YAML file authoring (data, not code). Estimated 1 day. |
| 7 | Technical notes | PASS | Content/data story. Block positions from existing LETTER_* bitmaps. Integration test role. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-030 (animation loop). |

### US-SPA-051: Render Progress Bar with ETA

| # | DoR Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement clear | PASS | "No output for 12 minutes. Doesn't know if system is working or stuck." |
| 2 | User/persona identified | PASS | Developer waiting for long multi-frame renders. |
| 3 | 3+ domain examples | PASS | 3 examples: early progress, mid-render, completion. Specific frame counts and ETAs. |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: frame count/percentage, ETA decreases, completion time. |
| 5 | AC derived from UAT | PASS | 5 AC items covering counter, percentage, bar, ETA, elapsed. |
| 6 | Right-sized (1-3 days) | PASS | Progress reporter class. Estimated half a day. |
| 7 | Technical notes | PASS | Ring 4 ProgressReporter. Carriage return updates. Running average for ETA. |
| 8 | Dependencies resolved/tracked | PASS | Depends on US-SPA-030 (animation loop provides per-frame hook). |

---

## Recommended Implementation Order

```
Phase 1 (Foundation):       US-SPA-000, US-SPA-013     [YAML loading + CLI]
Phase 2 (Scene Coverage):   US-SPA-010, US-SPA-011     [All shapes + validation]
Phase 3 (Physics Core):     US-SPA-020, US-SPA-021     [Jolt integration + shape mapping]
Phase 4 (Transform Bridge): US-SPA-022                  [Matrix4x4 + TransformedShape]
Phase 5 (YAML Physics):     US-SPA-023                  [Physics props in YAML]
Phase 6 (Animation):        US-SPA-030, US-SPA-031     [Animation loop + multi-step]
Phase 7 (Polish):           US-SPA-012, US-SPA-032, US-SPA-050, US-SPA-051  [Extras + demo]
Phase 8 (Extended):         US-SPA-024                  [Cylinder + mesh physics]
```

Phases 1-2 and Phase 3 can proceed in parallel (no dependencies between YAML loading and physics engine integration).
