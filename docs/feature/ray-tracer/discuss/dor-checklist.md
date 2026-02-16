# Ray Tracer -- Definition of Ready Checklist

**Document ID**: DOR-RAYTRACER-001
**Date**: 2026-02-16
**Status**: Draft

---

## DoR Validation Summary

| Story | 1. Problem | 2. Persona | 3. Examples | 4. UAT | 5. AC | 6. Size | 7. Tech Notes | 8. Dependencies | Status |
|---|---|---|---|---|---|---|---|---|---|
| US-000 | PASS | PASS | PASS (3) | PASS (5) | PASS (5) | PASS (~3d) | PASS | PASS | READY |
| US-101 | PASS | PASS | PASS (3) | PASS (3) | PASS (6) | PASS (~1d) | PASS | PASS (US-000) | READY |
| US-102 | PASS | PASS | PASS (3) | PASS (3) | PASS (5) | PASS (~1d) | PASS | PASS (US-000) | READY |
| US-103 | PASS | PASS | PASS (3) | PASS (3) | PASS (5) | PASS (~2d) | PASS | PASS (US-101) | READY |
| US-201 | PASS | PASS | PASS (3) | PASS (3) | PASS (5) | PASS (~1d) | PASS | PASS (US-000) | READY |
| US-202 | PASS | PASS | PASS (3) | PASS (3) | PASS (4) | PASS (~1d) | PASS | PASS (US-201) | READY |
| US-203 | PASS | PASS | PASS (3) | PASS (4) | PASS (5) | PASS (~1d) | PASS | PASS (US-201) | READY |
| US-301 | PASS | PASS | PASS (3) | PASS (3) | PASS (5) | PASS (~2d) | PASS | PASS (US-000, US-501) | READY |
| US-302 | PASS | PASS | PASS (3) | PASS (3) | PASS (5) | PASS (~1d) | PASS | PASS (US-301) | READY |
| US-401 | PASS | PASS | PASS (3) | PASS (4) | PASS (6) | PASS (~2d) | PASS | PASS (US-000, US-501) | READY |
| US-402 | PASS | PASS | PASS (3) | PASS (3) | PASS (3) | PASS (~1d) | PASS | PASS (US-401) | READY |
| US-501 | PASS | PASS | PASS (3) | PASS (3) | PASS (5) | PASS (~1d) | PASS | PASS (US-000) | READY |
| US-601 | PASS | PASS | PASS (3) | PASS (3) | PASS (6) | PASS (~2d) | PASS | PASS (US-000) | READY |
| US-602 | PASS | PASS | PASS (3) | PASS (3) | PASS (5) | PASS (~2d) | PASS | PASS (US-601, US-701) | READY |
| US-701 | PASS | PASS | PASS (3) | PASS (4) | PASS (6) | PASS (~1d) | PASS | PASS (US-000) | READY |
| US-702 | PASS | PASS | PASS (3) | PASS (3) | PASS (4) | PASS (~1d) | PASS | PASS (US-701) | READY |
| US-801 | PASS | PASS | PASS (3) | PASS (4) | PASS (5) | PASS (~3d) | PASS | PASS (US-000, all primitives) | READY |
| US-901 | PASS | PASS | PASS (3) | PASS (4) | PASS (6) | PASS (~3d) | PASS | PASS (all primitives, materials) | READY |
| US-1001 | PASS | PASS | PASS (3) | PASS (3) | PASS (5) | PASS (~2d) | PASS | PASS (US-203) | READY |
| US-1002 | PASS | PASS | PASS (3) | PASS (3) | PASS (4) | PASS (~1d) | PASS | PASS (US-000) | READY |

**Overall**: All 20 stories pass all 8 DoR items. Ready for DESIGN wave handoff.

---

## Detailed DoR Item Evaluation Per Story

### DoR Item 1: Problem Statement Clear and in Domain Language

All stories start from a specific user pain point using domain language (rendering, shading, materials, camera, anti-aliasing). No story starts with "implement X" or prescribes a technical solution.

| Story | Problem Statement | Verdict |
|---|---|---|
| US-000 | Elena wants to see her first ray-traced image but finds it overwhelming to know which pipeline pieces are needed | PASS |
| US-101 | David wants flat polygonal surfaces but only has spheres and infinite planes | PASS |
| US-102 | David wants box shapes but would need 12 manual triangles | PASS |
| US-103 | Sofia wants smooth curved meshes but individual triangles look faceted | PASS |
| US-201 | Elena's single-light scenes look flat; she wants multi-light setups | PASS |
| US-202 | David wants sunlight but point lights produce radial falloff, not parallel rays | PASS |
| US-203 | Elena's sphere floats with no shadow, making the image look unrealistic | PASS |
| US-301 | David wants reflective chrome but Lambertian scatters randomly | PASS |
| US-302 | David has perfect mirrors but real metals have blurry reflections | PASS |
| US-401 | Elena wants glass that bends light but her materials scatter or reflect only | PASS |
| US-402 | Elena renders solid glass but wants a thin shell (Christmas ornament) | PASS |
| US-501 | Prof. Tanaka needs configurable recursion depth for teaching | PASS |
| US-601 | Elena's camera is hardcoded; she cannot change viewpoint without recompiling | PASS |
| US-602 | Sofia wants shallow depth of field but pinhole camera renders everything in focus | PASS |
| US-701 | Elena's sphere edges look jagged from single-sample-per-pixel rendering | PASS |
| US-702 | Elena's random samples cluster at low SPP, producing uneven noise | PASS |
| US-801 | Sofia's 500-sphere scene takes 10+ minutes because every ray tests every object | PASS |
| US-901 | Elena must recompile C++ for every scene change (30+ second iteration cycle) | PASS |
| US-1001 | David's hard-edged shadows look artificial; real lights have physical size | PASS |
| US-1002 | Elena's images look too dark because linear colors display incorrectly on monitors | PASS |

---

### DoR Item 2: User/Persona Identified with Specific Characteristics

Four personas are used consistently, each with distinct motivations and contexts.

| Persona | Role | Context | Primary Motivation |
|---|---|---|---|
| Elena Marchetti | CG student | Ubuntu laptop, first renderer, learning | Visible progress, understanding pipeline |
| David Okonkwo | Hobbyist 3D artist | Scene authoring, varied materials | Visual quality, creative flexibility |
| Prof. Kenji Tanaka | CS instructor | Coursework, demonstrations | Testability, pedagogical clarity |
| Sofia Reyes | Technical artist | Product visualizations, complex scenes | Performance, fidelity, camera control |

Every story identifies which persona it serves and why. **PASS** for all stories.

---

### DoR Item 3: At Least 3 Domain Examples with Real Data

Every story contains exactly 3 domain examples:
1. **Happy path** with realistic scenario and concrete data values
2. **Edge case** or alternative scenario
3. **Error/boundary case**

All examples use real persona names, specific numeric values (coordinates, colors, angles), and concrete expected outcomes. No generic "user123" or placeholder data. **PASS** for all stories.

---

### DoR Item 4: UAT Scenarios in Given/When/Then (3-7 Scenarios)

| Story | Scenario Count | Range Check |
|---|---|---|
| US-000 | 5 | PASS (3-7) |
| US-101 | 3 | PASS (3-7) |
| US-102 | 3 | PASS (3-7) |
| US-103 | 3 | PASS (3-7) |
| US-201 | 3 | PASS (3-7) |
| US-202 | 3 | PASS (3-7) |
| US-203 | 4 | PASS (3-7) |
| US-301 | 3 | PASS (3-7) |
| US-302 | 3 | PASS (3-7) |
| US-401 | 4 | PASS (3-7) |
| US-402 | 3 | PASS (3-7) |
| US-501 | 3 | PASS (3-7) |
| US-601 | 3 | PASS (3-7) |
| US-602 | 3 | PASS (3-7) |
| US-701 | 4 | PASS (3-7) |
| US-702 | 3 | PASS (3-7) |
| US-801 | 4 | PASS (3-7) |
| US-901 | 4 | PASS (3-7) |
| US-1001 | 3 | PASS (3-7) |
| US-1002 | 3 | PASS (3-7) |

All scenarios are in Given/When/Then format with concrete data. **PASS** for all stories.

---

### DoR Item 5: Acceptance Criteria Derived from UAT

Every story's acceptance criteria are checkable binary conditions derived directly from the UAT scenarios. The detailed AC document (acceptance-criteria.md) further breaks these into Must Have and Boundary/Edge tiers with explicit verification methods. **PASS** for all stories.

---

### DoR Item 6: Story Right-Sized (1-3 Days, 3-7 Scenarios)

| Story | Estimated Effort | Scenario Count | Verdict |
|---|---|---|---|
| US-000 | ~3 days (full pipeline skeleton) | 5 | PASS |
| US-101 | ~1 day | 3 | PASS |
| US-102 | ~1 day | 3 | PASS |
| US-103 | ~2 days | 3 | PASS |
| US-201 | ~1 day | 3 | PASS |
| US-202 | ~1 day | 3 | PASS |
| US-203 | ~1 day | 4 | PASS |
| US-301 | ~2 days | 3 | PASS |
| US-302 | ~1 day | 3 | PASS |
| US-401 | ~2 days | 4 | PASS |
| US-402 | ~1 day | 3 | PASS |
| US-501 | ~1 day | 3 | PASS |
| US-601 | ~2 days | 3 | PASS |
| US-602 | ~2 days | 3 | PASS |
| US-701 | ~1 day | 4 | PASS |
| US-702 | ~1 day | 3 | PASS |
| US-801 | ~3 days | 4 | PASS |
| US-901 | ~3 days | 4 | PASS |
| US-1001 | ~2 days | 3 | PASS |
| US-1002 | ~1 day | 3 | PASS |

All stories fall within 1-3 day range with 3-5 scenarios each. No oversized stories. **PASS** for all stories.

---

### DoR Item 7: Technical Notes Identify Constraints and Dependencies

Every story includes a Technical Notes section identifying:
- Key algorithms (Moller-Trumbore, slab method, Schlick approximation, etc.)
- Data structures needed (Vec3, Ray, HitRecord, etc.)
- Numerical considerations (epsilon offsets, NaN guards, normalization)
- References to the research document for detailed formulas

**PASS** for all stories.

---

### DoR Item 8: Dependencies Resolved or Tracked

| Story | Dependencies | Status |
|---|---|---|
| US-000 | None (walking skeleton is foundational) | PASS |
| US-101 | US-000 (Hittable interface, Ray, HitRecord) | Tracked |
| US-102 | US-000 (Hittable interface) | Tracked |
| US-103 | US-101 (Triangle primitive, Moller-Trumbore) | Tracked |
| US-201 | US-000 (single light shading loop) | Tracked |
| US-202 | US-201 (multi-light infrastructure) | Tracked |
| US-203 | US-201 (light list for shadow rays) | Tracked |
| US-301 | US-000 (Material interface); US-501 (recursion) | Tracked |
| US-302 | US-301 (Metal material base) | Tracked |
| US-401 | US-000 (Material interface); US-501 (recursion) | Tracked |
| US-402 | US-401 (Dielectric material) | Tracked |
| US-501 | US-000 (trace_ray with depth parameter) | Tracked |
| US-601 | US-000 (basic camera exists) | Tracked |
| US-602 | US-601 (configurable camera); US-701 (multi-sample) | Tracked |
| US-701 | US-000 (render loop exists) | Tracked |
| US-702 | US-701 (supersampling infrastructure) | Tracked |
| US-801 | US-000 (Hittable::bounding_box); all primitives | Tracked |
| US-901 | All primitives and materials (to reference in parser) | Tracked |
| US-1001 | US-203 (shadow ray infrastructure) | Tracked |
| US-1002 | US-000 (pixel output pipeline) | Tracked |

All dependencies are explicitly identified and traceable. No circular dependencies exist. **PASS** for all stories.

---

## Recommended Implementation Order

Based on dependency analysis, the stories should be implemented in this sequence:

```
Phase 0: Walking Skeleton
  US-000  Render a Single Sphere on a Plane to PPM

Phase 1: Core Infrastructure
  US-601  Configurable Pinhole Camera
  US-501  Recursive Reflection with Configurable Depth
  US-701  Random Supersampling Anti-Aliasing
  US-1002 Gamma Correction and Tone Mapping

Phase 2: Primitives
  US-101  Render Triangles
  US-102  Render Axis-Aligned Boxes
  US-103  Triangle Meshes with Smooth Shading

Phase 3: Lighting and Shadows
  US-201  Multiple Point Lights
  US-202  Directional Light Support
  US-203  Hard Shadows

Phase 4: Materials
  US-301  Metal Material with Mirror Reflection
  US-302  Glossy (Fuzzy) Metal Reflections
  US-401  Dielectric (Glass) Material
  US-402  Hollow Glass Sphere

Phase 5: Advanced Camera and Sampling
  US-602  Thin Lens Camera (Depth of Field)
  US-702  Stratified (Jittered) Sampling

Phase 6: Performance
  US-801  BVH Acceleration

Phase 7: Usability
  US-901  Load Scene from YAML

Phase 8: Polish
  US-1001 Area Lights with Soft Shadows
```

---

## Anti-Pattern Check

| Anti-Pattern | Detected? | Evidence |
|---|---|---|
| Implement-X | No | All stories start from user pain points |
| Generic data | No | All examples use named personas and real numeric values |
| Technical AC | No | AC describe observable outcomes, not implementation details |
| Oversized story | No | All stories are 1-3 days with 3-5 scenarios |
| Abstract requirements | No | Every story has 3+ concrete domain examples with specific data |
