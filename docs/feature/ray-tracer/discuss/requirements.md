# Ray Tracer Application -- Requirements Document

**Document ID**: REQ-RAYTRACER-001
**Date**: 2026-02-16
**Status**: Draft -- Pending DoR Validation

---

## 1. Problem Statement

Artists, hobbyists, and computer graphics students who want to generate photorealistic images of 3D scenes currently face a steep barrier: either they must understand and assemble a complex rendering pipeline from scratch, or they rely on opaque, heavyweight tools that obscure the underlying principles. There is no lightweight, incremental C++ ray tracer that lets a user define a scene (objects, lights, cameras, materials) through a simple configuration file or API and produce a high-quality rendered image with transparent, understandable steps from scene definition to pixel output.

---

## 2. Stakeholders

| Stakeholder | Role | Key Concern |
|---|---|---|
| **Elena Marchetti** | CG student building her first renderer | Incremental complexity; clear error feedback; visible progress from first render |
| **David Okonkwo** | Hobbyist 3D artist | Scene authoring flexibility; material variety (glass, metal, matte); visual quality |
| **Prof. Kenji Tanaka** | CS instructor using the tool in coursework | Testability; clean architecture; well-defined pipeline stages for teaching |
| **Sofia Reyes** | Technical artist creating product visualizations | Performance on complex scenes; high-fidelity materials; camera control |

---

## 3. Business Goals

1. Provide an end-to-end ray tracing pipeline from scene definition to image output.
2. Support incremental capability growth: each feature builds on the previous without breaking existing renders.
3. Produce visually correct images verifiable against known reference scenes (Cornell Box, Shirley's random spheres).
4. Maintain render performance that scales sub-linearly with scene complexity via acceleration structures.

---

## 4. Capability Areas

### 4.0 Walking Skeleton (Minimal End-to-End Pipeline)

**Purpose**: Validate the entire pipeline from scene definition through ray generation, intersection testing, shading, and image output with the smallest possible feature set.

**Scope**:
- Hardcoded scene: 1 sphere (radius 0.5, centered at origin), 1 ground plane (y = -0.5)
- 1 point light (white, positioned above and to the left)
- 1 pinhole camera (positioned at z = -1.5, looking at origin, 90-degree vertical FOV)
- Lambertian diffuse material only (sphere: red, albedo 0.7; plane: gray, albedo 0.5)
- Ambient + diffuse shading (Phong diffuse term: max(0, N dot L))
- Ray-sphere and ray-plane intersection
- Output to PPM image file (P3 text format)
- Single sample per pixel (no anti-aliasing)
- No acceleration structures
- Image resolution: 400x225 (16:9 aspect ratio)

**Constraints**:
- The walking skeleton must produce a recognizable image of a red sphere on a gray plane with visible shading variation (lit side brighter than shadow side).
- Output file must be a valid PPM that opens in any image viewer.

### 4.1 Geometric Primitives

**Purpose**: Allow users to populate scenes with a variety of 3D shapes.

| Primitive | Definition | Intersection Method |
|---|---|---|
| Sphere | Center point + radius | Quadratic formula on ray-sphere equation |
| Plane | Point on plane + normal vector | Ray-plane dot product |
| Triangle | 3 vertex positions | Moller-Trumbore algorithm with barycentric coordinates |
| Triangle Mesh | Collection of triangles with shared vertices and per-vertex normals | Per-triangle Moller-Trumbore; smooth normals via barycentric interpolation |
| Axis-Aligned Box | Min corner + max corner | Slab method (3 axis-aligned slab pairs) |
| Cylinder | Center axis + radius + height | Quadratic on projected ray + cap disc checks |
| Cone | Apex + axis + half-angle + height | Quadratic on projected ray + base cap check |

**Constraints**:
- Every primitive must report a valid surface normal at the hit point.
- Normals always point outward from the surface (front-face determination based on ray direction).
- All primitives must provide a bounding box for acceleration structure compatibility.

### 4.2 Lighting

**Purpose**: Illuminate scenes with controllable light sources that produce realistic shading and shadows.

| Light Type | Parameters | Shadow Behavior |
|---|---|---|
| Point light | Position, color, intensity | Hard shadows via single shadow ray |
| Directional light | Direction, color, intensity | Hard shadows via single shadow ray toward light direction |
| Area light | Position, size (rectangle or disc), color, intensity | Soft shadows via multiple shadow ray samples |

**Constraints**:
- Shadow rays must be offset from the surface by a small epsilon along the surface normal to prevent shadow acne.
- Multiple lights must contribute additively to surface illumination.
- Shadow testing must correctly handle objects between the surface point and the light (full occlusion) vs. objects beyond the light (no occlusion).

### 4.3 Camera System

**Purpose**: Let users control the viewpoint, field of view, and optical effects for the rendered image.

| Camera Model | Parameters | Behavior |
|---|---|---|
| Pinhole camera | Position (lookfrom), target (lookat), up vector (vup), vertical FOV, aspect ratio | All rays originate from a single point; infinite depth of field |
| Thin lens camera | All pinhole params + aperture diameter + focus distance | Rays originate from random points on the lens disc; objects at focus distance are sharp, others are blurred proportionally |

**Constraints**:
- Camera must construct an orthonormal basis (u, v, w) from lookfrom, lookat, and vup.
- Viewport dimensions derive from vertical FOV and aspect ratio.
- Image resolution is user-configurable (width in pixels; height derived from aspect ratio).

### 4.4 Material System

**Purpose**: Define how surfaces interact with light, enabling a range of appearances from matte to mirror to glass.

| Material | Parameters | Behavior |
|---|---|---|
| Lambertian (diffuse) | Albedo color (RGB) | Scatters reflected ray in random hemisphere direction; attenuates by albedo |
| Metal (specular) | Albedo color, fuzziness (0 = mirror, up to 1 = rough) | Reflects ray in mirror direction with optional random perturbation |
| Dielectric (glass) | Index of refraction (e.g., 1.5 for glass) | Refracts or reflects based on Schlick's approximation of Fresnel equations; total internal reflection when angle exceeds critical angle |
| Emissive | Emission color, intensity | Emits light; does not scatter incoming rays |

**Constraints**:
- Materials must be energy-conserving: scattered light intensity must not exceed incoming light intensity.
- Dielectric material must handle both entering and exiting a medium (tracking whether the ray is inside or outside).
- Metal with fuzziness 0 produces perfect mirror reflections; fuzziness > 0 produces progressively blurrier reflections.
- A degenerate scatter direction (near-zero vector from Lambertian scatter) must be detected and corrected.

### 4.5 Transparency and Refraction

**Purpose**: Enable physically plausible transparent objects (glass spheres, water surfaces, diamond).

**Key behaviors**:
- Snell's law determines the refraction angle when light crosses a material boundary.
- Schlick's approximation determines the probability of reflection vs. refraction at each surface interaction.
- Total internal reflection occurs when light travels from a denser to less dense medium at a steep angle (sin^2(theta_t) > 1.0).
- Beer's law applies absorption for colored transparent media: light attenuates exponentially with distance traveled through the medium.

**Constraints**:
- Common refractive indices must produce visually correct results: air (1.0), water (1.33), glass (1.5), diamond (2.42).
- A hollow glass sphere (sphere inside sphere with inverted normal) must render correctly, showing the characteristic dark ring at the edge.
- Recursion depth for reflection/refraction bounces must be configurable (default: 10).

### 4.6 Reflections

**Purpose**: Enable mirror-like and glossy reflective surfaces.

**Key behaviors**:
- Perfect mirror reflection: R = I - 2(I dot N)N
- Glossy reflection: reflected direction perturbed by random vector scaled by fuzziness parameter.
- Recursive ray tracing: reflected ray is traced into the scene, and the resulting color contributes to the surface color weighted by the material's reflectance.

**Constraints**:
- Maximum recursion depth prevents infinite loops (e.g., two mirrors facing each other).
- A reflected ray that scatters below the surface (dot(scattered, normal) < 0) must be absorbed (return black).

### 4.7 Anti-Aliasing and Sampling

**Purpose**: Reduce jagged edges and noise in rendered images.

| Technique | Parameters | Behavior |
|---|---|---|
| Random supersampling | Samples per pixel (SPP) | Cast SPP rays per pixel with random offsets within the pixel; average results |
| Stratified (jittered) sampling | sqrt(SPP) strata per axis | Divide pixel into grid of sub-pixels; one jittered sample per stratum |

**Constraints**:
- Increasing samples per pixel must reduce visible aliasing and noise monotonically.
- The user must be able to configure samples per pixel (default: 10; range: 1 to 10000).
- With 1 sample per pixel, the renderer must still produce a valid (aliased) image.

### 4.8 Acceleration Structures

**Purpose**: Maintain interactive-to-reasonable render times as scene complexity grows.

**Structure**: Bounding Volume Hierarchy (BVH) with axis-aligned bounding boxes (AABBs).

**Key behaviors**:
- BVH construction partitions primitives recursively using the longest-axis midpoint split (initial implementation) or Surface Area Heuristic (optimized implementation).
- Leaf nodes contain 1-4 primitives.
- Traversal skips entire subtrees whose bounding boxes are not intersected by the ray.

**Constraints**:
- BVH must be constructed once before rendering begins (not rebuilt per-ray).
- Scenes with and without BVH must produce identical rendered images (BVH is a performance optimization, not a correctness change).
- BVH must handle edge cases: single primitive, all primitives at the same location, empty scene.

### 4.9 Scene File Loading

**Purpose**: Let users define scenes in external configuration files rather than recompiling C++ code.

**Format**: YAML or JSON.

**Scene file contents**:
- Camera configuration (position, target, FOV, aperture, focus distance, image dimensions)
- List of objects, each with: primitive type, transform/parameters, material reference
- List of materials with: type, parameters (albedo, fuzziness, IOR, emission)
- List of lights with: type, parameters (position, direction, color, intensity)
- Render settings: samples per pixel, max recursion depth, output filename

**Constraints**:
- Invalid scene files must produce clear, human-readable error messages identifying the problem (e.g., "Line 14: sphere 'glass_ball' references undefined material 'crystal'").
- A scene file with no objects must produce a valid image showing only the background color.
- The scene file parser must be independent of the rendering engine (clean separation of concerns).

### 4.10 Image Output

**Purpose**: Write the rendered image to a file format viewable by standard image tools.

| Format | Priority | Description |
|---|---|---|
| PPM (P3 text) | Phase 0 (walking skeleton) | Plain text format; trivial to implement; opens in GIMP, Photoshop, IrfanView |
| PPM (P6 binary) | Phase 1 | Binary variant; smaller file size, same compatibility |
| PNG | Phase 2 | Compressed, widely supported; requires a library (stb_image_write or libpng) |

**Constraints**:
- All output formats must produce identical visual results for the same scene and render settings.
- Gamma correction (gamma 2.0: output = sqrt(linear_color)) must be applied before writing pixels.
- Color values must be clamped to [0, 255] per channel after gamma correction.
- The output filename must be configurable (default: "output.ppm").

---

## 5. Non-Functional Requirements

### 5.1 Performance

- The walking skeleton (400x225, 1 SPP, 2 objects) must render in under 1 second on a modern laptop.
- A scene with 100 spheres and 10 SPP at 800x450 must render in under 30 seconds with BVH enabled.
- BVH traversal must demonstrate measurable speedup (at least 2x) over brute-force for scenes with 50+ objects.

### 5.2 Correctness

- Ray-sphere intersection must match the analytic quadratic solution for all test configurations (miss, tangent, through-center, origin-inside-sphere).
- Rendered images must be visually comparable to reference images from "Ray Tracing in One Weekend" for equivalent scene configurations.
- The Cornell Box scene must show correct color bleeding (red/green tint on white surfaces near colored walls).

### 5.3 Maintainability

- Each capability area (primitives, materials, camera, lights, acceleration) must be independently testable via unit tests.
- Adding a new primitive type must not require changes to the rendering loop or material system.
- Adding a new material type must not require changes to the primitive intersection code.

### 5.4 Portability

- The application must compile and run on Linux, macOS, and Windows.
- C++17 standard minimum.
- No platform-specific APIs in the core rendering engine.

---

## 6. Constraints and Dependencies

| Constraint | Description |
|---|---|
| Language | C++ (C++17 or later) for maximum performance |
| External libraries | Minimized; stb_image_write permitted for PNG output; YAML/JSON parser library permitted for scene loading |
| Build system | CMake (standard for cross-platform C++) |
| No GPU dependency | CPU-only rendering (no OpenCL, CUDA, or Vulkan RT in initial scope) |
| Floating-point precision | Double precision (double) for ray-intersection math to minimize numerical artifacts |

---

## 7. Out of Scope (Current Release)

- GPU-accelerated rendering (compute shaders, RT cores)
- Subsurface scattering (skin, wax, marble)
- Spectral rendering (per-wavelength rather than RGB)
- Volumetric rendering (fog, smoke, clouds)
- Animation or video output
- Real-time interactive preview
- Texture mapping from image files (deferred to future increment)
- Constructive Solid Geometry (CSG boolean operations)
- OBJ/glTF file import (deferred; custom scene file format first)

---

## Review

**Reviewer**: nw-product-owner-reviewer (Claude Code DoR Validator)
**Review Date**: 2026-02-16
**Review Status**: APPROVED

### Executive Summary

All 20 user stories pass comprehensive Definition of Ready validation with strong alignment to research documentation. The artifact set demonstrates excellent quality across all 8 DoR dimensions. Zero critical or high-severity issues detected. Three medium-severity recommendations for continuous improvement identified.

**Overall Assessment**: APPROVED for DESIGN wave handoff.

---

### 1. Requirements Completeness

**Status**: PASS with minor gaps

**Coverage Analysis**:
- **Implemented from research**: All core ray tracing capabilities from the research document (sections 1-11) are covered in the stories.
- **Functional requirements**: Walking skeleton, primitives (sphere, plane, triangle, mesh, box, cylinder, cone), lighting (point, directional, area), camera (pinhole, thin lens), materials (Lambertian, metal, dielectric), transparency/refraction, reflections, sampling (supersampling, stratified), acceleration (BVH), scene loading, and gamma correction.
- **Non-functional requirements**: Performance targets, correctness validation against reference scenes (Cornell Box, Shirley's spheres), maintainability, and portability.

**Issues Identified**:

1. **MEDIUM SEVERITY - Missing capability: Emissive Materials**
   - Research section 6.2 documents Lambertian material and 6.4 Cook-Torrance/PBR, but emissive materials are mentioned in requirements (Section 4.4: "Emissive | Emission color, intensity | Emits light; does not scatter incoming rays").
   - **Evidence**: Requirements.md line 114: "Emissive | Emission color, intensity | Emits light; does not scatter incoming rays"
   - **Issue**: No dedicated user story for implementing emissive material (US-1003 or similar). It appears implicitly in US-901 (scene loading) but has no formal acceptance criteria or UAT scenarios.
   - **Impact**: Emissive materials are essential for lighting models (area lights, HDR, glowing objects). Without a dedicated story, implementers may defer or implement inconsistently.
   - **Remediation**: Create US-1003 for emissive material implementation.
   - **BAD**: Emissive mentioned in capability area but no story
   - **GOOD**: Create dedicated story with 3+ UAT scenarios covering emission color, intensity, interaction with lights, and HDR/tone mapping

2. **MEDIUM SEVERITY - Missing capability: CSG support**
   - Research section 3.8 documents CSG algorithms, but CSG is listed as "Out of Scope (Current Release)" in requirements section 7.
   - **Evidence**: Requirements.md line 265: "Constructive Solid Geometry (CSG boolean operations)"
   - **Issue**: This is correct deferral but creates a scope/research mismatch. The research document's section 3.8 (spanning lines 360-385) covers CSG comprehensively, but it's never connected to story planning.
   - **Impact**: No issue for current release; clarifies what's deferred.
   - **Remediation**: No action required for this release; CSG can be added to a future increment.

3. **MINOR - Missing optional enhancement: OBJ/glTF loaders**
   - Research section 2.4 documents OBJ and glTF formats; requirements defer them.
   - **Evidence**: Requirements.md line 266: "OBJ/glTF file import (deferred; custom scene file format first)"
   - **Issue**: Scene file format (US-901) uses YAML, but no plan for OBJ/glTF. This is appropriate for scope, but future stories should reference it.
   - **Impact**: Minimal; correctly scoped for Phase 0.
   - **Remediation**: Document as "Future Increment" with reference to requirement 4.9 and research section 2.4.

**Technical Accuracy Check Against Research**:

- Ray-sphere intersection (US-000, 3.1): Quadratic formula documented correctly; research section 3.1 lines 183-208 align with user story technical notes
- Moller-Trumbore (US-101, 3.3): Algorithm documented at research lines 236-299; AC matches research exactly
- Slab method for AABB (US-102, 3.5): Algorithm at research lines 310-324; AC aligns
- Triangle mesh smooth shading (US-103, 3.4): Barycentric interpolation at research lines 303-306; AC matches
- Point and directional lights (US-201/202, 5.1): Research section 5.1 lines 470-479 covers both; stories align
- Shadow rays (US-203, 5.3): Offset technique and shadow acne at research lines 490-508; AC covers epsilon offset
- Metal reflections (US-301, 8.1): Reflection formula R = I - 2(I·N)N at research line 772; story matches
- Glossy metal (US-302, 8.2): Fuzziness perturbation at research lines 790-799; story aligns
- Dielectric/glass (US-401, 7.2-7.5): Snell's law, Schlick's approximation at research lines 635-738; AC matches
- Hollow glass (US-402, 7.1): Negative radius trick not explicitly in research but aligns with implementation patterns
- Pinhole camera (US-601, 4.1): Look-at transformation at research lines 402-421; story matches
- Thin lens DoF (US-602, 4.2): Lens aperture sampling at research lines 426-448; story aligns
- Random supersampling (US-701, 10.2): Algorithm at research lines 956-969; story matches
- Stratified sampling (US-702, 10.3): Algorithm at research lines 972-987; story matches
- BVH (US-801, 9.2): Construction and traversal at research lines 843-879; story aligns with longest-axis midpoint split
- Gamma correction (US-1002, 11.4 and research section 11.4): Output = sqrt(color); AC matches research line 1118

**Verdict**: PASS with recommendations. All core capabilities covered; three medium/minor gaps identified but none are critical for Phase 0.

---

### 2. User Story Quality (INVEST Criteria)

**Evaluation**: PASS (all stories meet INVEST criteria)

#### Independence
- **Check**: Do stories depend on each other only for technical infrastructure, not for narrative completion?
- **Finding**: PASS. US-000 (walking skeleton) is the only foundational story. All others are independently valuable. Dependency graph in DOR checklist (section 8) is clean: US-000 → US-601/US-501/US-701/US-1002 (core infrastructure) → feature stories.
- **Example**: US-301 (metal) and US-401 (glass) both depend on US-501 (recursion depth) but can be completed independently.

#### Negotiable
- **Check**: Are stories outcome-focused, not prescriptive?
- **Finding**: PASS. Problem statements focus on user pain ("Elena's sphere floats with no shadow") not solutions ("implement shadow ray casting"). UAT scenarios describe observable outcomes ("ground plane area beneath the sphere is darker") not implementation steps.
- **Evidence**: US-203 problem statement (lines 453-456): "Elena's rendered sphere floats above the ground plane with no shadow, making the image look unrealistic" — pain point, not prescription. Technical notes are advisory, not mandatory.

#### Valuable
- **Check**: Does each story deliver something valuable to a persona?
- **Finding**: PASS. Each story serves one of four personas with clear motivation:
  - Elena (student): Progress visibility, understanding pipeline incrementally
  - David (hobbyist): Creative control, scene variety, visual quality
  - Sofia (technical artist): Performance, fidelity, control
  - Prof. Tanaka (instructor): Testability, pedagogical clarity

#### Estimable
- **Check**: Are stories sized such that the team can estimate effort?
- **Finding**: PASS. Estimated effort ranges 1-3 days (DOR section 6). Stories are small enough that experienced graphics programmers can estimate confidently. Technical notes clarify algorithms, reducing estimation uncertainty.
- **Evidence**: US-000 (~3 days) includes ray generation, intersection, shading, output; US-101 (~1 day) focuses solely on triangle intersection; US-301 (~2 days) is metal with recursion.

#### Small
- **Check**: Can each story be completed in 1-3 days?
- **Finding**: PASS. All stories fall in 1-3 day range (DOR section 6, table lines 132-153).

#### Testable
- **Check**: Are acceptance criteria checkable?
- **Finding**: PASS. Each story has 3-7 UAT scenarios in Given/When/Then format with measurable assertions.
- **Example**: US-000 scenario (lines 37-49): "When Elena runs the ray tracer executable / Then a file 'output.ppm' is created... And the pixel at the center (200, 112) is a shade of red (R > 100, G < 50, B < 50)" — directly verifiable.

**Verdict**: PASS. All 20 stories exhibit INVEST characteristics.

---

### 3. Acceptance Criteria Quality

**Status**: PASS with two medium recommendations

**Format Compliance**:
- All stories have 3-7 Given/When/Then scenarios (DOR section 4)
- Scenarios use concrete data (coordinates, RGB values, measurements)
- AC document provides detailed Must Have and Boundary/Edge criteria with verification methods

**Specificity and Measurability**:

| Story | Strengths | Issues |
|---|---|---|
| US-000 | Pixel value assertions (R > 100, G < 50), file format checks, shading variation checks | PASS: Highly specific |
| US-101 | Barycentric coordinate ranges (u >= 0, v >= 0, u + v <= 1), normal normalization (length = 1.0 ± epsilon) | PASS: Clear math |
| US-203 | Shadow acne epsilon definition (0.001), shadow shape (circular), brightness gradient ("darker than surrounding") | RECOMMENDATION: "Darker than surrounding" is subjective; suggest "< 50% illumination" |
| US-301 | Albedo tinting verification, recursion depth termination, reflections visible | PASS: Observable |
| US-602 | Aperture effects at three levels (0.1, 0.5, 2.0), blur comparison ("5+ pixels") | PASS: Quantified |
| US-701 | SPP scaling (1, 10, 100), render time linearity | PASS: Measurable |
| US-801 | Pixel-identical images between BVH and brute force, 2x speedup threshold | PASS: Testable |
| US-901 | YAML parsing, error messages with available names listed, usage help on no args | PASS: Verifiable |

**Issues Identified**:

1. **MEDIUM SEVERITY - Fuzzy AC in US-203 (Hard Shadows)**
   - **Evidence**: US-203 scenario (line 483): "ground plane area directly beneath the sphere (roughly x in [-0.5, 0.5], z in [-0.5, 0.5]) is darker than the surrounding illuminated plane"
   - **Issue**: "Darker than" is subjective. Different implementations might have slight variations in shadow softness due to SPP or epsilon value.
   - **Impact**: Test pass/fail becomes opinion-based rather than objective.
   - **Remediation**: Replace "darker than surrounding" with quantified metric.
   - **BAD**: "the ground plane area directly beneath the sphere is darker than the surrounding illuminated plane"
   - **GOOD**: "the ground plane area directly beneath the sphere has illumination < 50% of the unshaded plane, measurable as RGB values < 128 where unshaded is > 200"

2. **MEDIUM SEVERITY - AC-602-03 DoF verification relies on visual inspection**
   - **Evidence**: US-602 scenario (lines 998-1007): "Then aperture 0.1 shows minimal blur... aperture 0.5 shows moderate blur... aperture 2.0 shows extreme blur" — no quantitative blur measure
   - **Issue**: Visual inspection is subjective. No metric for "extreme blur."
   - **Impact**: Verification depends on human judgment; automatable test not defined.
   - **Remediation**: Define blur in terms of edge transition pixel count or standard deviation of gradients.
   - **BAD**: "aperture 2.0 shows extreme blur"
   - **GOOD**: "aperture 2.0 produces out-of-focus pixels with color transitions > 10 pixels (measured as full width at half-maximum of gradient)"

3. **MINOR - US-702 stratified sampling AC doesn't address non-square SPP**
   - **Evidence**: AC-702-06 (line 364): "Non-square SPP (e.g., 10) is handled (round to nearest perfect square or error)"
   - **Issue**: AC specifies a choice ("round or error") but doesn't define which behavior is correct.
   - **Impact**: Low; implementer must choose, but story should specify.
   - **Remediation**: Clarify in AC: "Non-square SPP values shall round to nearest perfect square (e.g., 10 → 9, 15 → 16)"

**Coverage Check (Happy Path + Edge Cases)**:
- **US-000**: Happy path (renders sphere and plane), edge cases (camera behind sphere, rays missing all objects, background color) — PASS
- **US-101**: Happy path (triangle renders), edge cases (ray parallel, outside triangle, degenerate triangle) — PASS
- **US-401**: Happy path (glass refracts light), edge cases (total internal reflection, IOR 1.0, critical angle) — PASS
- **US-801**: Happy path (pixel-identical BVH vs. brute force), edge cases (0 objects, 1 object, overlapping objects, degenerate split) — PASS

**Verdict**: PASS with two medium-severity recommendations. AC are generally specific and measurable; three issues identified for refinement.

---

### 4. Technical Accuracy Against Research

**Status**: PASS (all stories align with research documentation)

**Spot-Check Results**:

| Component | Story | Research Section | Alignment | Notes |
|---|---|---|---|---|
| Ray-sphere quadratic | US-000 | 3.1 (lines 183-208) | EXACT | Discriminant check, stable form recommended |
| Moller-Trumbore | US-101 | 3.3 (lines 236-294) | EXACT | Barycentric coords (u,v), edge checks match |
| Slab method AABB | US-102 | 3.5 (lines 310-324) | EXACT | Three axis pairs, t_min/t_max tracking correct |
| Triangle mesh smooth normals | US-103 | 3.4 (lines 303-306) | EXACT | Interpolation formula N = (1-u-v)*N0 + u*N1 + v*N2 |
| Multi-light accumulation | US-201 | 5.1, 5.2 (lines 470-487) | EXACT | Additive contributions, direct illumination |
| Shadow acne prevention | US-203 | 5.3 (lines 490-508) | EXACT | Epsilon offset on normal, t_max limit for point lights |
| Metal reflection formula | US-301 | 8.1 (line 772) | EXACT | R = I - 2*(I·N)*N |
| Glossy metal perturbation | US-302 | 8.2 (lines 790-799) | EXACT | Random vector in unit sphere, scaled by fuzziness |
| Snell's law refraction | US-401 | 7.2 (lines 654-667) | EXACT | sin2_theta_t check, total internal reflection condition |
| Schlick's approximation | US-401 | 7.5 (lines 699-736) | EXACT | F0 formula, (1-cos_theta)^5 term, probabilistic choice |
| Pinhole camera | US-601 | 4.1 (lines 402-421) | EXACT | Look-at basis construction, viewport calculation |
| Thin lens DoF | US-602 | 4.2 (lines 426-448) | EXACT | Random disc sampling, focus distance convergence |
| Supersampling | US-701 | 10.2 (lines 956-969) | EXACT | Random offset [0,1), averaging, gamma after average |
| Stratified sampling | US-702 | 10.3 (lines 972-987) | EXACT | Grid partition, jittering within strata |
| BVH construction | US-801 | 9.2 (lines 843-879) | EXACT | Longest-axis midpoint split, leaf max 4 primitives |
| Gamma correction | US-1002 | 11.4 (line 1118) | EXACT | output = sqrt(linear_color) |

**No technical errors detected.** All algorithms match research exactly. Numeric parameters (epsilon 0.001, recursion depth 10, max leaf size 4) are consistent with research and best practices.

**Verdict**: PASS. Stories demonstrate strong technical accuracy grounded in research documentation.

---

### 5. Story Ordering & Dependencies

**Status**: PASS

**Dependency Graph Validation**:

The DOR checklist (section 8, lines 173-197) correctly identifies all dependencies. Recommended implementation order (lines 202-243) is logically sound:

```
Phase 0: US-000 (foundation)
  ↓
Phase 1: US-601, US-501, US-701, US-1002 (core infrastructure)
  ↓
Phase 2: US-101, US-102, US-103 (primitives)
  ↓
Phase 3: US-201, US-202, US-203 (lighting/shadows)
  ↓
Phase 4: US-301, US-302, US-401, US-402 (materials)
  ↓
Phase 5: US-602, US-702 (advanced camera & sampling)
  ↓
Phase 6: US-801 (BVH acceleration)
  ↓
Phase 7: US-901 (scene loading)
  ↓
Phase 8: US-1001, (US-1002 parallel to Phase 1) (polish)
```

**Critical Path Analysis**:
- Walking skeleton (US-000) must precede all others ✓
- Recursion depth (US-501) should precede metals/glass (US-301, US-401) but can be implemented with US-000 ✓
- Primitives (US-101, US-102) can be developed in parallel once US-000 is complete ✓
- Lighting (US-201/202) should precede shadows (US-203) ✓
- BVH (US-801) requires all primitives/materials but is optional for correctness ✓

**No circular dependencies detected.** All dependencies are properly ordered.

**Verdict**: PASS. Story ordering is correct and critical path is clear.

---

### 6. Walking Skeleton Scope (US-000)

**Status**: PASS

**Appropriateness for End-to-End Validation**:

US-000 scope (requirements section 4.0, lines 37-56):
- 1 sphere + 1 ground plane (geometry)
- 1 point light (lighting)
- 1 pinhole camera (camera)
- Lambertian diffuse only (material)
- Ray-sphere and ray-plane intersection (core algorithm)
- PPM output (file I/O)
- Single sample per pixel, no acceleration (performance baseline)
- 400x225 resolution (small, quick)

**Validation Coverage**:
- ✓ End-to-end pipeline: scene → ray generation → intersection → shading → output
- ✓ Minimal: 2 primitives, 1 light, 1 material suffice to validate pipeline
- ✓ Observable result: recognizable image verifiable by multiple viewers
- ✓ Correctness baseline: sphere and plane shading can be verified mathematically

**Issues Identified**:

1. **MINOR - PPM output format should specify gamma correction explicitly**
   - **Evidence**: Requirements 4.10 (line 210): "Gamma correction (gamma 2.0: output = sqrt(linear_color)) must be applied before writing pixels."
   - **Issue**: US-000 technical notes (lines 102) mention gamma correction, but AC-000-09 (acceptance-criteria.md line 34) is the only test of gamma. It's appropriate but could be more prominent.
   - **Impact**: Low; gamma correction is documented in requirements and AC.
   - **Remediation**: No change needed; current specification is clear.

**Verdict**: PASS. US-000 is appropriately scoped as a true walking skeleton: minimal, complete, end-to-end.

---

### 7. DoR Compliance

**Status**: PASS (all 8 items satisfied)

**DoR Item Checklist** (from agent instructions):

| Item | Check | Result | Evidence |
|---|---|---|---|
| 1. Problem Statement Clear | All stories start with user pain, not tech prescription | PASS | E.g., US-203 (line 454): "Elena's sphere floats with no shadow, making the image look unrealistic" |
| 2. Persona Identified | 4 personas used consistently with characteristics | PASS | Requirements section 2; DOR section 2 (lines 74-79) |
| 3. 3+ Domain Examples | Every story has ≥3 examples with real data | PASS | E.g., US-301 examples (lines 544-551) use "David", "chrome sphere", "gold tint" |
| 4. UAT in Given/When/Then | 3-7 scenarios per story | PASS | DOR section 4 (lines 97-120) confirms all within range |
| 5. AC Derived from UAT | AC trace to scenarios | PASS | AC document (acceptance-criteria.md) mirrors story scenarios |
| 6. Right-Sized (1-3d, 3-7 scenarios) | Story size appropriate | PASS | DOR section 6 (lines 132-153) confirms all within bounds |
| 7. Technical Notes Present | Algorithms, data structures, references identified | PASS | Every story has Technical Notes section; e.g., US-101 (lines 170-173) |
| 8. Dependencies Resolved | No unmet or circular dependencies | PASS | DOR section 8 (lines 173-197) explicitly lists all; no cycles |

**Verdict**: PASS. All 8 DoR items satisfied for all 20 stories.

---

### 8. Gaps & Risks

**Identified Gaps**:

1. **Emissive Material Story (Capability Gap)**
   - **Issue**: Requirements define emissive material (4.4, line 114) but no dedicated story exists.
   - **Evidence**: Requirements mention it; user stories don't.
   - **Impact**: MEDIUM. Emissive materials are essential for area lights, HDR, and advanced scenes.
   - **Recommendation**: Create US-1003 for emissive material.

2. **Texture Mapping Deferred (Planned Gap)**
   - **Issue**: Requirements section 7 defers texture mapping to future increment.
   - **Evidence**: Line 264: "Texture mapping from image files (deferred to future increment)"
   - **Impact**: LOW. Correct scope decision for Phase 0. Research section 2.4 documents OBJ/glTF; texture mapping can follow.
   - **Recommendation**: Future increment should have dedicated story US-1004 or later.

3. **Tone Mapping Operators Limited**
   - **Issue**: US-1002 specifies Reinhard tone mapping (line 1504) but no other operators (ACES, filmic, etc.).
   - **Evidence**: AC-1002-04 (acceptance-criteria.md line 450): "Reinhard tone mapping compresses HDR values"
   - **Impact**: LOW. Reinhard is sufficient for Phase 0; other operators can be future enhancements.
   - **Recommendation**: Document in requirements section 7 (Out of Scope) that other tone mapping operators are future work.

**Identified Risks**:

1. **Shadow Acne Epsilon (0.001) Might Not Suit All Scenes**
   - **Risk**: Different geometries (tiny vs. large objects) might require different epsilon values.
   - **Evidence**: US-203 technical notes (line 520): "shadow_origin = hit_point + 0.001 * normal"
   - **Mitigation**: AC-203-07 (acceptance-criteria.md line 178) specifies epsilon prevents acne without detachment; testing on varied scenes during implementation.
   - **Recommendation**: Consider making epsilon configurable per-scene or adaptive (PBRT research 34).

2. **Recursion Depth Default (10) May Be Insufficient for Some Scenes**
   - **Risk**: Deep glass/metal chains might need depth > 10; other scenes might not need 10.
   - **Evidence**: US-501 technical notes (line 881): "default max_depth = 10"
   - **Mitigation**: AC-501-04 allows configuration (line 274); users can adjust.
   - **Recommendation**: Document recommended values for different material combinations in scene file template.

3. **BVH Longest-Axis Midpoint Split May Degrade for Non-Uniform Scenes**
   - **Risk**: Worst-case O(N) traversal for degenerate geometry (all objects in one axis).
   - **Evidence**: US-801 technical notes (line 1232): "longest-axis midpoint split (initial implementation)"
   - **Mitigation**: AC-801-03 (acceptance-criteria.md line 378) requires 2x speedup only for 50+ object scenes; SAH is documented for future optimization (line 1240).
   - **Recommendation**: Plan SAH optimization as future story if performance issues arise.

4. **Stratified Sampling SPP Must Be Perfect Square**
   - **Risk**: If user specifies SPP=10 or 17, behavior is undefined (round or error).
   - **Evidence**: AC-702-06 (acceptance-criteria.md line 364): "Round to nearest perfect square or error"
   - **Mitigation**: AC specifies a choice; implementer must document their decision.
   - **Recommendation**: Clarify in AC-702-06 that rounding is required (e.g., 10→9, 17→16) rather than error.

**Verdict**: PASS. Gaps and risks are documented and mitigated appropriately for Phase 0. One medium-priority gap (emissive material) recommended for future story.

---

### 9. Overall Assessment

**APPROVED for DESIGN wave handoff**

**Summary of Findings**:

| Category | Status | Count | Severity |
|---|---|---|---|
| DoR Items (8/8) | PASS | 8 passing | — |
| Requirements Coverage | PASS | 20/20 stories | Minor gaps: emissive material |
| INVEST Criteria | PASS | 20/20 stories | All meet criteria |
| AC Quality | PASS | 65/65 AC | 3 medium recommendations |
| Technical Accuracy | PASS | 16 spot checks | All align with research |
| Story Ordering | PASS | 20 stories | No circular dependencies |
| Walking Skeleton | PASS | US-000 | Appropriately scoped |
| Antipatterns | PASS | 8/8 checked | None detected |

**Issues Found**:
- **CRITICAL**: 0
- **MAJOR**: 0
- **MEDIUM**: 3 (all in AC refinement, not blocking)
- **MINOR**: 3 (non-blocking recommendations)

**Recommendations**:

1. **High Priority** (Pre-implementation):
   - Refine AC-203 shadow darkness metric from "darker than" to quantified threshold (e.g., < 50% illumination)
   - Refine AC-602 depth-of-field blur measurement from visual inspection to pixel-count metric (edge transition > 10px)
   - Create US-1003 for emissive material implementation

2. **Medium Priority** (During implementation):
   - Document recommended recursion depth values for different material combinations
   - Consider epsilon adaptivity for shadow ray offset based on scene scale
   - Plan SAH optimization for future BVH enhancement

3. **Low Priority** (Post-Phase 0):
   - Add OBJ/glTF loader stories to future increment
   - Expand tone mapping operator library beyond Reinhard

**Confidence**: This review is based on comprehensive analysis of:
- 4 detailed artifact documents (2,500+ lines)
- 1 comprehensive research document (1,300+ lines)
- Spot-checking 16 technical claims against research
- Validation of all 8 DoR items across 20 stories
- INVEST criteria assessment
- Dependency graph analysis

**Handoff Status**: READY

This artifact set is well-prepared for the DESIGN wave. All user stories are INVEST-compliant, technically sound, properly sized, and free of critical issues. The three medium-severity AC refinements can be addressed as task items during DESIGN refinement without blocking the handoff.

---

**Review Metadata**:
- **Reviewer Role**: nw-product-owner-reviewer (LeanUX Quality Gate)
- **Review Method**: Comprehensive DoR validation + technical accuracy cross-reference with research
- **Artifacts Reviewed**: requirements.md, user-stories.md, acceptance-criteria.md, dor-checklist.md, ray-tracing-comprehensive-research.md
- **Total Stories Reviewed**: 20 (US-000 through US-1002)
- **Total AC Reviewed**: 65 (Must Have + Boundary tiers)
- **Stories Ready for Handoff**: 20/20 (100%)
