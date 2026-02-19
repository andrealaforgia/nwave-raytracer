# GPU Compute Rendering -- Definition of Ready Checklist

**Document ID**: DOR-GPU-COMPUTE-001
**Date**: 2026-02-17
**Status**: Draft

---

## DoR Validation Summary

| Story | 1. Problem | 2. Persona | 3. Examples | 4. UAT | 5. AC | 6. Size | 7. Tech Notes | 8. Dependencies | Status |
|---|---|---|---|---|---|---|---|---|---|
| US-GPU-000 | PASS | PASS | PASS (3) | PASS (4) | PASS (8+4) | PASS (~3d) | PASS | PASS (none) | READY |
| US-GPU-001 | PASS | PASS | PASS (3) | PASS (4) | PASS (7+3) | PASS (~2d) | PASS | PASS (US-GPU-000) | READY |
| US-GPU-002 | PASS | PASS | PASS (3) | PASS (3) | PASS (5+3) | PASS (~2d) | PASS | PASS (US-GPU-000, US-GPU-001) | READY |
| US-GPU-003 | PASS | PASS | PASS (3) | PASS (4) | PASS (8+4) | PASS (~3d) | PASS | PASS (US-GPU-001) | READY |
| US-GPU-004 | PASS | PASS | PASS (3) | PASS (3) | PASS (8+4) | PASS (~3d) | PASS | PASS (US-GPU-002, US-GPU-003) | READY |
| US-GPU-005 | PASS | PASS | PASS (3) | PASS (4) | PASS (9+4) | PASS (~3d) | PASS | PASS (US-GPU-004) | READY |
| US-GPU-006 | PASS | PASS | PASS (3) | PASS (4) | PASS (7+4) | PASS (~3d) | PASS | PASS (US-GPU-004) | READY |
| US-GPU-007 | PASS | PASS | PASS (3) | PASS (4) | PASS (7+3) | PASS (~2d) | PASS | PASS (US-GPU-005) | READY |
| US-GPU-008 | PASS | PASS | PASS (3) | PASS (4) | PASS (6+4) | PASS (~2d) | PASS | PASS (US-GPU-007) | READY |

**Overall**: All 9 stories pass all 8 DoR items. Ready for DESIGN wave handoff.

---

## Detailed DoR Item Evaluation Per Story

### DoR Item 1: Problem Statement Clear and in Domain Language

All stories start from a specific user pain point using domain language (rendering, GPU, performance, iteration time). No story starts with "implement Metal" or prescribes a technical framework.

| Story | Problem Statement | Verdict |
|---|---|---|
| US-GPU-000 | Elena wants to confirm the Metal pipeline works end-to-end before building a complex kernel | PASS |
| US-GPU-001 | Sofia wants to switch between CPU and GPU without changing her workflow | PASS |
| US-GPU-002 | Elena wants GPU ray generation to match CPU output to validate camera math on GPU | PASS |
| US-GPU-003 | Elena cannot test GPU intersections because Scene uses virtual dispatch/pointers | PASS |
| US-GPU-004 | Sofia has sky and packed data but no visible scene from GPU yet | PASS |
| US-GPU-005 | David's metal sphere appears black (no reflections) and glass is opaque (no refraction) on GPU | PASS |
| US-GPU-006 | Sofia's 500-sphere scene is slow on GPU because of brute-force intersection | PASS |
| US-GPU-007 | Sofia's GPU renders are noisy and aliased at 1 SPP | PASS |
| US-GPU-008 | Sofia's 300-frame animation takes 10+ hours on CPU; she wants GPU per-frame rendering | PASS |

---

### DoR Item 2: User/Persona Identified with Specific Characteristics

Four personas from the existing requirements are reused consistently, each with distinct GPU-relevant motivations.

| Persona | Role | GPU Context | Primary Motivation |
|---|---|---|---|
| Elena Marchetti | CG student | Learning GPU compute concepts; validating CPU-GPU equivalence | Understanding data-oriented design for GPUs |
| David Okonkwo | Hobbyist 3D artist | Wants fast iteration on scenes with reflections and glass | Seeing all material types work on GPU |
| Prof. Kenji Tanaka | CS instructor | Needs CPU fallback for lab machines without Metal | Teaching GPU parallelism with concrete example |
| Sofia Reyes | Technical artist | Production renders at 4K; animation sequences | 50-200x speedup for complex scenes |

Every story identifies which persona it serves and why. **PASS** for all stories.

---

### DoR Item 3: At Least 3 Domain Examples with Real Data

| Story | Example 1 (Happy Path) | Example 2 (Edge/Variant) | Example 3 (Boundary) | Verdict |
|---|---|---|---|---|
| US-GPU-000 | Elena renders gradient via Metal on M2 MacBook | Elena tries Metal on Linux (error) | Elena times 3840x2160 gradient (<1s) | PASS |
| US-GPU-001 | Sofia renders same scene CPU vs GPU | Sofia runs animation with GPU | David uses default (CPU) unchanged | PASS |
| US-GPU-002 | Elena compares GPU sky to CPU sky (identical) | Elena changes FOV, GPU matches | Elena tilts camera, GPU matches | PASS |
| US-GPU-003 | Elena flattens 3 spheres + 2 materials | Elena flattens TransformedShape | Elena flattens empty scene | PASS |
| US-GPU-004 | Sofia renders 3 spheres on plane via GPU | Sofia renders scene with shadows | Sofia renders empty scene (regression) | PASS |
| US-GPU-005 | David sees reflections in chrome sphere | David sees glass refraction | David sees emissive glow | PASS |
| US-GPU-006 | Sofia's 500-sphere: 10x faster with BVH | Sofia's 3-sphere: minimal overhead | All shapes at same position (degenerate) | PASS |
| US-GPU-007 | Sofia compares 1 SPP vs 48 SPP on GPU | Sofia GPU vs CPU at 48 SPP (match) | Sofia linear time scaling | PASS |
| US-GPU-008 | Sofia renders bouncing-ball animation GPU | Sofia verifies frame 0 CPU vs GPU | Sofia monitors progress during GPU animation | PASS |

All examples use real personas (Sofia Reyes, Elena Marchetti, David Okonkwo), real data (specific coordinates, SPP values, image dimensions), and real scenarios (Cornell Box, 500-sphere scene, physics animation). **PASS** for all stories.

---

### DoR Item 4: UAT Scenarios in Given/When/Then (3-7 scenarios)

| Story | Scenario Count | Scenario Types | Verdict |
|---|---|---|---|
| US-GPU-000 | 4 | Valid PPM output, non-macOS fallback, default backend unchanged, timing budget | PASS |
| US-GPU-001 | 4 | CPU identical, animation integration, unknown backend error, settings propagation | PASS |
| US-GPU-002 | 3 | Pixel-identical sky, camera buffer correctness, timing budget | PASS |
| US-GPU-003 | 4 | Sphere data, all shape types, material dedup, Linux compilation | PASS |
| US-GPU-004 | 3 | Diffuse shading, shadows, GPU vs CPU comparison | PASS |
| US-GPU-005 | 4 | Metal reflection, glass refraction, depth limit, all materials | PASS |
| US-GPU-006 | 4 | BVH vs brute-force identical, speedup, node layout, stack no overflow | PASS |
| US-GPU-007 | 4 | Aliasing reduction, gamma match, NaN handling, linear time scaling | PASS |
| US-GPU-008 | 4 | Frame naming, physics updates, progress reporting, CPU vs GPU comparison | PASS |

All scenarios use Given/When/Then format with specific, measurable assertions. **PASS** for all stories.

---

### DoR Item 5: Acceptance Criteria Derived from UAT

Each story's acceptance criteria directly trace to its UAT scenarios. Criteria are organized into Must Have and Boundary/Edge tiers in the acceptance-criteria.md document.

| Story | Must Have AC | Boundary AC | Total | Verdict |
|---|---|---|---|---|
| US-GPU-000 | 8 | 4 | 12 | PASS |
| US-GPU-001 | 7 | 3 | 10 | PASS |
| US-GPU-002 | 5 | 3 | 8 | PASS |
| US-GPU-003 | 8 | 4 | 12 | PASS |
| US-GPU-004 | 8 | 4 | 12 | PASS |
| US-GPU-005 | 9 | 4 | 13 | PASS |
| US-GPU-006 | 7 | 4 | 11 | PASS |
| US-GPU-007 | 7 | 3 | 10 | PASS |
| US-GPU-008 | 6 | 4 | 10 | PASS |

All acceptance criteria are checkable with defined verification methods (unit test, integration test, visual comparison, timing measurement, pixel comparison). **PASS** for all stories.

---

### DoR Item 6: Story Right-Sized (1-3 days, 3-7 scenarios)

| Story | Estimated Effort | Justification | Scenarios | Verdict |
|---|---|---|---|---|
| US-GPU-000 | ~3 days | Metal API init, CMake shader compilation, compute dispatch, readback, CLI flag | 4 | PASS |
| US-GPU-001 | ~2 days | Abstract interface, CPU wrapper, factory, CLI wiring | 4 | PASS |
| US-GPU-002 | ~2 days | Camera buffer packing, ray generation shader, sky gradient | 3 | PASS |
| US-GPU-003 | ~3 days | Tagged union structs (3 types), scene traversal, material dedup, alignment | 4 | PASS |
| US-GPU-004 | ~3 days | 3 shape intersections, diffuse shading, shadow rays, GPU RNG | 3 | PASS |
| US-GPU-005 | ~3 days | 4 material types in iterative loop, accumulation pattern | 4 | PASS |
| US-GPU-006 | ~3 days | BVH flattening, GPU traversal with stack, performance validation | 4 | PASS |
| US-GPU-007 | ~2 days | SPP loop, jittering, accumulation, gamma, NaN clamping | 4 | PASS |
| US-GPU-008 | ~2 days | WriteCallback wiring, per-frame re-flatten, progress integration | 4 | PASS |

All stories fall within 2-3 day range. Total feature effort: approximately 23 developer-days. **PASS** for all stories.

---

### DoR Item 7: Technical Notes Identify Constraints and Dependencies

Every story includes a Technical Notes section identifying:

| Story | Key Technical Constraints |
|---|---|
| US-GPU-000 | MTLCreateSystemDefaultDevice(), CMake .metal compilation, threadgroup 16x16, #ifdef __APPLE__ guards |
| US-GPU-001 | Ring 3 abstract class, Ring 4 Metal implementation, factory in main.cpp, RenderSettings.backend field |
| US-GPU-002 | Float vs double precision, camera buffer struct alignment, CPU-GPU ray formula equivalence |
| US-GPU-003 | ShapeType/MaterialType enums, alignas(16), material dedup via pointer map, TransformedShape inverse matrix |
| US-GPU-004 | Quadratic sphere, dot-product plane, slab box, GPU PCG RNG, epsilon 0.001 |
| US-GPU-005 | Iterative loop pattern (throughput + accumulated_color), Schlick's formula, Snell's law, PCG seeding |
| US-GPU-006 | LinearBVHNode struct layout, implicit first child, explicit second child offset, fixed 64-entry stack |
| US-GPU-007 | Per-pixel sample loop, PCG seeding strategy, NaN guard (x != x), gamma after average |
| US-GPU-008 | WriteCallback unchanged, per-frame re-flatten, GPU device/queue reuse, buffer pooling |

**PASS** for all stories.

---

### DoR Item 8: Dependencies Resolved or Tracked

#### Dependency Graph

```
US-GPU-000 (Walking Skeleton: Metal pipeline)
    |
    +-- US-GPU-001 (Render Backend Abstraction)
    |       |
    |       +-- US-GPU-002 (Ray Generation + Sky)
    |       |       |
    |       |       +-- US-GPU-004 (Single-Bounce Diffuse)
    |       |               |
    |       |               +-- US-GPU-005 (Multi-Bounce)
    |       |               |       |
    |       |               |       +-- US-GPU-007 (SPP Accumulation)
    |       |               |               |
    |       |               |               +-- US-GPU-008 (Animation Integration)
    |       |               |
    |       |               +-- US-GPU-006 (Linear BVH)
    |       |
    |       +-- US-GPU-003 (Scene Data Packing)
    |               |
    |               +-- US-GPU-004 (Single-Bounce Diffuse)
```

#### External Dependencies

| Dependency | Status | Notes |
|---|---|---|
| Metal.framework | Available | System framework on macOS 10.14+; no external download required |
| MetalKit.framework | Available | System framework; may not be needed if using Metal directly |
| CMake metal compilation | Tracked | Custom CMake commands needed; documented in US-GPU-000 technical notes |
| Existing CPU Renderer | Stable | 243 tests passing; no changes needed |
| Existing CLI Dispatcher | Stable | Needs `--backend` flag addition (US-GPU-000) |
| Apple Silicon GPU | Available | M1/M2/M3 series; Intel Macs with Metal also supported |

#### Cross-Story Dependencies

| Story | Depends On | Reason |
|---|---|---|
| US-GPU-000 | (none) | Foundation story -- no prerequisites |
| US-GPU-001 | US-GPU-000 | Needs Metal pipeline to wrap in MetalRenderBackend |
| US-GPU-002 | US-GPU-000, US-GPU-001 | Needs Metal compute infrastructure and backend interface |
| US-GPU-003 | US-GPU-001 | Needs backend interface to know what data the GPU needs |
| US-GPU-004 | US-GPU-002, US-GPU-003 | Needs GPU ray generation and flat scene data |
| US-GPU-005 | US-GPU-004 | Extends single-bounce shader to multi-bounce |
| US-GPU-006 | US-GPU-004 | Replaces brute-force with BVH in intersection code |
| US-GPU-007 | US-GPU-005 | Needs full material support before SPP averaging is meaningful |
| US-GPU-008 | US-GPU-007 | Needs production-quality GPU rendering before animation is worthwhile |

No circular dependencies. US-GPU-005 and US-GPU-006 can be developed in parallel after US-GPU-004 (they modify different parts of the shader: material loop vs intersection traversal).

**PASS** for all stories.

---

## Recommended Implementation Order

```
Week 1:  US-GPU-000 (Walking Skeleton)
Week 1:  US-GPU-001 (Backend Abstraction) -- can overlap with 000
Week 2:  US-GPU-002 (Ray Generation + Sky)
Week 2:  US-GPU-003 (Scene Data Packing) -- parallel with 002
Week 3:  US-GPU-004 (Single-Bounce Diffuse) -- first visible scene
Week 4:  US-GPU-005 (Multi-Bounce) -- reflections + glass
Week 4:  US-GPU-006 (Linear BVH) -- parallel with 005
Week 5:  US-GPU-007 (SPP Accumulation) -- production quality
Week 5:  US-GPU-008 (Animation Integration)
```

Estimated total: 5 weeks of focused development (23 developer-days, accounting for parallelism in weeks 2 and 4).

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Float vs double precision causes visible artifacts | Medium | Medium | Validate early (US-GPU-002 sky comparison); accept small per-pixel differences |
| Metal shader compilation adds build complexity | Medium | Low | Isolate to CMake conditional block; document setup steps |
| GPU memory limits for very large scenes (50K+ shapes) | Low | Medium | Document GPU buffer size limits; defer to future optimization story |
| TriangleMesh GPU support deferred | High | Low | Documented as out-of-scope; most scenes use basic shapes |
| PCG RNG quality differs from std::mt19937 | Low | Low | Statistical tests in US-GPU-005; visual comparison against CPU output |
| Linux/CI builds break from Metal code | Medium | High | Platform guards (#ifdef __APPLE__) in every Metal source; CI runs on Linux without Metal |
