# Ray Tracer -- Detailed Acceptance Criteria

**Document ID**: AC-RAYTRACER-001
**Date**: 2026-02-16
**Status**: Draft -- Pending DoR Validation

---

## How to Read This Document

Each story's acceptance criteria are organized into three tiers:
- **Must Have**: Core criteria that define the story as done. All must pass.
- **Boundary/Edge**: Criteria covering edge cases and error conditions. All must pass.
- **Verification Method**: How to confirm the criterion is met (unit test, visual inspection, measurement).

---

## Feature 0: Walking Skeleton

### US-000: Render a Single Sphere on a Plane to PPM

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-000-01 | Running the executable with no arguments produces a file "output.ppm" in the current directory | Run executable; check file exists |
| AC-000-02 | The PPM file starts with header "P3\n400 225\n255\n" (magic, dimensions, max color) | Parse first 3 lines of output.ppm |
| AC-000-03 | The PPM file contains exactly 400 x 225 = 90,000 pixel RGB triplets after the header | Count triplets in file |
| AC-000-04 | Every RGB value in the file is an integer in [0, 255] | Parse and validate all values |
| AC-000-05 | The sphere is visible: pixels at the image center show red-dominant color (R > 2*G and R > 2*B) | Sample pixel at (200, 112) |
| AC-000-06 | The ground plane is visible: pixels below the sphere show gray color (R approximately equals G approximately equals B, all > 30) | Sample pixels at (200, 180) |
| AC-000-07 | Shading varies on the sphere: the side facing the light is brighter than the side facing away | Compare pixel R-values at sphere left-center vs. sphere right-center |
| AC-000-08 | Background pixels (above the sphere) are distinct from sphere and plane colors | Sample pixel at (200, 10) |
| AC-000-09 | Gamma correction is applied: midtone values are perceptually correct (not too dark) | Compare output of linear value 0.5 against expected gamma-corrected value ~181 |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-000-10 | The executable completes without crashing (exit code 0) | Check process exit code |
| AC-000-11 | Render time is under 5 seconds on a modern laptop (400x225, 1 SPP, 2 objects) | Time the execution |
| AC-000-12 | The PPM file opens correctly in GIMP and at least one other viewer | Manual visual check |
| AC-000-13 | No NaN values appear in the output (no pixel with value outside [0, 255]) | Parse all values |

---

## Feature 1: Additional Geometric Primitives

### US-101: Render Triangles in a Scene

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-101-01 | Triangle is defined by 3 vertex positions (Vec3 A, Vec3 B, Vec3 C) | Unit test: construct triangle |
| AC-101-02 | Ray hitting inside the triangle returns a valid HitRecord with t > 0 | Unit test: ray through triangle center |
| AC-101-03 | Ray hitting the triangle's plane but outside the triangle returns no hit | Unit test: ray through point outside triangle |
| AC-101-04 | Surface normal is perpendicular to the triangle face and normalized | Unit test: verify dot(normal, edge1) = 0 and length(normal) = 1 |
| AC-101-05 | Barycentric coordinates (u, v) satisfy u >= 0, v >= 0, u + v <= 1 at hit point | Unit test: verify coordinate ranges |
| AC-101-06 | Triangle provides a bounding box enclosing all 3 vertices | Unit test: verify AABB contains all vertices |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-101-07 | Ray parallel to triangle plane returns no hit (determinant near zero) | Unit test: parallel ray |
| AC-101-08 | Ray hitting triangle edge (u = 0 or v = 0 or u + v = 1) is handled consistently | Unit test: ray along edge |
| AC-101-09 | Degenerate triangle (two vertices identical) returns no hit | Unit test: degenerate triangle |
| AC-101-10 | Back-face detection: front_face flag is correctly set based on ray direction vs. normal | Unit test: ray from both sides |

---

### US-102: Render Axis-Aligned Boxes

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-102-01 | Box is defined by min corner and max corner (Vec3, Vec3) | Unit test: construct box |
| AC-102-02 | Ray hitting the box returns a valid HitRecord with correct t | Unit test: ray through box center |
| AC-102-03 | Surface normal at hit point identifies the struck face (one of 6 axis-aligned normals) | Unit test: verify normal is axis-aligned and outward |
| AC-102-04 | Ray missing the box returns no hit | Unit test: ray passing beside box |
| AC-102-05 | Box provides a bounding box (itself) for BVH compatibility | Unit test: bounding_box() == box bounds |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-102-06 | Ray originating inside the box hits the exit face with front_face = false | Unit test: origin inside box |
| AC-102-07 | If min > max, the box swaps coordinates to normalize | Unit test: inverted corners |
| AC-102-08 | Ray along a face (grazing) is handled without division by zero | Unit test: ray along box face |
| AC-102-09 | A box with zero volume in one axis (flat box) still intersects correctly | Unit test: flat box |

---

### US-103: Render Triangle Meshes with Smooth Shading

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-103-01 | TriangleMesh stores a list of vertices, a list of per-vertex normals, and a list of triangle indices | Unit test: construct mesh |
| AC-103-02 | Hit point normal is barycentric interpolation of per-vertex normals: N = (1-u-v)*N0 + u*N1 + v*N2 | Unit test: verify interpolated normal against manual computation |
| AC-103-03 | Interpolated normal is re-normalized (length = 1.0 within epsilon) | Unit test: verify normalization |
| AC-103-04 | When per-vertex normals are absent, the face normal (cross product of edges) is used | Unit test: mesh without normals |
| AC-103-05 | TriangleMesh bounding box encloses all vertices | Unit test: verify AABB |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-103-06 | Mesh with 1 triangle behaves identically to a standalone Triangle | Unit test: single-triangle mesh |
| AC-103-07 | Smooth shading across a 42-vertex icosphere produces no visible facet edges | Visual inspection of rendered icosphere |
| AC-103-08 | Mesh with inconsistent winding order still renders (normals may need correction) | Unit test: mixed winding mesh |

---

## Feature 2: Enhanced Lighting

### US-201: Support Multiple Point Lights

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-201-01 | Scene stores a list of 0 or more lights | Unit test: scene with 0, 1, and 3 lights |
| AC-201-02 | Each light contributes independently: final_color = sum(light_contributions) | Unit test: verify additive contribution |
| AC-201-03 | Shadow rays are cast independently per light | Unit test: one light occluded, another not |
| AC-201-04 | Colored lights tint surfaces: contribution = albedo * light_color * intensity * max(0, dot(N, L)) | Unit test: red light on white surface produces red |
| AC-201-05 | Scene with 0 lights produces a valid image (ambient-only or black) | Visual inspection |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-201-06 | Two lights at the same position produce 2x the brightness of a single light | Unit test: compare pixel values |
| AC-201-07 | A light behind the surface (dot(N, L) < 0) contributes zero | Unit test: light behind surface |

---

### US-202: Directional Light Support

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-202-01 | Directional light is defined by direction vector, color, and intensity (no position) | Unit test: construct directional light |
| AC-202-02 | Shadow rays travel in -light_direction with t_max = infinity | Unit test: verify shadow ray direction and range |
| AC-202-03 | Two objects at different positions cast parallel shadows | Visual inspection of rendered image |
| AC-202-04 | Directional light integrates with existing point lights in the shading loop | Unit test: scene with both light types |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-202-05 | Object very far away in the light direction still casts shadow (t_max = infinity) | Unit test: distant occluder |
| AC-202-06 | No distance-based attenuation for directional lights | Unit test: verify constant intensity |

---

### US-203: Hard Shadows from Point and Directional Lights

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-203-01 | Shadow ray is cast from hit_point + epsilon * normal toward each light | Unit test: verify shadow ray origin and direction |
| AC-203-02 | If any object intersects the shadow ray in (0, t_light), the light contribution is zero | Unit test: occluded point vs. unoccluded point |
| AC-203-03 | Epsilon offset prevents shadow acne (no speckled dark spots on flat surfaces) | Visual inspection of rendered plane |
| AC-203-04 | Objects beyond the light (t > t_light) do not cast shadows | Unit test: occluder behind light |
| AC-203-05 | Self-shadow on curved surfaces produces smooth terminator | Visual inspection of sphere |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-203-06 | Shadow ray uses "any hit" early exit (does not need closest hit) | Code review / performance measurement |
| AC-203-07 | Epsilon value (0.001) prevents acne without visibly detaching shadows from objects | Visual inspection |

---

## Feature 3: Material System

### US-301: Metal Material with Mirror Reflection

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-301-01 | Reflected direction R = I - 2*dot(I, N)*N is correctly computed | Unit test: verify reflection vector for known inputs |
| AC-301-02 | Reflected color is attenuated by metal albedo (component-wise multiply) | Unit test: gold tint on reflected white surface |
| AC-301-03 | At max recursion depth, trace_ray returns (0, 0, 0) | Unit test: verify termination |
| AC-301-04 | Metal sphere shows reflections of surrounding objects | Visual inspection of rendered scene |
| AC-301-05 | Albedo (1,1,1) produces untinted reflections; (0.8, 0.6, 0.2) produces gold tint | Visual comparison |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-301-06 | Scattered ray origin is offset by epsilon to prevent self-intersection | Unit test: verify ray origin offset |
| AC-301-07 | Two facing mirrors produce diminishing reflections up to max depth | Visual inspection |

---

### US-302: Glossy (Fuzzy) Metal Reflections

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-302-01 | Fuzziness parameter accepted in range [0.0, 1.0], clamped if outside | Unit test: fuzziness 1.5 clamps to 1.0 |
| AC-302-02 | Fuzziness 0.0 produces mirror-sharp reflections (identical to US-301) | Visual comparison |
| AC-302-03 | Fuzziness > 0 perturbs reflected direction: reflected + fuzziness * random_in_unit_sphere() | Code verification |
| AC-302-04 | Rays perturbed below the surface (dot(perturbed, normal) <= 0) are absorbed | Unit test: verify absorption |
| AC-302-05 | Higher fuzziness produces visually blurrier reflections (at 50+ SPP) | Visual comparison of fuzziness 0.0, 0.3, 0.8 |

---

## Feature 4: Transparency and Refraction

### US-401: Dielectric (Glass) Material with Refraction

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-401-01 | Refraction direction follows Snell's law: sin(theta_t) = (n1/n2) * sin(theta_i) | Unit test: verify refracted direction for known angle and IOR |
| AC-401-02 | Schlick reflectance at normal incidence for glass (IOR 1.5) is approximately 0.04 | Unit test: verify Schlick computation |
| AC-401-03 | Total internal reflection occurs when eta * sin(theta_i) > 1.0 | Unit test: steep angle inside glass |
| AC-401-04 | Ray entering medium (front_face = true) uses eta = 1.0/ior; exiting uses eta = ior | Unit test: verify eta selection |
| AC-401-05 | Glass sphere (IOR 1.5) shows see-through transparency with refraction distortion | Visual inspection |
| AC-401-06 | IOR 1.0 produces an effectively invisible sphere (no bending, no reflection) | Visual inspection |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-401-07 | Schlick reflectance approaches 1.0 at grazing angles | Unit test: cos_theta near 0 |
| AC-401-08 | No NaN or infinite values from refraction at any angle | Unit test: sweep angles 0-90 degrees |
| AC-401-09 | Max recursion depth prevents infinite bouncing inside glass | Unit test: verify termination |

---

### US-402: Hollow Glass Sphere

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-402-01 | Sphere with negative radius inverts its surface normal | Unit test: verify normal direction for negative radius |
| AC-402-02 | Outer + inner sphere combination produces hollow glass appearance | Visual inspection |
| AC-402-03 | Hollow sphere is visually distinct from solid sphere (less central distortion) | Visual comparison |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-402-04 | Inner sphere radius magnitude < outer sphere radius (fits inside) | Logical verification |
| AC-402-05 | Multiple nested shells produce correct multi-layer refraction | Visual inspection |

---

## Feature 5: Reflections

### US-501: Recursive Reflection with Configurable Depth

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-501-01 | max_depth is configurable (integer, range 0-100, default 10) | Unit test: set and verify |
| AC-501-02 | At max depth, trace_ray returns Color(0, 0, 0) | Unit test: verify return at depth limit |
| AC-501-03 | Depth 1 shows single-bounce reflections; depth 10 shows 10 levels | Visual comparison |
| AC-501-04 | Depth 0 produces an entirely black image | Visual inspection |
| AC-501-05 | Two parallel mirrors at depth 100 complete without stack overflow | Run and verify completion |

---

## Feature 6: Camera Enhancements

### US-601: Configurable Pinhole Camera

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-601-01 | Camera accepts lookfrom, lookat, vup, vfov (degrees), aspect_ratio | Unit test: construct camera |
| AC-601-02 | Orthonormal basis (u, v, w) is correctly computed from parameters | Unit test: verify basis is orthonormal |
| AC-601-03 | Changing lookfrom moves the viewpoint (different perspective of same scene) | Visual comparison |
| AC-601-04 | Changing vfov adjusts zoom (smaller = narrower = more zoom) | Visual comparison |
| AC-601-05 | Changing vup rotates the image (camera roll) | Visual comparison |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-601-06 | lookfrom == lookat is handled gracefully (no division by zero; error or default) | Unit test: degenerate input |
| AC-601-07 | vup parallel to view direction is handled (degenerate cross product) | Unit test: vup = (0,0,-1) with camera looking along -z |
| AC-601-08 | vfov near 0 or near 180 produces valid (extreme) images | Unit test: vfov = 1 and vfov = 170 |

---

### US-602: Thin Lens Camera with Depth of Field

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-602-01 | Camera accepts aperture and focus_distance in addition to pinhole params | Unit test: construct thin lens camera |
| AC-602-02 | Ray origins are randomly distributed on a disc of diameter = aperture | Unit test: verify ray origin distribution |
| AC-602-03 | Objects at focus_distance are sharp; others are blurred proportionally | Visual inspection at 100 SPP |
| AC-602-04 | Aperture = 0 produces pinhole-equivalent result (everything in focus) | Visual comparison |
| AC-602-05 | Larger aperture produces more blur on out-of-focus objects | Visual comparison at aperture 0.1, 0.5, 2.0 |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-602-06 | Focus distance > 0 (zero or negative is rejected or handled) | Unit test: invalid focus distance |
| AC-602-07 | Depth of field requires multiple SPP to be visible (SPP = 1 looks identical to pinhole) | Visual inspection |

---

## Feature 7: Anti-Aliasing and Sampling

### US-701: Random Supersampling Anti-Aliasing

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-701-01 | SPP is configurable (default 10, range 1-10000) | Unit test: set and verify |
| AC-701-02 | Each sample ray has a random offset within [px, px+1) x [py, py+1) | Unit test: verify offset range |
| AC-701-03 | Final pixel color is the arithmetic mean of all samples | Unit test: verify averaging |
| AC-701-04 | Higher SPP produces smoother edges | Visual comparison at 1, 10, 100 SPP |
| AC-701-05 | SPP = 1 produces a valid image | Run and verify output |
| AC-701-06 | Gamma correction applied after averaging, not per-sample | Code review |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-701-07 | Render time scales approximately linearly with SPP | Time measurement at 1, 10, 100 SPP |
| AC-701-08 | SPP = 10000 completes without memory issues (no per-sample storage needed) | Run at 10000 SPP |

---

### US-702: Stratified (Jittered) Sampling

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-702-01 | Pixel is divided into sqrt_spp x sqrt_spp strata | Unit test: verify stratum count |
| AC-702-02 | Each stratum gets exactly one jittered sample | Unit test: verify one sample per stratum |
| AC-702-03 | Total samples = sqrt_spp^2 | Unit test: verify total count |
| AC-702-04 | Stratified produces less noise than random at equal SPP | Visual comparison at 16 SPP |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-702-05 | sqrt_spp = 1 produces single-sample result | Unit test: verify degeneracy |
| AC-702-06 | Non-square SPP (e.g., 10) is handled (round to nearest perfect square or error) | Unit test: non-square input |

---

## Feature 8: Acceleration Structures

### US-801: BVH for Scene Acceleration

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-801-01 | BVH is constructed before rendering (not per-ray) | Code review / timing |
| AC-801-02 | BVH renders produce pixel-identical results to brute force | Compare images at byte level |
| AC-801-03 | BVH provides at least 2x speedup for 50+ object scenes | Time comparison |
| AC-801-04 | Every Hittable provides bounding_box() | Code review |
| AC-801-05 | Construction uses longest-axis midpoint split | Code review |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-801-06 | 0 objects: BVH is empty, render produces background-only image | Unit test |
| AC-801-07 | 1 object: BVH has single leaf node, renders correctly | Unit test |
| AC-801-08 | All objects at same position: BVH handles degenerate split | Unit test |
| AC-801-09 | Leaf nodes contain at most 4 primitives | Code review / unit test |

---

## Feature 9: Scene File Loading

### US-901: Load Scene from YAML

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-901-01 | Executable accepts YAML file path as command-line argument | Run: ./raytracer scene.yaml |
| AC-901-02 | Camera, lights, materials, objects, and render settings are parsed | Unit test: parse known YAML |
| AC-901-03 | All supported primitives and materials are loadable | Unit test: one of each type |
| AC-901-04 | Undefined material reference produces error with available names listed | Unit test: bad material reference |
| AC-901-05 | Missing required field produces error identifying the field and object | Unit test: missing radius |
| AC-901-06 | Running with no arguments prints usage instructions | Run with no args |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-901-07 | Empty scene file (no objects) produces background-only image | Unit test |
| AC-901-08 | Malformed YAML produces a readable parse error (not a crash) | Unit test: invalid YAML syntax |
| AC-901-09 | Very large scene file (1000 objects) is parsed without issues | Performance test |

---

## Feature 10: Advanced Features

### US-1001: Area Lights with Soft Shadows

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-1001-01 | Area light defined by position, dimensions, normal, color, intensity | Unit test: construct area light |
| AC-1001-02 | Multiple shadow rays cast to random points on light surface | Code review / unit test |
| AC-1001-03 | Shadow softness = unoccluded_rays / total_shadow_rays | Unit test: verify fraction |
| AC-1001-04 | Visible penumbra around shadows from area lights | Visual inspection |
| AC-1001-05 | More shadow samples produce smoother penumbra | Visual comparison at 4 vs. 64 samples |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-1001-06 | Near-zero size area light behaves like point light (hard shadows) | Visual comparison |
| AC-1001-07 | Shadow sample count is configurable per light | Unit test |

---

### US-1002: Gamma Correction and Tone Mapping

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-1002-01 | Gamma 2.0 (sqrt) applied to all pixel colors by default | Unit test: linear 0.25 maps to ~0.5 (128 out of 255) |
| AC-1002-02 | RGB values clamped to [0, 255] after gamma | Unit test: verify no values outside range |
| AC-1002-03 | Gamma-corrected image is visibly brighter in midtones than linear output | Visual comparison |
| AC-1002-04 | Reinhard tone mapping compresses HDR values: mapped = color / (1 + color) | Unit test: verify mapping for value 5.0 |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-1002-05 | NaN values are clamped to 0 (no NaN in output) | Unit test: NaN input |
| AC-1002-06 | Negative color values are clamped to 0 | Unit test: negative input |
