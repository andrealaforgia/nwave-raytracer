# Definition of Ready Checklist: Soft Body Jelly Physics

**Feature**: Deformable jelly cube with physics-driven animation
**Date**: 2026-02-18
**Validation**: Each story is checked against 8 DoR items. All must PASS for DESIGN wave handoff.

---

## DoR Items

| # | Item | Description |
|---|---|---|
| 1 | Problem statement clear and in domain language | Pain point described using business/user terms, not technical implementation |
| 2 | User/persona identified with specific characteristics | Named persona with role, context, and motivation |
| 3 | At least 3 domain examples with real data | Concrete examples using real names, real values, real scenarios |
| 4 | UAT scenarios in Given/When/Then (3-7 scenarios) | Testable BDD scenarios covering happy path, edge cases, error cases |
| 5 | Acceptance criteria derived from UAT | Checkable outcomes mapped to scenarios |
| 6 | Story right-sized (1-3 days, 3-7 scenarios) | Deliverable in a single sprint increment |
| 7 | Technical notes identify constraints and dependencies | Known constraints, integration points, risks |
| 8 | Dependencies resolved or tracked | Upstream dependencies identified and either completed or planned |

---

## US-01: Soft Body Domain Types

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "Marco Benedetti has no way to describe a soft body in the domain model" -- domain language, no implementation detail |
| 2 | User/persona | PASS | Marco Benedetti, technical artist building physics-animated scenes; developer extending the physics pipeline |
| 3 | Domain examples (3+) | PASS | 3 examples: Marco's jelly cube description, extracting deformed mesh after collision, distinguishing soft from rigid bodies |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: SoftBodyDesc holds parameters, SoftBodyMeshData holds mesh, BodyType includes SOFT |
| 5 | Acceptance criteria | PASS | 5 checkable items derived from scenarios (struct fields, defaults, enum value, backward compat) |
| 6 | Right-sized | PASS | Size S (< 1 day), 3 scenarios |
| 7 | Technical notes | PASS | Domain ring location specified, no Jolt dependencies in domain types |
| 8 | Dependencies | PASS | No dependencies |

**Result: PASS -- ready for DESIGN wave**

---

## US-02: PhysicsSimulator Soft Body API

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "Interface only knows add_body() for rigid bodies and get_transform() for position/rotation" -- describes the gap in domain terms |
| 2 | User/persona | PASS | Developer implementing the animation pipeline; Dependency Inversion Principle motivation |
| 3 | Domain examples (3+) | PASS | 3 examples: adding Marco's jelly cube (body ID 3), querying body type in render loop, extracting mesh at frame 15 |
| 4 | UAT scenarios (3-7) | PASS | 3 scenarios: add returns valid ID, is_soft_body distinguishes types, get_mesh returns current positions |
| 5 | Acceptance criteria | PASS | 5 checkable items: method signatures, unchanged existing methods, compilation |
| 6 | Right-sized | PASS | Size S (< 1 day), 3 scenarios |
| 7 | Technical notes | PASS | Application ring location, mock update impact noted |
| 8 | Dependencies | PASS | US-01 (domain types) -- tracked and preceding in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-03: JoltPhysicsSimulator Soft Body Creation

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "JoltPhysicsSimulator only knows how to create rigid bodies" -- describes capability gap in domain terms |
| 2 | User/persona | PASS | Developer implementing Jolt integration; motivated by getting first soft body running |
| 3 | Domain examples (3+) | PASS | 3 examples: Marco's 5x5x5 (125 vertices, 300 edges, 384 volume constraints), Sofia's stiffer 8x8x8, minimal 3x3x3 for testing |
| 4 | UAT scenarios (3-7) | PASS | 5 scenarios: creation, gravity response, static floor collision, dynamic body interaction, parameter variation |
| 5 | Acceptance criteria | PASS | 6 checkable items: vertex grid creation, gravity, static/dynamic collision, parameter application, is_soft_body |
| 6 | Right-sized | PASS | Size M (2-3 days), 5 scenarios |
| 7 | Technical notes | PASS | Reference to SoftBodyCreator::CreateCube, collision layer concern flagged, grid spacing formula |
| 8 | Dependencies | PASS | US-01, US-02 -- both tracked and preceding in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-04: JoltPhysicsSimulator Soft Body Mesh Extraction

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "Deformed vertex positions are locked inside Jolt's SoftBodyMotionProperties" -- describes the data access gap |
| 2 | User/persona | PASS | Developer connecting physics output to rendering input |
| 3 | Domain examples (3+) | PASS | 3 examples: mesh at rest (125 vertices, 576 face indices), mesh after collision (positions differ), world-space transform |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: correct vertex count, world-space positions, deformation reflected, consistent winding order |
| 5 | Acceptance criteria | PASS | 5 checkable items: vertex count, world-space transform, flat face index vector, constant topology, position changes |
| 6 | Right-sized | PASS | Size S (1 day), 4 scenarios |
| 7 | Technical notes | PASS | BodyLockRead requirement, transform formula, face count formula |
| 8 | Dependencies | PASS | US-03 -- tracked and preceding in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-05: DeformableMesh Shape (Ray Intersection)

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "No shape that can represent deforming geometry; TriangleMesh never changes; TransformedShape only applies rigid transform" |
| 2 | User/persona | PASS | Developer building the rendering side of soft body support |
| 3 | Domain examples (3+) | PASS | 3 examples: intersecting at rest (hits face 47), intersecting after deformation (different face), ray misses deformed AABB |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: ray hits, ray misses, AABB rejection, smooth normal interpolation |
| 5 | Acceptance criteria | PASS | 5 checkable items: hit() with Moller-Trumbore, hit record fields, smooth normals, AABB, material |
| 6 | Right-sized | PASS | Size M (1-2 days), 4 scenarios |
| 7 | Technical notes | PASS | Domain ring location, same algorithm as TriangleMesh, constructor design |
| 8 | Dependencies | PASS | No dependencies (parallel development possible) |

**Result: PASS -- ready for DESIGN wave**

---

## US-06: DeformableMesh Vertex Update and Normal Recomputation

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "Mesh needs to accept new vertex positions from physics and recompute normals and AABB" |
| 2 | User/persona | PASS | Developer connecting per-frame physics output to rendering mesh |
| 3 | Domain examples (3+) | PASS | 3 examples: normal recomputation after floor impact, AABB shrinks after compression, consistent winding after deformation |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: vertices replaced, normals recomputed, AABB recomputed, area-weighted averaging |
| 5 | Acceptance criteria | PASS | 5 checkable items: vertex replacement, normal recomputation, AABB recomputation, hit() uses new data, vertex count validation |
| 6 | Right-sized | PASS | Size S (1 day), 4 scenarios |
| 7 | Technical notes | PASS | Area-weighted normal formula, adjacency precomputation, performance note |
| 8 | Dependencies | PASS | US-05 -- tracked and preceding in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-07: AnimationRenderer Soft Body Loop

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "No code path that extracts deformed mesh data from physics and feeds it into DeformableMesh" |
| 2 | User/persona | PASS | Technical artist running `nwave render --physics-animate`; expects rendered frames to show deformation |
| 3 | Domain examples (3+) | PASS | 3 examples: frame 15 mesh update, mixed rigid+soft scene, 150-frame animation |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: mesh updated each frame, mixed rigid/soft updates, soft body identification, progressive deformation in output |
| 5 | Acceptance criteria | PASS | 5 checkable items: soft body detection, per-frame update sequence, rigid behavior unchanged, output format, body-to-shape mapping |
| 6 | Right-sized | PASS | Size M (1-2 days), 4 scenarios |
| 7 | Technical notes | PASS | Dynamic cast or tracking structure, sequential pipeline preserved, no threading changes |
| 8 | Dependencies | PASS | US-02, US-04, US-06 -- all tracked in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-08: YAML Parsing for soft_body_cube

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "YamlSceneLoader does not recognize soft_body_cube or body_type soft; Marco cannot author a soft body scene" |
| 2 | User/persona | PASS | Technical artist authoring scenes in YAML; expects new types to follow existing patterns |
| 3 | Domain examples (3+) | PASS | 3 examples: full parameter YAML, default-parameter YAML (Sofia), invalid configuration caught |
| 4 | UAT scenarios (3-7) | PASS | 5 scenarios: full params, defaults, body_type soft, backward compatibility, missing field error |
| 5 | Acceptance criteria | PASS | 6 checkable items: type recognition, body_type parsing, defaults, backward compat, error messages, SoftBodyDesc in result |
| 6 | Right-sized | PASS | Size M (1-2 days), 5 scenarios |
| 7 | Technical notes | PASS | SceneLoadResult extension, initial geometry at origin, type-specific fields |
| 8 | Dependencies | PASS | US-01, US-05 -- both tracked in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-09: YAML Parsing for letter Object Type

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "Only way to get a letter shape is manually composing 12 boxes; tedious, blocky, not suitable for physics" |
| 2 | User/persona | PASS | Technical artist wanting a single YAML entry for a smooth 3D letter |
| 3 | Domain examples (3+) | PASS | 3 examples: Marco's 'e' with physics, Sofia's static 'S', custom font path |
| 4 | UAT scenarios (3-7) | PASS | 5 scenarios: mesh generation, physics body, no-physics letter, missing field error, unsupported glyph error |
| 5 | Acceptance criteria | PASS | 6 checkable items: type recognition, closed mesh, counter handling, optional physics, error messages, default font |
| 6 | Right-sized | PASS | Size M (1-2 days), 5 scenarios |
| 7 | Technical notes | PASS | Dependencies on US-10/US-11, one-time generation, TransformedShape wrapper, ASCII scope |
| 8 | Dependencies | PASS | US-10, US-11 -- both tracked in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-10: Font Glyph to 3D Mesh Generation

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "No capability to generate 3D geometry from font glyphs; letters are manually positioned boxes" |
| 2 | User/persona | PASS | Developer building the font-to-mesh pipeline |
| 3 | Domain examples (3+) | PASS | 3 examples: 'e' letter (400-600 triangles, with hole), 'W' (no holes, 200-300 triangles), 'o' (circular hole) |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: 'e' with correct dimensions, interior contour handled, centered at origin, normals correct |
| 5 | Acceptance criteria | PASS | 5 checkable items: closed mesh from character+font+height+depth, correct holes, outward normals, bundled font, < 1s generation |
| 6 | Right-sized | PASS | Size L (2-3 days), 4 scenarios -- note: L is at the upper bound but acceptable for a library integration story |
| 7 | Technical notes | PASS | ttf2mesh primary, FreeType+earcut fallback, Infrastructure ring, CMake integration |
| 8 | Dependencies | PASS | No dependencies (parallel development possible) |

**Result: PASS -- ready for DESIGN wave**

---

## US-11: Letter Convex Decomposition for Physics

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "Jolt requires dynamic bodies to use convex shapes; 'e' is concave with a hole; MeshShape is static-only" |
| 2 | User/persona | PASS | Developer connecting letter mesh to Jolt physics |
| 3 | Domain examples (3+) | PASS | 3 examples: 'e' decomposes to 8-15 hulls, 'I' decomposes to 1 hull, collision vs. render shape distinction |
| 4 | UAT scenarios (3-7) | PASS | 4 scenarios: decomposition produces hulls, hulls work as Jolt compound shape, convex input gives single hull, completion time |
| 5 | Acceptance criteria | PASS | 4 checkable items: hull output from mesh input, Jolt compatibility, time bound, hull count bound |
| 6 | Right-sized | PASS | Size M (2 days), 4 scenarios |
| 7 | Technical notes | PASS | V-HACD/CoACD candidates, one-time at load, Infrastructure ring, CMake integration |
| 8 | Dependencies | PASS | US-10 -- tracked in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-12: Scene Validation for Soft Body Parameters

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "Marco sets pressure=50000 and solver_iterations=0; simulation explodes; 20 minutes wasted" |
| 2 | User/persona | PASS | Technical artist wanting fast feedback on configuration errors |
| 3 | Domain examples (3+) | PASS | 3 examples: extreme pressure caught, zero iterations blocked, negative compliance blocked |
| 4 | UAT scenarios (3-7) | PASS | 7 scenarios: valid params pass, grid_resolution range, excessive pressure warning, negative compliance, solver_iterations, body_type mismatch |
| 5 | Acceptance criteria | PASS | 8 checkable items covering all parameter validations |
| 6 | Right-sized | PASS | Size S (1 day), 7 scenarios |
| 7 | Technical notes | PASS | Extends existing SceneValidator, same ValidationResult pattern, fail-fast before physics init |
| 8 | Dependencies | PASS | US-08, US-09 -- both tracked in priority order |

**Result: PASS -- ready for DESIGN wave**

---

## US-13: Demo Scene - Jelly Cube Hits Letter 'e'

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Problem statement | PASS | "All components built but nobody has seen them work together; no example scene to start from" |
| 2 | User/persona | PASS | Technical artist wanting to try the new feature; copy-paste-and-run example |
| 3 | Domain examples (3+) | PASS | 3 examples: Marco runs the demo, Sofia modifies parameters, validation before render |
| 4 | UAT scenarios (3-7) | PASS | 5 scenarios: validates, renders 150 frames, deformation visible, letter topples, translucent material |
| 5 | Acceptance criteria | PASS | 8 checkable items: file exists, scene contents, camera, animation config, validation, no crashes, deformation, toppling |
| 6 | Right-sized | PASS | Size S (< 1 day), 5 scenarios |
| 7 | Technical notes | PASS | Specific parameter values for jelly and letter, camera positioning guidance |
| 8 | Dependencies | PASS | US-07, US-08, US-09, US-12 -- all tracked and preceding |

**Result: PASS -- ready for DESIGN wave**

---

## Summary

| Story | DoR Status | Notes |
|---|---|---|
| US-01 | PASS (8/8) | Foundation story, no blockers |
| US-02 | PASS (8/8) | Interface contract, depends on US-01 |
| US-03 | PASS (8/8) | Core physics, depends on US-01, US-02 |
| US-04 | PASS (8/8) | Mesh extraction, depends on US-03 |
| US-05 | PASS (8/8) | Rendering shape, no blockers, parallel development |
| US-06 | PASS (8/8) | Vertex update, depends on US-05 |
| US-07 | PASS (8/8) | Integration story, depends on US-02, US-04, US-06 |
| US-08 | PASS (8/8) | YAML parsing, depends on US-01, US-05 |
| US-09 | PASS (8/8) | Letter YAML, depends on US-10, US-11 |
| US-10 | PASS (8/8) | Font-to-mesh, no blockers, parallel development |
| US-11 | PASS (8/8) | Convex decomposition, depends on US-10 |
| US-12 | PASS (8/8) | Validation, depends on US-08, US-09 |
| US-13 | PASS (8/8) | Demo scene, depends on US-07, US-08, US-09, US-12 |

**All 13 stories pass Definition of Ready. Approved for DESIGN wave handoff.**

---

## Risks Tracked at Handoff

| Risk | Affected Stories | Mitigation |
|---|---|---|
| Soft body vs dynamic compound shape collision untested in Jolt | US-03, US-13 | Validate with simple test case in US-03 before full 'e' letter implementation |
| ttf2mesh may not handle 'e' counter (hole) correctly | US-10 | FreeType + earcut.hpp as fallback; test 'e' character early |
| GPU Metal rendering incompatible with per-frame mesh updates | US-05, US-07 | CPU-only rendering for soft body scenes in v1; GPU deferred to future work |
| Jelly parameters need experimental tuning | US-03, US-13 | Start with recommended defaults (pressure=2000, edge_compliance=1e-4); iterate on physics-only runs before full renders |
| New library dependencies (ttf2mesh, V-HACD) need CMake integration | US-10, US-11 | Spike CMake integration early; both libraries are self-contained |
