# GPU Compute Rendering -- Detailed Acceptance Criteria

**Document ID**: AC-GPU-COMPUTE-001
**Date**: 2026-02-17
**Status**: Draft -- Pending DoR Validation

---

## How to Read This Document

Each story's acceptance criteria are organized into three tiers:
- **Must Have**: Core criteria that define the story as done. All must pass.
- **Boundary/Edge**: Criteria covering edge cases and error conditions. All must pass.
- **Verification Method**: How to confirm the criterion is met (unit test, visual inspection, measurement).

---

## Phase 0: Walking Skeleton

### US-GPU-000: Render a Flat-Color Frame via Metal Compute

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-000-01 | `--backend=metal` flag is accepted by CliDispatcher and stored in RenderCommand.backend | Unit test: parse CLI args, assert backend field |
| AC-GPU-000-02 | Metal device (`MTLCreateSystemDefaultDevice()`) initializes successfully on macOS with Metal GPU | Integration test on macOS: assert device is non-null |
| AC-GPU-000-03 | A .metal compute shader file compiles to .metallib during CMake build | Build system test: verify .metallib exists in build output |
| AC-GPU-000-04 | Compute shader dispatches a 2D grid matching image dimensions (width x height) | Integration test: dispatch returns without error for 400x225 |
| AC-GPU-000-05 | GPU texture is allocated at requested dimensions and pixel data is read back to CPU | Integration test: readback produces width*height pixels with valid values |
| AC-GPU-000-06 | Output PPM file is valid: P3 header, correct dimensions, all RGB values in [0, 255] | Parse output file; validate header and value ranges |
| AC-GPU-000-07 | Output image shows a horizontal gradient (left pixels dark, right pixels bright) | Pixel sampling: assert pixel(0, H/2) is near-black; pixel(W-1, H/2) is near-blue |
| AC-GPU-000-08 | Default backend (no --backend flag) uses CPU renderer with zero behavioral change | Regression test: render without flag, compare output to pre-feature baseline |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-000-09 | On non-macOS (Linux), `--backend=metal` prints "Metal backend is only available on macOS" and exits with code 1 | Build and run on Linux; check stderr and exit code |
| AC-GPU-000-10 | Metal initialization + dispatch + readback + PPM write completes in under 1 second for 3840x2160 | Timing measurement on M-series Mac |
| AC-GPU-000-11 | Threadgroup size (16x16) dispatches correctly for non-power-of-2 image dimensions (e.g., 400x225) | Integration test: render at 400x225, verify complete image (no missing rows/columns) |
| AC-GPU-000-12 | No memory leaks from Metal objects (device, command queue, buffer, texture released) | Run with `leaks` tool or Instruments on macOS |

---

## Phase 1: Render Backend Abstraction

### US-GPU-001: Select Render Backend via CLI

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-001-01 | RenderBackend abstract class exists in Ring 3 with pure virtual `render(camera, scene, settings) -> vector<Color3>` | Compile test: verify class exists and is abstract |
| AC-GPU-001-02 | CpuRenderBackend wraps existing Renderer and produces byte-identical output to Renderer::render() | Unit test: render same scene with both; compare pixel arrays |
| AC-GPU-001-03 | `--backend=cpu` explicitly selects CPU and produces identical output to no --backend flag | Integration test: compare PPM outputs |
| AC-GPU-001-04 | `--backend=metal` on macOS selects MetalRenderBackend | Integration test: verify Metal device is initialized when flag is used |
| AC-GPU-001-05 | RenderCommand.backend field defaults to empty string (meaning CPU) | Unit test: parse CLI args without --backend, assert empty string |
| AC-GPU-001-06 | AnimationRenderer WriteCallback works with MetalRenderBackend (frame files produced) | Integration test: run 3-frame animation with --backend=metal |
| AC-GPU-001-07 | Unknown backend value (e.g., "vulkan") prints "Error: unknown backend 'vulkan'. Available: cpu, metal" and exits with code 1 | Unit test: parse --backend=vulkan, verify error message |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-001-08 | All 243 existing tests pass without modification after RenderBackend refactor | Run full test suite; assert zero failures |
| AC-GPU-001-09 | Renderer public API (render(), trace_ray(), set_quiet()) remains available for backward compatibility | Compile test: existing code using Renderer directly still compiles |
| AC-GPU-001-10 | RenderBackend is in `application/` directory (Ring 3); MetalRenderBackend is in `infrastructure/` (Ring 4) | File path check; no Metal imports in Ring 3 |

---

## Phase 2: Ray Generation and Sky on GPU

### US-GPU-002: Generate Camera Rays and Render Sky Gradient on GPU

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-002-01 | Camera parameters (lookfrom, lookat, vup, vfov, aspect_ratio, image_width, image_height, pixel00_loc, pixel_delta_u, pixel_delta_v) are packed into a Metal buffer | Unit test: verify buffer contents match Camera member values |
| AC-GPU-002-02 | GPU ray generation for pixel (px, py) produces the same direction as CPU Camera::generate_ray(px, py) within float precision tolerance | Unit test: compare GPU vs CPU ray directions for 10 sample pixels; delta < 1e-5 |
| AC-GPU-002-03 | Sky gradient formula on GPU matches CPU: `(1-a)*bottom + a*top` where `a = 0.5*(normalize(dir).y + 1)` | Pixel comparison: GPU vs CPU empty scene at 400x225, max per-channel diff <= 1 (rounding) |
| AC-GPU-002-04 | GPU-rendered empty scene at 1 SPP is pixel-identical to CPU-rendered empty scene at 1 SPP (within +/-1 per channel for float rounding) | Integration test: render both, compare pixel arrays |
| AC-GPU-002-05 | Camera buffer struct layout is documented with field offsets and types | Code comment or header doc in camera buffer definition |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-002-06 | GPU handles non-standard camera orientations: vup=(1,0,0) produces rotated sky gradient | Visual check: rotated gradient matches CPU output |
| AC-GPU-002-07 | GPU handles narrow FOV (vfov=20): zoomed-in sky matches CPU output | Pixel comparison at narrow FOV |
| AC-GPU-002-08 | Float precision (GPU) vs double precision (CPU) introduces no visible artifacts in the sky gradient | Visual inspection: no banding or color jumps in GPU gradient |

---

## Phase 3: Scene Data Packing

### US-GPU-003: Flatten Scene into GPU-Uploadable Buffers

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-003-01 | GPUShape tagged union covers Sphere, Plane, Box, Cylinder, Triangle with correct parameters | Unit test: flatten scene with each type, verify parameters |
| AC-GPU-003-02 | GPUMaterial tagged union covers Lambertian (albedo), Metal (albedo, fuzziness), Dielectric (IOR), Emissive (emission color, intensity) | Unit test: flatten scene with each material type, verify parameters |
| AC-GPU-003-03 | GPULight tagged union covers Point (position, color, intensity) and Directional (direction, color, intensity) | Unit test: flatten scene with each light type, verify parameters |
| AC-GPU-003-04 | Each GPUShape has a material_index that correctly references the GPUMaterial array | Unit test: verify index lookups resolve to expected material |
| AC-GPU-003-05 | Shared material instances are deduplicated: 3 shapes sharing 1 material produce 1 GPUMaterial entry | Unit test: 3 spheres with same material, assert GPUMaterial array length = 1 |
| AC-GPU-003-06 | TransformedShape stores the inverse transformation matrix in the GPUShape entry | Unit test: flatten TransformedShape, verify matrix values match |
| AC-GPU-003-07 | GPUShape, GPUMaterial, GPULight structs have no Metal framework dependencies (compile on Linux) | Build on Linux; all struct headers compile without error |
| AC-GPU-003-08 | Structs are 16-byte aligned for Metal buffer compatibility | Static assert: sizeof(GPUShape) % 16 == 0, etc. |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-003-09 | Empty scene (0 shapes, 0 lights) produces zero-length arrays without crash | Unit test: flatten empty scene |
| AC-GPU-003-10 | Scene with 1000 shapes flattens in under 10ms | Benchmark: time flattening of 1000-shape scene |
| AC-GPU-003-11 | Null material pointer on a shape defaults to material_index = 0 (or documented fallback) | Unit test: shape with nullptr material |
| AC-GPU-003-12 | TriangleMesh shapes are skipped or logged as unsupported (not silently dropped) | Unit test: flatten scene with TriangleMesh, verify warning and shape count |

---

## Phase 4: Single-Bounce GPU Rendering

### US-GPU-004: Render a Recognizable Scene on GPU with Diffuse Shading

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-004-01 | GPU compute shader intersects rays with Sphere shapes using quadratic formula | Unit test: GPU renders scene with sphere, sphere is visible at correct position |
| AC-GPU-004-02 | GPU compute shader intersects rays with Plane shapes using dot-product formula | Unit test: GPU renders scene with ground plane, plane is visible |
| AC-GPU-004-03 | GPU compute shader intersects rays with Box shapes using slab method | Unit test: GPU renders scene with box, box is visible with distinct face shading |
| AC-GPU-004-04 | Lambertian diffuse shading: `albedo * max(0, dot(N, L)) * light_intensity` | Visual comparison: GPU sphere has bright side toward light, dark side away |
| AC-GPU-004-05 | Shadow rays cast from hit point with epsilon offset (0.001 * normal) | Visual check: shadows appear beneath objects, no shadow acne visible |
| AC-GPU-004-06 | Ambient term (0.05 * albedo) applied to shaded surfaces matching CPU renderer | Pixel comparison: shadow regions have non-zero color |
| AC-GPU-004-07 | Miss rays produce sky gradient (regression from US-GPU-002) | Visual check: background above scene shows gradient |
| AC-GPU-004-08 | GPU output is visually comparable to CPU output for a 3-sphere scene at 1 SPP | Side-by-side comparison: sphere positions, silhouettes, and shading patterns match |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-004-09 | GPU handles scene with 0 shapes (background only) correctly | Integration test: empty scene produces sky gradient |
| AC-GPU-004-10 | GPU handles scene with 1 shape and 0 lights (ambient only) | Integration test: shape is visible at ambient level |
| AC-GPU-004-11 | GPU float precision produces per-pixel RGB within +/-10 of CPU double precision for 1 SPP | Pixel comparison script over 90% of pixels |
| AC-GPU-004-12 | GPU brute-force scan correctly finds closest intersection (smallest positive t) | Integration test: overlapping shapes render front-most visible |

---

## Phase 5: Multi-Bounce Ray Tracing

### US-GPU-005: Iterative Multi-Bounce Ray Tracing on GPU

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-005-01 | Metal material reflects rays: R = I - 2*(I dot N)*N on GPU | Visual check: metal sphere shows reflections of nearby objects |
| AC-GPU-005-02 | Metal fuzziness: reflected direction perturbed by `fuzziness * random_in_unit_sphere()` | Visual check: fuzzy=0.3 sphere shows blurry reflections at 16+ SPP |
| AC-GPU-005-03 | Dielectric refraction: Snell's law `eta = front_face ? (1/ior) : ior` applied on GPU | Visual check: glass sphere shows see-through with distortion |
| AC-GPU-005-04 | Schlick's reflectance: `F0 + (1-F0)*(1-cos_theta)^5` determines reflect vs refract probability | Visual check: glass sphere edges are more reflective than center |
| AC-GPU-005-05 | Total internal reflection: when `sin2_theta_t > 1.0`, ray reflects instead of refracting | Unit test: steep-angle ray inside glass sphere reflects |
| AC-GPU-005-06 | Emissive material contributes emitted color and does not scatter | Visual check: emissive sphere appears bright; nearby surfaces show color bleeding |
| AC-GPU-005-07 | max_depth is respected: loop terminates, returns black when bounce count exhausted | Integration test: max_depth=1 shows only direct light, max_depth=5 shows reflections |
| AC-GPU-005-08 | Multi-light accumulation: each hit point sums contributions from all lights | Visual check: scene with 2 lights shows dual shadow/illumination |
| AC-GPU-005-09 | GPU renders with all 4 material types are visually comparable to CPU at same SPP | Side-by-side comparison for scene with Lambertian, Metal, Dielectric, Emissive |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-005-10 | max_depth=0 produces black image on GPU (no secondary rays) | Integration test: all pixels are black/near-black |
| AC-GPU-005-11 | Two metal spheres facing each other at max_depth=50 render without hang or crash | Integration test: render completes within timeout |
| AC-GPU-005-12 | GPU RNG produces statistically uniform distribution (scatter directions cover hemisphere) | Statistical test: sample 10000 scatter directions, verify hemisphere coverage |
| AC-GPU-005-13 | Fuzzy metal with perturbed ray below surface (dot < 0) absorbs correctly on GPU | Visual check: fuzzy=1.0 sphere is darker than fuzzy=0.0 sphere |

---

## Phase 6: Linear BVH on GPU

### US-GPU-006: GPU-Accelerated BVH Traversal for Complex Scenes

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-006-01 | CPU BVH is flattened into a contiguous array of LinearBVHNode structs | Unit test: flatten BVH, verify node count and structure |
| AC-GPU-006-02 | LinearBVHNode stores AABB bounds (min, max), offset, and count | Struct definition check: fields present with correct types |
| AC-GPU-006-03 | Interior nodes have count=0 and offset=second_child_index; leaf nodes have count>0 and offset=first_primitive_index | Unit test: inspect node types in flattened array |
| AC-GPU-006-04 | Linear BVH array is uploaded to Metal buffer and accessible in compute shader | Integration test: shader reads BVH data without error |
| AC-GPU-006-05 | GPU BVH traversal produces pixel-identical images to GPU brute-force for the same scene | Pixel comparison: 100-sphere scene, BVH vs brute-force |
| AC-GPU-006-06 | GPU BVH provides at least 5x speedup over GPU brute-force for 500+ shape scenes | Timing comparison: brute-force time / BVH time >= 5 |
| AC-GPU-006-07 | GPU traversal stack is fixed-size (64 entries) and does not overflow for practical scenes | Integration test: 10000-shape scene renders without stack overflow |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-006-08 | Empty scene (0 shapes): BVH is empty, GPU renders sky only | Integration test: empty scene with BVH path |
| AC-GPU-006-09 | Single shape: BVH has 1 leaf node, renders correctly | Integration test: 1-sphere scene with BVH |
| AC-GPU-006-10 | All shapes at same position: BVH degrades gracefully (no infinite loop) | Integration test: 100 overlapping spheres |
| AC-GPU-006-11 | LinearBVHNode struct is 16-byte aligned for Metal buffer compatibility | Static assert: sizeof(LinearBVHNode) alignment check |

---

## Phase 7: SPP Accumulation

### US-GPU-007: Multi-Sample Anti-Aliasing on GPU

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-007-01 | GPU compute shader loops `samples_per_pixel` times per pixel | Integration test: SPP=1 traces 1 ray; SPP=48 traces 48 rays per pixel |
| AC-GPU-007-02 | Each sample jitters ray within pixel area: offset in [0, 1) x [0, 1) using GPU RNG | Statistical test: sample offsets are uniformly distributed over pixel area |
| AC-GPU-007-03 | Final pixel color is arithmetic mean of all sample colors | Integration test: at high SPP, pixel colors converge (variance decreases) |
| AC-GPU-007-04 | Gamma correction `sqrt(clamp(color, 0, 1))` applied after averaging | Pixel check: midtone gray (albedo 0.5) maps to ~181 in PPM output |
| AC-GPU-007-05 | NaN values replaced with 0 before accumulation | Integration test: degenerate scene does not produce NaN in output |
| AC-GPU-007-06 | GPU at 48 SPP produces image quality comparable to CPU at 48 SPP | Visual comparison: anti-aliasing quality and noise levels match |
| AC-GPU-007-07 | Render time scales approximately linearly with SPP (T(8*N) ~ 8*T(N) within 20%) | Timing measurement at 1, 8, 48 SPP |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-007-08 | SPP=1 produces a valid (aliased) image identical to non-jittered single ray | Integration test: SPP=1 renders correctly |
| AC-GPU-007-09 | All RGB values in output PPM are in [0, 255] regardless of scene content | Parse output file: validate all values |
| AC-GPU-007-10 | GPU at very high SPP (1000) does not crash or produce artifacts | Stress test: render at 1000 SPP on small image |

---

## Phase 8: Animation Integration

### US-GPU-008: Physics Animation Rendering via GPU Backend

#### Must Have

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-008-01 | `--physics-animate --backend=metal` triggers GPU rendering for each frame | Integration test: run animation, verify Metal device is used |
| AC-GPU-008-02 | Frame files follow naming convention: frame_0000.ppm, frame_0001.ppm, ... | File check: all frame files exist with correct names |
| AC-GPU-008-03 | Each frame shows physics-updated positions (sphere moves between frames) | Visual check: frame_0000 and frame_0029 show sphere at different y positions |
| AC-GPU-008-04 | SceneFlattener re-runs each frame (GPU buffers updated with new positions) | Integration test: buffer contents differ between frame 0 and frame 10 |
| AC-GPU-008-05 | Progress reporting to stderr works during GPU animation | Output check: stderr contains "Frame N/M" messages |
| AC-GPU-008-06 | Per-frame GPU render time is less than per-frame CPU render time | Timing comparison: GPU frame time < CPU frame time |

#### Boundary/Edge

| ID | Criterion | Verification |
|---|---|---|
| AC-GPU-008-07 | Animation with 1 frame works correctly | Integration test: 1-frame animation produces frame_0000.ppm |
| AC-GPU-008-08 | GPU device and command queue are reused across frames (not re-initialized) | Debug logging: Metal init appears once, not per-frame |
| AC-GPU-008-09 | Animation with 0 objects (camera only) renders sky-only frames via GPU | Integration test: empty animation produces gradient-only frames |
| AC-GPU-008-10 | All 243 existing tests pass after animation GPU integration | Full test suite execution |
