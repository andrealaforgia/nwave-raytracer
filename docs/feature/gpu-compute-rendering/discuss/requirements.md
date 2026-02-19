# GPU Compute Rendering -- Requirements Document

**Document ID**: REQ-GPU-COMPUTE-001
**Date**: 2026-02-17
**Status**: Draft -- Pending DoR Validation

---

## 1. Problem Statement

Artists and technical users rendering complex scenes with nwave experience multi-minute render times even on modern multi-core CPUs. Sofia Reyes's 500-sphere scene at 3840x2160 with 48 SPP takes over 40 minutes on her M2 Max MacBook Pro, using all 12 CPU cores at 1200% utilization. The CPU-parallel renderer (std::thread scanline splitting) has reached its practical ceiling: doubling core count yields diminishing returns, and the fundamental bottleneck is per-ray throughput. GPU compute hardware (Apple M-series, dedicated AMD/Intel GPUs in Macs) has thousands of execution units optimized for exactly this kind of embarrassingly parallel workload, but the current renderer cannot use them.

The renderer's CPU architecture -- virtual dispatch for Shape::hit() and Material::scatter(), recursive trace_ray(), pointer-based scene graph -- is structurally incompatible with GPU execution. Moving to GPU compute requires a data-oriented transformation of the rendering kernel while preserving the existing CPU renderer for portability (Linux, CI, headless servers).

---

## 2. Stakeholders

| Stakeholder | Role | Key Concern |
|---|---|---|
| **Sofia Reyes** | Technical artist rendering product visualizations at 4K | Render time reduction from 40+ minutes to under 1 minute for production scenes |
| **David Okonkwo** | Hobbyist 3D artist iterating on scene designs | Interactive iteration: see GPU-rendered previews in seconds instead of minutes |
| **Elena Marchetti** | CG student learning about GPU compute | Understanding the CPU-to-GPU translation of ray tracing (virtual dispatch to tagged unions, recursion to iteration) |
| **Prof. Kenji Tanaka** | CS instructor demonstrating GPU parallelism | Teaching GPU architecture concepts using the renderer as a concrete example; needs CPU fallback for lab machines without Metal |

---

## 3. Business Goals

1. Achieve 50-200x speedup over CPU rendering on Apple Silicon GPUs for production scenes (500+ objects, 3840x2160, 48 SPP).
2. Preserve the existing CPU renderer as a first-class fallback -- no regressions to the 243 existing tests.
3. Enable backend selection via CLI flag (`--backend=metal` or `--gpu`) without changing scene files, YAML loading, physics, or animation pipelines.
4. Maintain Clean Architecture ring separation -- Metal API code lives exclusively in Ring 4 (Infrastructure).
5. Deliver incrementally via Elephant Carpaccio slicing: each story produces a working, demonstrable render.

---

## 4. Capability Areas

### 4.0 Walking Skeleton: Metal Compute Pipeline End-to-End

**Purpose**: Validate the entire CPU-to-GPU-to-PPM pipeline with the smallest possible kernel -- a compute shader that writes a flat color gradient to a texture, which the CPU reads back and saves as PPM.

**Scope**:
- Metal device and command queue initialization
- A trivial compute shader (.metal) that writes a gradient based on pixel coordinates (no ray tracing)
- GPU texture allocation matching image dimensions
- Compute dispatch with proper threadgroup sizing
- GPU-to-CPU readback of pixel data
- Integration with existing PPM writer
- CLI flag `--backend=metal` to select GPU path
- Graceful fallback to CPU if Metal is unavailable

**Constraints**:
- The walking skeleton does not perform ray tracing -- it validates the Metal compute infrastructure
- Must compile and link Metal shaders via CMake
- Must produce a valid PPM file identical in format to CPU output
- On non-macOS platforms or machines without Metal, the `--backend=metal` flag must produce a clear error message and exit cleanly

### 4.1 Render Backend Abstraction

**Purpose**: Introduce a RenderBackend interface at Ring 3 (Application) that both the existing CPU Renderer and the new Metal GPU renderer implement, enabling polymorphic dispatch without changing callers.

**Scope**:
- Abstract RenderBackend interface: `render(camera, scene, settings) -> vector<Color3>`
- CpuRenderBackend wrapping the existing Renderer logic
- MetalRenderBackend implementing the GPU path
- Backend selection in CLI and AnimationRenderer's WriteCallback
- Settings extension: `RenderSettings.backend` field (cpu, metal, auto)

**Constraints**:
- The existing Renderer public API must remain available for backward compatibility
- AnimationRenderer's WriteCallback must work transparently with either backend
- The "auto" backend defaults to "cpu" (GPU is opt-in)

### 4.2 Flat-Color GPU Rendering (Ray Generation + Miss Shader)

**Purpose**: The first actual ray tracing on the GPU -- generate camera rays in a compute shader and evaluate the sky gradient for rays that hit nothing. No scene intersection.

**Scope**:
- Camera parameters packed into a Metal buffer and uploaded to GPU
- Compute shader generates primary rays using the same math as Camera::generate_ray()
- Miss shader: sky gradient computation matching CPU background_top/background_bottom
- GPU output read back and compared pixel-for-pixel against CPU-rendered empty scene

**Constraints**:
- The GPU-rendered empty scene must be pixel-identical to the CPU-rendered empty scene (same sky gradient formula)
- Camera buffer layout must support all Camera parameters (lookfrom, lookat, vup, vfov, aspect_ratio, image dimensions, pixel deltas)

### 4.3 GPU Scene Data Packing (Flat Scene Representation)

**Purpose**: Transform the pointer-based, polymorphic Scene graph into a flat, GPU-uploadable buffer representation using tagged unions instead of virtual dispatch.

**Scope**:
- GPU-compatible shape representation: tagged union with ShapeType enum (Sphere, Plane, Box, Cylinder, Triangle) and union of parameter structs
- GPU-compatible material representation: tagged union with MaterialType enum (Lambertian, Metal, Dielectric, Emissive) and union of parameter structs
- GPU-compatible light representation: tagged union with LightType enum (Point, Directional)
- Scene flattener: traverses the CPU Scene and produces packed arrays of GPU shapes, materials, and lights
- Metal buffer upload of flattened scene data

**Constraints**:
- Every Shape subclass currently in the codebase (Sphere, Plane, Box, Cylinder, Triangle, TriangleMesh, TransformedShape) must have a GPU representation or be documented as unsupported in this story
- Material raw pointers in HitRecord translate to material indices in the GPU representation
- TransformedShape requires a Matrix4x4 per shape instance in the GPU buffer
- TriangleMesh requires a separate vertex/index buffer strategy (may be deferred to a later story)

### 4.4 GPU Ray-Scene Intersection (Single Bounce, Diffuse Only)

**Purpose**: Perform ray-object intersection on the GPU for basic shapes (spheres, planes, boxes) with Lambertian diffuse shading and direct lighting from a single point light. This is the first story that produces a recognizable GPU-rendered scene.

**Scope**:
- Compute shader implements hit() for Sphere, Plane, and Box using the same math as CPU
- Brute-force intersection (linear scan over all shapes)
- Lambertian diffuse shading: color = albedo * max(0, dot(N, L)) * light_intensity
- Single point light, single shadow ray per hit point
- GPU random number generation (PCG) for Lambertian scatter direction
- No recursion -- single bounce only

**Constraints**:
- The GPU-rendered image of a simple scene (3-5 spheres on a plane, one light) must be visually comparable to the CPU-rendered image at 1 SPP (allowing for RNG differences in scatter directions)
- Shadow ray epsilon offset must match CPU implementation (0.001 * normal)

### 4.5 Iterative Ray Tracing (Multi-Bounce on GPU)

**Purpose**: Replace the CPU's recursive trace_ray() with an iterative loop on the GPU that handles reflection, refraction, and multiple light bounces up to max_depth.

**Scope**:
- Iterative ray tracing loop replacing recursion: accumulate color and attenuation per bounce
- Metal material: mirror reflection on GPU (reflect formula)
- Dielectric material: Snell's law refraction, Schlick's approximation, total internal reflection on GPU
- Emissive material: emit light, no scatter
- Glossy metal: fuzziness perturbation with GPU RNG
- Multi-light support: iterate over light array per hit point
- max_depth termination

**Constraints**:
- GPU iterative tracing must produce visually equivalent results to CPU recursive tracing for the same scene at the same SPP (statistical equivalence, not pixel-identical, due to RNG differences)
- All four material types (Lambertian, Metal, Dielectric, Emissive) must be implemented

### 4.6 Linear BVH for GPU Traversal

**Purpose**: Build the BVH on the CPU and flatten it into a linear array that the GPU can traverse without pointers, enabling sub-linear intersection performance on complex scenes.

**Scope**:
- CPU-side BVH construction (reuse existing algorithm)
- BVH flattening: convert pointer-based tree into a contiguous array of LinearBVHNode structs (AABB bounds, primitive offset/count for leaves, second child offset for interior nodes)
- Metal buffer upload of linear BVH
- GPU traversal kernel using an explicit stack (fixed-size array) instead of recursion
- Performance measurement: GPU render time with and without BVH on a 500-shape scene

**Constraints**:
- Linear BVH traversal on GPU must produce identical intersection results to brute-force GPU traversal (visual equivalence)
- The linear BVH node struct must be packed to Metal's buffer alignment requirements
- Stack depth for GPU traversal must be bounded (64 levels supports 2^64 nodes -- effectively unlimited)

### 4.7 GPU SPP Accumulation and Anti-Aliasing

**Purpose**: Support multiple samples per pixel on the GPU with proper accumulation, random ray jittering, and gamma correction, producing final image quality matching the CPU renderer.

**Scope**:
- Per-pixel SPP loop in the compute shader (or multi-pass accumulation)
- Random sub-pixel jittering for anti-aliasing (matching CPU's random offset strategy)
- Color accumulation and averaging across samples
- Gamma correction (sqrt) applied after averaging
- GPU-side NaN and infinity clamping

**Constraints**:
- GPU at N SPP must produce image quality comparable to CPU at N SPP (convergence behavior matches)
- Gamma correction formula must match CPU exactly: output = sqrt(clamp(linear_color, 0, 1))

### 4.8 Animation Pipeline GPU Integration

**Purpose**: Enable AnimationRenderer to use the GPU backend for physics-driven animation rendering, producing each frame via Metal compute.

**Scope**:
- AnimationRenderer's WriteCallback uses the selected backend (CPU or GPU)
- Per-frame scene re-upload to GPU (physics changes object positions each frame)
- Scene flattener re-runs per frame with updated TransformedShape positions
- Progress reporting works with GPU backend

**Constraints**:
- The `--physics-animate --backend=metal` combination must work end-to-end
- Each frame's GPU buffer upload + render + readback must complete before the next frame begins
- Frame output must match existing PPM naming convention (frame_0000.ppm, frame_0001.ppm, ...)

---

## 5. Non-Functional Requirements

### 5.1 Performance

- GPU rendering of Sofia's 500-sphere scene at 3840x2160, 48 SPP must complete in under 60 seconds on Apple M2 Max (compared to 40+ minutes on CPU).
- GPU walking skeleton (flat gradient, 3840x2160) must complete in under 1 second including Metal initialization, dispatch, readback, and PPM write.
- GPU scene data upload for a 500-object scene must complete in under 100ms.
- GPU BVH traversal must provide measurable speedup (at least 5x) over GPU brute-force for scenes with 100+ objects.

### 5.2 Correctness

- GPU and CPU renderers must produce visually equivalent images for the same scene, settings, and SPP count. "Visually equivalent" means: per-pixel RGB values differ by no more than +/-5 per channel when averaged over 100+ SPP (statistical convergence).
- All 243 existing tests must continue passing without modification.
- GPU renderer must handle edge cases: empty scene (background only), single object, camera inside object.

### 5.3 Maintainability

- Metal API code is confined to Ring 4 (Infrastructure). Ring 1 (Core), Ring 2 (Domain), and Ring 3 (Application) contain no Metal imports or Objective-C++.
- The RenderBackend interface lives in Ring 3. New GPU backends (future Vulkan, future CUDA) can be added as Ring 4 implementations without changing Ring 3.
- The scene flattener (CPU Scene to GPU buffers) is testable independently of Metal -- it produces plain C structs that can be verified in unit tests.

### 5.4 Portability

- The application must compile on Linux and Windows without Metal support. The GPU backend is compiled only on macOS (guarded by CMake platform checks and `#ifdef __APPLE__`).
- The CPU renderer remains the default backend on all platforms.
- The `--backend=metal` flag on non-macOS produces: "Error: Metal backend is only available on macOS" and exits with code 1.

### 5.5 Build System

- CMake must compile .metal shader files into a Metal library (.metallib) during the build.
- Metal framework linking must be conditional (macOS only).
- The build must succeed on Linux/Windows by excluding Metal sources entirely.

---

## 6. Constraints and Dependencies

| Constraint | Description |
|---|---|
| Platform | Metal API -- macOS only (Apple Silicon and Intel Macs with Metal support) |
| Language | Objective-C++ (.mm) for Metal API calls; Metal Shading Language (.metal) for compute shaders; C++17 for all other code |
| Architecture | Clean Architecture ring model -- Metal adapter in Ring 4 only |
| Backward compatibility | All 243 existing tests pass; CPU renderer unchanged; existing CLI commands work identically |
| Framework | Metal.framework and MetalKit.framework (system frameworks, no external dependency) |
| Build system | CMake with custom commands for .metal compilation |
| Existing interfaces | Renderer::render(), AnimationRenderer::WriteCallback, RenderSettings, CliDispatcher::RenderCommand must remain stable |

---

## 7. Out of Scope (Current Feature)

- Vulkan compute backend (future cross-platform GPU support)
- CUDA backend
- Ray tracing hardware acceleration (Metal RT API for Apple Silicon with ray tracing cores)
- GPU-side BVH construction (BVH is built on CPU and uploaded)
- GPU-side scene loading or YAML parsing
- Texture mapping on GPU
- TriangleMesh on GPU (deferred -- requires vertex/index buffer strategy beyond tagged unions)
- Real-time interactive preview (render-to-window)
- Multi-GPU rendering
- Shared memory / tile-based optimizations (future performance story)
- Async compute (overlapping CPU physics with GPU render) -- future optimization

---

## 8. Incremental Delivery Path

The stories follow Elephant Carpaccio slicing, each producing a working, demonstrable render:

```
Phase 0: US-GPU-000 Walking Skeleton (flat gradient via Metal compute)
    |
Phase 1: US-GPU-001 Render Backend Abstraction (CPU/GPU polymorphism)
    |
Phase 2: US-GPU-002 Ray Generation + Sky Gradient on GPU
    |
Phase 3: US-GPU-003 Flat Scene Data Packing
    |
Phase 4: US-GPU-004 Single-Bounce Diffuse GPU Rendering
    |
Phase 5: US-GPU-005 Iterative Multi-Bounce Ray Tracing
    |
Phase 6: US-GPU-006 Linear BVH for GPU
    |
Phase 7: US-GPU-007 SPP Accumulation + Anti-Aliasing
    |
Phase 8: US-GPU-008 Animation Pipeline Integration
```

Each phase produces a demonstrable output. Phase 0 produces a gradient image. Phase 2 produces a sky. Phase 4 produces a recognizable scene. Phase 5 produces reflections and glass. Phase 7 produces production-quality images.
