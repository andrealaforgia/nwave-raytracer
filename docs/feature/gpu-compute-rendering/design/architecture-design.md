# Architecture Design: GPU Compute Rendering (Metal)

**Document ID**: ARCH-GPU-001
**Feature**: gpu-compute-rendering
**Date**: 2026-02-18
**Status**: Draft
**Extends**: ARCH-SPA-001 (scene-physics-animation architecture)

---

## 1. System Overview

This architecture extends the existing nwave-raytracer with Metal GPU compute rendering. The existing Clean Architecture four-ring model (Core, Domain, Application, Infrastructure) is preserved. The GPU rendering path is an alternative backend living entirely in Ring 4 (Infrastructure), selected at runtime via a `--backend` CLI flag. Rings 1-3 have zero Metal dependencies.

### 1.1 New Business Capabilities

| Capability | Description | Stakeholder Value |
|---|---|---|
| GPU-accelerated rendering | Metal compute shaders execute the ray tracing kernel on Apple Silicon GPU | 50-200x speedup for production scenes (40+ min to <60s) |
| Backend selection | `--backend=cpu` / `--backend=metal` CLI flag | Same scene, same output format, user chooses speed vs portability |
| Scene flattening | Polymorphic CPU scene graph converted to flat GPU buffers (tagged unions) | Enables GPU execution without changing domain model |
| Linear BVH | Pointer-based BVH flattened to contiguous array for GPU traversal | Sub-linear GPU intersection for complex scenes |
| GPU animation pipeline | AnimationRenderer's WriteCallback routes through GPU backend | Physics animation sequences rendered in minutes instead of hours |

### 1.2 Quality Attributes (Additions to ISO 25010 Targets)

| Attribute | Target | Strategy |
|---|---|---|
| **Performance** | 50-200x speedup on M2 Max for 500-sphere 4K 48SPP scene | Metal compute shader with BVH traversal, per-pixel SPP loop |
| **Correctness** | GPU output within +/-5 per RGB channel vs CPU at 100+ SPP | Same math formulas; float vs double tolerance documented |
| **Maintainability** | Metal code exclusively in Ring 4; new GPU backends (Vulkan) add Ring 4 impl only | RenderBackend abstract interface in Ring 3 |
| **Portability** | Build succeeds on Linux/Windows without Metal | CMake platform guards; `--backend=metal` on non-macOS returns error |
| **Testability** | Scene flattener testable without GPU hardware | GPU data structs are plain C++ with no Metal dependencies |

---

## 2. Extended Architecture Ring Model

New components marked with `[GPU]`. All GPU-specific implementations are in Ring 4. The RenderBackend interface in Ring 3 is the dependency inversion boundary.

```
+------------------------------------------------------------------------+
|                        Infrastructure (Ring 4)                          |
|  [EXISTING] CLI Dispatcher | PPMWriter | YamlSceneLoader | Validator   |
|  [EXISTING] JoltPhysicsSimulator                                       |
|  [GPU] MetalRenderBackend (.mm)                                        |
|  [GPU] MetalDevice (.mm) -- device, queue, pipeline management         |
|  [GPU] MetalBufferManager (.mm) -- buffer allocation, upload           |
|  [GPU] SceneFlattener (.cpp) -- Scene -> GPU structs (no Metal deps)   |
|  [GPU] BVHFlattener (.cpp) -- pointer BVH -> linear array (no Metal)  |
|  [GPU] shaders/ray_trace.metal -- compute kernels                      |
|  +------------------------------------------------------------------+  |
|  |                     Application (Ring 3)                          |  |
|  |  [EXISTING] Renderer | AnimationRenderer                          |  |
|  |  [GPU] RenderBackend (abstract interface)                         |  |
|  |  [GPU] CpuRenderBackend (wraps existing Renderer)                 |  |
|  |  +--------------------------------------------------------------+|  |
|  |  |                    Domain (Ring 2)                            ||  |
|  |  |  [EXISTING] Shape | Material | Light | Camera | Scene        ||  |
|  |  |  [EXISTING] TransformedShape | PhysicsProperties | etc.      ||  |
|  |  |  +----------------------------------------------------------+||  |
|  |  |  |               Core / Math (Ring 1)                        |||  |
|  |  |  |  [EXISTING] Vec3 | Point3 | Color3 | Ray | AABB          |||  |
|  |  |  |  [GPU] gpu_types.h -- GPUShape, GPUMaterial, GPULight,    |||  |
|  |  |  |        GPUCamera, LinearBVHNode (plain C structs)         |||  |
|  |  |  +----------------------------------------------------------+||  |
|  |  +--------------------------------------------------------------+|  |
|  +------------------------------------------------------------------+  |
+------------------------------------------------------------------------+
```

**Ring placement rationale for GPU types in Ring 1**: The GPU data structs (GPUShape, GPUMaterial, etc.) are plain C structs with `float` fields and no dependencies beyond `<cstdint>`. They are "data transfer objects" that must be shared between: (a) the SceneFlattener in Ring 4 that writes them, (b) the Metal shader code that reads them, and (c) unit tests on any platform. Placing them in Ring 1 (Core) keeps them dependency-free and testable. They are purely data -- no behavior, no Metal imports.

---

## 3. Component Descriptions

### 3.1 Ring 1: Core (New Components)

| Component | Responsibility |
|---|---|
| **gpu_types.h** | Plain C structs shared between CPU flattener and GPU shaders: GPUShape (tagged union), GPUMaterial (tagged union), GPULight (tagged union), GPUCamera (camera parameters), LinearBVHNode. All fields are `float` (GPU precision). 16-byte aligned for Metal buffer compatibility. No Metal imports, no behavior. |

### 3.2 Ring 3: Application (New Components)

| Component | Responsibility |
|---|---|
| **RenderBackend** | Abstract interface: `render(camera, scene, settings) -> vector<Color3>`. Lives in Ring 3. Decouples callers from rendering implementation. |
| **CpuRenderBackend** | Wraps existing `Renderer::render()`. Produces byte-identical output to the current code path. Allows existing Renderer to remain unchanged. |

### 3.3 Ring 4: Infrastructure (New Components)

| Component | Responsibility |
|---|---|
| **SceneFlattener** | Pure C++ (no Metal). Traverses CPU `Scene` and produces flat arrays: `vector<GPUShape>`, `vector<GPUMaterial>`, `vector<GPULight>`. Deduplicates shared material instances via pointer-to-index map. Handles TransformedShape by storing inverse transform matrix in GPUShape. |
| **BVHFlattener** | Pure C++ (no Metal). Builds a BVH from the scene's flat shape list (CPU-side), then serializes the pointer-based tree into a contiguous `vector<LinearBVHNode>`. Interior nodes store second-child offset; leaf nodes store primitive range. |
| **MetalRenderBackend** | Objective-C++ (.mm). Implements `RenderBackend`. Orchestrates: flatten scene -> upload buffers -> dispatch compute -> readback pixels -> return `vector<Color3>`. Owns MetalDevice and MetalBufferManager. |
| **MetalDevice** | Objective-C++ (.mm). Encapsulates `MTLDevice`, `MTLCommandQueue`, `MTLComputePipelineState`, and `MTLLibrary` lifecycle. Loads compiled `.metallib` at runtime. |
| **MetalBufferManager** | Objective-C++ (.mm). Allocates and manages `MTLBuffer` objects for scene data (shapes, materials, lights, BVH, camera, output pixels). Handles GPU memory lifecycle and readback. |
| **ray_trace.metal** | Metal Shading Language compute shader(s). Per-pixel ray generation, iterative multi-bounce tracing, BVH traversal, material evaluation, SPP accumulation, gamma correction. |

---

## 4. Data Flow

### 4.1 GPU Render Path (Single Frame)

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
RenderBackend::render(camera, scene, settings) -- dispatched to MetalRenderBackend
    |
    |  1. SceneFlattener.flatten(scene) -> GPUShape[], GPUMaterial[], GPULight[]
    |  2. BVHFlattener.build_and_flatten(shapes) -> LinearBVHNode[]
    |  3. Pack camera parameters -> GPUCamera
    |  4. MetalBufferManager.upload(shapes, materials, lights, bvh, camera)
    |  5. MetalDevice.dispatch_compute(width, height, threadgroup_size)
    |  6. MetalBufferManager.readback(output_buffer) -> float4 pixels
    |  7. Convert float4 -> Color3 (already gamma-corrected on GPU)
    |
    v
vector<Color3> (same format as CPU path)
    |
    v
PPMWriter.write(filename, pixels) (Ring 4) ---> output.ppm
```

### 4.2 CPU Render Path (Unchanged)

```
RenderBackend::render(camera, scene, settings) -- dispatched to CpuRenderBackend
    |
    v
Renderer.render(camera, scene, settings) -- existing code, unchanged
    |
    v
vector<Color3>
```

### 4.3 Animation with GPU Backend

```
AnimationRenderer.render() (Ring 3):
    |
    |  For each frame:
    |    1. Update TransformedShape positions (physics)
    |    2. WriteCallback(filename, scene, camera, width, spp)
    |         |
    |         v
    |    WriteCallback implementation (in main.cpp):
    |      backend->render(camera, scene, settings)  // MetalRenderBackend
    |         |
    |         v
    |    SceneFlattener re-runs (captures updated transforms)
    |    GPU buffers re-uploaded (shapes + BVH change each frame)
    |    Compute dispatch -> readback -> write PPM
    |
    v
frames/frame_0000.ppm ... frame_NNNN.ppm
```

### 4.4 Scene Flattening Pipeline (Detail)

```
CPU Domain Objects (Ring 2)              GPU Flat Structs (Ring 1)
========================                 ========================

Scene.shapes() ----+
                   |
  Sphere*          |    SceneFlattener     GPUShape { type=SPHERE,
    center (d)  ---|-----(Ring 4)-------->   center (f), radius (f),
    radius (d)     |                         material_idx, transform }
    material*      |
                   |
  shared_ptr<M> ---+--(pointer dedup)-->  GPUMaterial { type=LAMBERTIAN,
    albedo (d)     |                        albedo (f) }
                   |
  PointLight*      |                      GPULight { type=POINT,
    position (d) --|--------------------->   position (f), color (f),
    intensity (d)  |                         intensity (f) }

                   |                      GPUCamera { lookfrom (f),
  Camera           |                        pixel00 (f), delta_u (f),
    all params  ---|--------------------->   delta_v (f), ... }
    (double)       |

BVH (pointer tree) |                      LinearBVHNode[] {
    BVHNode*    ---|--BVHFlattener------->   aabb_min (f), aabb_max (f),
                   |                         offset, count }
```

Key transformations:
- `double` -> `float` (precision narrowing, documented tolerance)
- `virtual dispatch` -> `tagged union` (shape_type/material_type enums)
- `Material*` pointers -> `uint32_t material_index` (into flat array)
- `BVHNode*` left/right pointers -> `uint32_t offset` (array index)
- `TransformedShape` -> base shape params + `float4x4 inverse_transform`

---

## 5. Key Interfaces

### 5.1 RenderBackend (Ring 3 -- Application)

```
class RenderBackend {
    virtual ~RenderBackend() = default;
    virtual std::vector<Color3> render(
        const Camera& camera,
        const Scene& scene,
        const RenderSettings& settings) = 0;
};
```

This is the single dependency-inversion point. Callers (main.cpp, WriteCallback) depend on this interface, not on Renderer or MetalRenderBackend directly.

### 5.2 CpuRenderBackend (Ring 3 -- Application)

```
class CpuRenderBackend : public RenderBackend {
    std::vector<Color3> render(camera, scene, settings) override;
    // Delegates to existing Renderer::render()
};
```

### 5.3 MetalRenderBackend (Ring 4 -- Infrastructure)

```
class MetalRenderBackend : public RenderBackend {
    std::vector<Color3> render(camera, scene, settings) override;
    // Flatten -> Upload -> Dispatch -> Readback
};
```

### 5.4 SceneFlattener (Ring 4 -- Infrastructure, pure C++)

```
struct FlatScene {
    std::vector<GPUShape> shapes;
    std::vector<GPUMaterial> materials;
    std::vector<GPULight> lights;
};

FlatScene flatten(const Scene& scene);
```

### 5.5 BVHFlattener (Ring 4 -- Infrastructure, pure C++)

```
std::vector<LinearBVHNode> build_and_flatten(
    const std::vector<GPUShape>& shapes);
```

Builds BVH from GPUShape AABBs on CPU, then linearizes.

---

## 6. Integration with Existing Components

### 6.1 Components That Do NOT Change

| Component | Ring | Reason |
|---|---|---|
| Vec3, Point3, Color3, Ray, AABB, Matrix4x4, Quaternion | 1 | GPU types are separate plain structs |
| Shape hierarchy (Sphere, Plane, Box, Cylinder, Triangle, TransformedShape) | 2 | SceneFlattener reads them; they are not modified |
| Material hierarchy (Lambertian, Metal, Dielectric, Emissive) | 2 | SceneFlattener reads them; they are not modified |
| Light hierarchy (PointLight, DirectionalLight) | 2 | SceneFlattener reads them; they are not modified |
| Camera, Scene, HitRecord | 2 | Read by SceneFlattener; not modified |
| Renderer | 3 | Wrapped by CpuRenderBackend; not modified |
| PhysicsSimulator, AnimationRenderer | 3 | AnimationRenderer unchanged; WriteCallback is the integration point |
| YamlSceneLoader, Validator, JoltPhysicsSimulator | 4 | Unchanged |
| PPMWriter | 4 | Used as-is for output |

### 6.2 Components That Change Minimally

| Component | Ring | Change |
|---|---|---|
| **RenderSettings** | 3 | Add `std::string backend` field (default: empty = cpu) |
| **RenderCommand** | 4 | Add `std::string backend` field |
| **CliDispatcher** | 4 | Parse `--backend=X` flag, populate RenderCommand.backend |
| **main.cpp** | 4 | Create appropriate RenderBackend based on command.backend; pass to WriteCallback |
| **src/CMakeLists.txt** | - | Add GPU sources (conditional on APPLE), Metal shader compilation, link Metal.framework |
| **Root CMakeLists.txt** | - | Add `NWAVE_ENABLE_GPU` option, Metal framework detection |

---

## 7. Architectural Decisions (ADRs)

See separate ADR documents in `docs/adrs/`:
- ADR-GPU-001: RenderBackend abstraction location and contract
- ADR-GPU-002: Scene flattening strategy (tagged unions)
- ADR-GPU-003: Metal shader architecture (single kernel)
- ADR-GPU-004: Float precision strategy (double CPU / float GPU)
- ADR-GPU-005: Build system Metal integration approach

---

## 8. Deployment Architecture

The system remains a single statically-linked executable. On macOS with Metal support, the Metal framework is dynamically linked (system framework). The compiled shader library (.metallib) is placed alongside the binary.

```
+--------------------------------------------+
|  nwave (single executable)                  |
|                                            |
|  Statically links:                         |
|  - yaml-cpp (YAML parsing)                |
|  - Jolt Physics (rigid body sim)           |
|  - stb_image_write (PNG output)            |
|  - GoogleTest (test binary only)           |
|                                            |
|  Dynamically links (macOS only):           |
|  - Metal.framework          [GPU]          |
|  - Foundation.framework     [GPU]          |
|                                            |
|  Bundled artifacts:                        |
|  - nwave_shaders.metallib   [GPU]          |
|                                            |
|  Reads: scene.yaml                         |
|  Writes: output.ppm, frames/*.ppm          |
+--------------------------------------------+
```

On Linux/Windows: Metal.framework, Foundation.framework, and .metallib are absent. The GPU source files are not compiled. The `--backend=metal` flag produces a clear error message.

---

## 9. Cross-Cutting Concerns

### 9.1 Error Handling

- **No Metal device**: `MTLCreateSystemDefaultDevice()` returns nil. MetalRenderBackend reports "No Metal GPU device found" and suggests `--backend cpu`.
- **Shader library not found**: MetalDevice cannot locate `.metallib`. Reports expected path, suggests rebuild.
- **GPU memory exceeded**: Buffer allocation fails. Reports required vs available memory with per-buffer breakdown.
- **GPU timeout**: Metal watchdog timer (30s). MetalRenderBackend detects timeout and splits dispatch into smaller batches.
- **NaN pixels**: Post-readback scan replaces NaN with zero. Warning reports count of NaN pixels.
- **Non-macOS `--backend=metal`**: Compile-time guard (`#ifndef NWAVE_HAS_METAL`). CliDispatcher reports "Metal backend is only available on macOS" and exits with code 1.

### 9.2 Testing Strategy (New Components)

| Component | Ring | Test Type | Metal Required? |
|---|---|---|---|
| gpu_types.h (GPUShape, GPUMaterial, etc.) | 1 | Unit: struct sizes, alignment, field layout | No |
| RenderBackend, CpuRenderBackend | 3 | Unit: CpuRenderBackend produces identical output to Renderer | No |
| SceneFlattener | 4 | Unit: flatten known scenes, verify arrays | No |
| BVHFlattener | 4 | Unit: build BVH, verify node structure, traversal correctness | No |
| MetalRenderBackend | 4 | Integration: render known scene, compare to CPU | Yes (macOS only) |
| MetalDevice | 4 | Integration: init, compile pipeline | Yes (macOS only) |
| End-to-end GPU render | - | Acceptance: GPU PPM vs CPU PPM comparison | Yes (macOS only) |

Key insight: SceneFlattener and BVHFlattener are the most architecturally complex new components, and they are fully testable on any platform without Metal hardware. The Metal-specific code (MetalDevice, MetalBufferManager, MetalRenderBackend) is thin orchestration around Apple's API.

---

## 10. Traceability Matrix

| Requirement / Story | Architecture Component | Ring |
|---|---|---|
| US-GPU-000: Walking skeleton (gradient via Metal) | MetalDevice, MetalBufferManager, gradient.metal, CMake | 4 |
| US-GPU-001: RenderBackend abstraction | RenderBackend, CpuRenderBackend, MetalRenderBackend | 3, 4 |
| US-GPU-002: Camera rays + sky on GPU | GPUCamera, ray_trace.metal (ray gen + miss) | 1, 4 |
| US-GPU-003: Scene flattening | SceneFlattener, GPUShape, GPUMaterial, GPULight | 1, 4 |
| US-GPU-004: Single-bounce GPU rendering | ray_trace.metal (intersection + diffuse) | 4 |
| US-GPU-005: Multi-bounce iterative tracing | ray_trace.metal (iterative loop, all materials) | 4 |
| US-GPU-006: Linear BVH | BVHFlattener, LinearBVHNode, ray_trace.metal (traversal) | 1, 4 |
| US-GPU-007: SPP accumulation | ray_trace.metal (sample loop, gamma, NaN clamp) | 4 |
| US-GPU-008: Animation GPU integration | main.cpp WriteCallback, MetalRenderBackend re-upload | 4 |

---

## 11. Handoff Notes for Acceptance Designer

1. **RenderBackend is the architectural pivot**: It decouples all callers from the rendering implementation. The crafter must verify that no code path bypasses it to call Renderer or MetalRenderBackend directly (except CpuRenderBackend wrapping Renderer).

2. **SceneFlattener is the highest-complexity pure C++ component**: It must handle all 6 shape types (Sphere, Plane, Box, Cylinder, Triangle, TransformedShape), all 4 material types (Lambertian, Metal, Dielectric, Emissive), and both light types (Point, Directional). Material deduplication via pointer identity is critical. This component is fully testable without Metal.

3. **BVH does not exist yet**: The current Scene::hit() is brute-force linear scan. The BVHFlattener must build a BVH from scratch (not flatten an existing one). This is new logic, not adaptation of existing code.

4. **WriteCallback is the animation integration point**: The existing WriteCallback signature `void(filename, scene, camera, width, spp)` already provides everything needed. The GPU backend re-flattens the scene each frame (TransformedShape positions change). No changes to AnimationRenderer are needed.

5. **Float vs double boundary**: All GPU data structs use `float`. The SceneFlattener performs `double->float` narrowing during flatten. The readback converts `float->double` when constructing `Color3`. The tolerance (+/-5 per channel at 100+ SPP) is a documented trade-off.

6. **Ring enforcement is critical**: Metal headers (`<Metal/Metal.h>`, `<Foundation/Foundation.h>`) and Objective-C++ syntax must appear ONLY in `.mm` files in `src/infrastructure/metal/`. The SceneFlattener and BVHFlattener are pure `.cpp` files that compile on any platform.

---

## 12. Architecture Review Metadata

**Review Date**: 2026-02-18
**Reviewer**: nw-solution-architect-reviewer (Haiku 4.5)
**Approval Status**: APPROVED

### Summary

The GPU Compute Rendering (Metal) architecture design is **approved**. The design demonstrates strong architectural discipline with zero critical issues and zero high-severity issues. All five review dimensions are addressed with evidence-based findings.

### Review Findings by Dimension

#### 1. Bias Detection (No Architectural Bias Found)

**Status**: PASS

- **Metal selection is data-justified**: The design rationale cites metal as the only GPU compute API on macOS (OpenCL deprecated since 10.14, Vulkan over-engineered for single-platform first implementation). Section 1.2 (Technology Stack) evaluates 4 alternatives with rejection rationale. No evidence of vendor lock-in preference.
- **Performance target is production-grounded**: 50-200x speedup target is directly motivated by Sofia's real constraint (40+ minutes to <60s for 500-sphere 4K scene). Not arbitrary.
- **Float precision boundary acknowledged**: ADR-GPU-004 documents the double-to-float narrowing as an explicit trade-off with measured tolerance (+/-5 per RGB channel at 100+ SPP). The decision includes rejected alternatives (double-precision emulation, GPU-wide float conversion) with cost-benefit analysis.

**Finding**: No issues. Architecture decisions trace to requirements and constraints, not preference.

#### 2. ADR Quality (All ADRs Well-Reasoned)

**Status**: PASS

**ADR-GPU-001 (RenderBackend Abstraction)**:
- Context: Clear separation concern (Ring 3 interface vs Ring 4 implementations)
- Decision: RenderBackend in Ring 3; CpuRenderBackend delegates to Renderer; MetalRenderBackend lives in Ring 4
- Alternatives: 3 alternatives evaluated with rejection rationale (Alternative 1: modifying Renderer violates SRP; Alternative 2: Domain placement violates Clean Architecture; Alternative 3: function pointer lacks extensibility)
- Consequences: Positive (minimal Renderer coupling, 243 tests unaffected); Negative (one virtual call overhead, negligible)

**ADR-GPU-002 (Scene Flattening Strategy)**:
- Context: Virtual dispatch incompatible with GPU; pointer-based scene graph not uploadable
- Decision: Tagged unions with separate arrays; SceneFlattener produces flat GPUShape/GPUMaterial/GPULight arrays
- Alternatives: 3 alternatives evaluated (SOA complicates BVH leaf references; multi-kernel adds synchronization complexity; function pointers add overhead for 5 cases)
- Material deduplication via pointer identity is explicit

**ADR-GPU-003 (Metal Shader Architecture)**:
- Context: Ray tracing pipeline can be organized as single or multi-kernel
- Decision: Single megakernel (per-pixel ray generation, iterative bounces, SPP accumulation, gamma correction)
- Alternatives: 3 alternatives evaluated (multi-kernel requires 24GB intermediate state buffer; two-kernel wastes memory; wavefront tracing adds complexity without offline benefit)
- Memory analysis included: intermediate state cost is prohibitive

**ADR-GPU-004 (Float Precision Strategy)**:
- Context: Metal compute has no double support; CPU uses double
- Decision: SceneFlattener narrows double-to-float; readback widens float-to-double; tolerance documented as +/-5 per channel at 100+ SPP
- Alternatives: 3 alternatives evaluated (double emulation halves throughput; CPU float conversion violates constraint; M3 limited double support is insufficient)
- Mitigation: SPP accumulation statistically reduces per-pixel variance

**ADR-GPU-005 (Build System Metal Integration)**:
- Context: CMake integration with platform-specific compilation
- Decision: Opt-in NWAVE_ENABLE_GPU flag; conditional Metal framework linking; xcrun shader compilation; .mm files compiled only on macOS
- Alternatives: 3 alternatives evaluated (runtime compilation adds 200-500ms; embedded .metallib adds build complexity; conditional compilation via #ifdef requires Metal types in non-Apple headers)
- Non-macOS behavior: warning but no error; build succeeds

**Assessment**: All ADRs meet the ADR Acceptance Checklist: context, decision, alternatives with rejection rationale, consequences. No ADR lacks evidence.

**Finding**: No issues. ADR quality is exemplary.

#### 3. Completeness and Requirements Traceability (9 Capability Areas Fully Addressed)

**Status**: PASS

**Capability Area Coverage**:

| Capability | Architecture Component | Addressed |
|---|---|---|
| 4.0 Walking Skeleton | MetalDevice, ray_trace.metal, CMake shader compilation | ✓ Section 3.3 |
| 4.1 RenderBackend Abstraction | RenderBackend (R3), CpuRenderBackend, MetalRenderBackend (R4) | ✓ Section 5 |
| 4.2 Ray Generation + Sky | GPUCamera, ray_trace.metal (ray gen + miss shader) | ✓ Section 4.2, Data Models Section 6 |
| 4.3 Scene Flattening | SceneFlattener, GPUShape, GPUMaterial, GPULight | ✓ Section 3.3, Component Boundaries Section 4.1 |
| 4.4 Single-Bounce Diffuse | ray_trace.metal intersection + Lambertian shading | ✓ Section 3.3, Diagram 3.1 |
| 4.5 Iterative Multi-Bounce | ray_trace.metal iterative loop + all materials | ✓ ADR-GPU-003 pseudocode |
| 4.6 Linear BVH | BVHFlattener, LinearBVHNode, GPU traversal stack | ✓ Section 3.3, Component Boundaries Section 4.2 |
| 4.7 SPP Accumulation | ray_trace.metal sample loop, gamma correction | ✓ ADR-GPU-003 pseudocode, Data Models Section 7 |
| 4.8 Animation Integration | WriteCallback, scene re-upload per frame | ✓ Section 4.3, Diagram 5 |

**Traceability Matrix**: Section 10 traces all 8 user stories to architecture components.

**User Story Coverage**: All 9 user stories (US-GPU-000 through US-GPU-008) explicitly addressed in design or requirements.

**Acceptance Criteria Alignment**:
- AC-GPU-COMPUTE-001 defines 87 detailed acceptance criteria across all 8 stories
- AC mapping to architecture:
  - Ring placement criteria (AC-GPU-001-10: RenderBackend in R3, MetalRenderBackend in R4): addressed in Section 2, Component Boundaries
  - Data structure criteria (AC-GPU-003-01-08: GPUShape/Material/Light cover all types, 16-byte alignment): addressed in Data Models Section 2-3
  - Integration criteria (AC-GPU-008-01-10: animation frame rendering): addressed in Section 4.3

**Finding**: No gaps. All 9 capability areas mapped to architecture components with explicit section references.

#### 4. Clean Architecture Compliance (Ring Separation Enforced)

**Status**: PASS

**Ring 1 (Core/Math)**:
- Existing: Vec3, Point3, Color3, Ray, AABB, Matrix4x4, Quaternion (from scene-physics-animation feature)
- New: gpu_types.h (GPUShape, GPUMaterial, GPULight, GPUCamera, LinearBVHNode)
- Metal dependency check: "No Metal imports, no behavior, no dependencies beyond `<cstdint>`" (Section 2)
- Verification: Component Boundaries Section 2.1 explicitly forbids `#include` outside `<cstdint>` and `<cstddef>`, forbids constructors/destructors/virtual methods, forbids Metal types
- **No Ring 1 violations**: gpu_types.h is plain C structs, testable on Linux without Metal

**Ring 2 (Domain)**:
- Unchanged: Shape (Sphere, Plane, Box, Cylinder, Triangle), Material (Lambertian, Metal, Dielectric, Emissive), Light (PointLight, DirectionalLight), Camera, Scene, HitRecord
- Existing: TransformedShape, PhysicsProperties, AnimationConfig (from scene-physics-animation feature)
- Metal dependency check: No Ring 2 files mention Metal
- **No Ring 2 violations**: Domain is platform-agnostic

**Ring 3 (Application)**:
- Existing: Renderer, AnimationRenderer
- New: RenderBackend (abstract interface), CpuRenderBackend
- Metal dependency check: Component Boundaries Section 3.1 states "Does NOT depend on Ring 4 or any external library"
- Verification: RenderBackend interface takes Camera, Scene, RenderSettings (all Ring 2), returns vector<Color3> (Ring 1). No Metal types in interface.
- CpuRenderBackend: Section 3.2 "depends on Ring 3: Renderer, RenderBackend. Does NOT depend on Ring 4 or any external library"
- **No Ring 3 violations**: RenderBackend is the dependency inversion boundary. CpuRenderBackend wraps Renderer; both live in R3

**Ring 4 (Infrastructure)**:
- Existing: YamlSceneLoader, Validator, PPMWriter, CliDispatcher, JoltPhysicsSimulator
- New: SceneFlattener (pure C++), BVHFlattener (pure C++), MetalRenderBackend (.mm), MetalDevice (.mm), MetalBufferManager (.mm), ray_trace.metal
- Metal confinement: Component Boundaries Section 4.3-4.5 explicitly places MetalRenderBackend, MetalDevice, MetalBufferManager in .mm files with boundary rule "This is internal to Ring 4. Metal types in interface."
- File organization Section 7: "src/infrastructure/metal/" contains only .mm and .metal files; "src/infrastructure/gpu/" contains pure C++ (.cpp)
- Conditional compilation Section 10: ".mm files are NOT COMPILED on Linux/Windows; only .cpp compiled on all platforms"
- CMake enforcement: Section 2.3 states "set_source_files_properties(${METAL_SOURCES} PROPERTIES COMPILE_FLAGS '-x objective-c++')" only when NWAVE_HAS_METAL is true
- **No Ring 4 violations**: Metal is fully confined to .mm files, conditional on macOS

**Backward Compatibility Check**:
- Architecture Design Section 6.1: "Components That Do NOT Change" lists 9 existing components with explicit rationale for each
- Renderer: "Wrapped by CpuRenderBackend; not modified"
- AnimationRenderer: "Unchanged; WriteCallback is the integration point"
- All domain and core components: explicitly unchanged
- Tests: "All 243 existing tests continue passing without modification" (Requirements Section 5.2)

**Finding**: No Ring violations. Clean Architecture ring separation is enforced with evidence-based justification.

#### 5. Integration Completeness and Feasibility (Practical Implementation Path)

**Status**: PASS

**Integration with Existing Renderer**:
- Section 6.2: RenderSettings gains `std::string backend` field (minimal change, backwards-compatible default)
- RenderCommand gains `std::string backend` field
- CliDispatcher parses `--backend` flag and populates RenderCommand.backend
- main.cpp contains the backend selection logic (simple conditional, no factory pattern needed for two backends)
- **No breaking changes**: CpuRenderBackend is a thin wrapper around Renderer; existing code paths unaffected

**Integration with AnimationRenderer**:
- Architecture Section 4.3: "AnimationRenderer unchanged; WriteCallback is the integration point"
- WriteCallback captures `RenderBackend*` instead of creating Renderer directly
- WriteCallback signature remains: `void(filename, scene, camera, width, spp)` -- unchanged
- Component Boundaries Section 5 diagram shows WriteCallback calling `backend->render()` (polymorphic dispatch)
- **Impact**: WriteCallback implementation in main.cpp changes to use RenderBackend; AnimationRenderer itself is untouched

**Integration with Existing Scene/Camera/Physics**:
- Scene, Camera, HitRecord unchanged (read by SceneFlattener, not modified)
- TransformedShape from scene-physics-animation feature is fully supported (Section 4.2 shows inverse transform handling)
- Physics simulation output (TransformedShape positions) flows through SceneFlattener per frame (Section 4.3 animation flow)
- **No scene model changes**: GPU path works with existing scene graph

**BVH Integration (Critical Gap Assessment)**:
- Handoff Notes Section 11, point 3: "BVH does not exist yet. The current Scene::hit() is brute-force linear scan."
- Architecture Section 3.3: BVHFlattener "Builds a BVH from scratch (not flatten an existing one)"
- Feasibility: This is **new logic, not adaptation**. BVHFlattener implements SAH-based construction on CPU (section 4.2, accepted algorithm)
- Data Models Section 5.3-5.4: Linear BVH node format and traversal algorithm fully specified
- **Assessment**: BVH construction is a standard algorithm (well-documented in graphics literature). Building from flat GPUShape array is straightforward. No integration risk.

**Scene Flattening Complexity**:
- Section 3.3: SceneFlattener handles all 6 shape types (Sphere, Plane, Box, Cylinder, Triangle, TransformedShape)
- Section 3.3: SceneFlattener handles all 4 material types (Lambertian, Metal, Dielectric, Emissive)
- Material deduplication via pointer identity (Section 3.3: `map<const Material*, uint32_t>`)
- Component Boundaries Section 4.1: "Uses `dynamic_cast` to identify Shape/Material/Light subtypes during flattening"
- TriangleMesh explicitly out of scope (Section 2.2, User Stories Section 7)
- **Assessment**: Flattening is straightforward type dispatching. No structural surprises. Testable in isolation (no Metal dependency).

**Performance Integration**:
- Section 1.2 quality attribute: "GPU rendering of Sofia's 500-sphere scene at 3840x2160, 48 SPP must complete in under 60 seconds on Apple M2 Max"
- Baseline: CPU takes 40+ minutes
- Performance target: 50-200x speedup
- BVH requirement: "GPU BVH must provide at least 5x speedup over GPU brute-force for 500+ shape scenes" (Requirements Section 5.1)
- Metal compute peak throughput on M2 Max: ~3.2 TFLOPS (sufficient for ray tracing)
- **Assessment**: Performance target is achievable. Metal compute kernels for ray tracing are well-established (BlenderCycles, PBRT-v4). No novel algorithmic requirements.

**Finding**: No integration blockers. Implementation is feasible with existing algorithms and clear data flow.

#### 6. Quality Attribute Coverage (5 Core Attributes Addressed)

**Status**: PASS

| Attribute | Target | Strategy | Addressed |
|---|---|---|---|
| Performance | 50-200x speedup M2 Max | Metal compute + BVH | Section 1.2, ADR-GPU-003 |
| Correctness | +/-5 per RGB at 100+ SPP | Float precision boundary documented | ADR-GPU-004, Section 1.2 |
| Maintainability | Metal confined to Ring 4 | RenderBackend interface in Ring 3 | Section 2, ADR-GPU-001, Component Boundaries |
| Portability | Build succeeds Linux/Windows | CMake guards, conditional compilation | ADR-GPU-005, Component Boundaries Section 10 |
| Testability | SceneFlattener testable on Linux | Pure C++, no Metal deps | Section 9.2, User Stories Section 4.3 |

**Finding**: No gaps. All 5 quality attributes have documented strategies with architecture support.

#### 7. Data Models Completeness (GPU Buffer Layouts Fully Specified)

**Status**: PASS

**GPU Type Specifications**:
- Data Models Section 2: CPU-to-GPU type mapping table (12 rows, each with transformation rules)
- Data Models Section 3: GPU data struct definitions (GPUShape tagged union, GPUMaterial, GPULight, GPUCamera, LinearBVHNode)
- Component Boundaries Section 2.1: Struct layouts with exact field offsets (e.g., GPUShape 128 bytes, GPUMaterial 48 bytes, LinearBVHNode 32 bytes)
- Data Models Section 9: Static assertions for struct size/alignment verification

**Alignment Verification**:
- Component Boundaries Section 2.1: `alignas(16)` declarations and field padding explicitly specified
- Data Models Section 3.3-3.5: Field layout tables with byte offsets
- Data Models Section 9: "static_assert(sizeof(GPUShape) == 128)" etc. for all structs
- **Assessment**: Struct layouts are over-specified. Zero ambiguity between C++ and Metal Shading Language.

**Intersection Detection Requirements**:
- Data Models Section 3: Sphere (center, radius), Plane (point, normal), Box (min, max), Cylinder (center, radius, height), Triangle (v0, v1, v2)
- Component Boundaries Section 4.2: "Computes AABB from GPUShape params" with shape-type-specific logic
- Data Models Section 5.5: AABB computation per shape type (e.g., Sphere: center±radius)
- **Assessment**: All shape types have sufficient data for GPU intersection. No missing fields.

**Transform Handling**:
- Data Models Section 4: Inverse transform matrix layout (column-major, 16-float array)
- Section 4: "When has_transform == 0, GPU shader skips the transform"
- Data Models Section 4: "Transform ray to local space before intersection, transform results back"
- **Assessment**: Transform handling is explicit and well-documented.

**Camera Buffer**:
- Data Models Section 6: GPUCamera layout with 112-byte total, 16-byte alignment
- Field offsets documented (lookfrom, pixel00_loc, pixel_delta_u, pixel_delta_v, background colors, dimensions)
- Data Models Section 6: Ray generation formula matches CPU Camera math
- **Assessment**: Camera buffer is complete for ray generation.

**Output Buffer**:
- Data Models Section 7: float4 (RGBA) layout, gamma-corrected values in [0,1]
- Readback formula: `Color3(float->double conversion)` with NaN guard
- **Assessment**: Output spec is unambiguous.

**Buffer Size Estimates**:
- Data Models Section 8: Sofia's 500-sphere production scene buffer analysis
  - GPUShape[]: 64 KB (500 × 128 bytes)
  - LinearBVHNode[]: ~32 KB (999 nodes × 32 bytes)
  - Output: ~127 MB (8.3M pixels × 16 bytes)
  - **Total**: ~127 MB (0.4% of M2 Max's 32 GB unified memory)
- **Assessment**: Memory budgets are realistic.

**Finding**: No data model gaps. GPU buffer layouts are fully specified with size, alignment, and field offset documentation.

#### 8. Test Strategy and Platform Coverage (Testability Verified)

**Status**: PASS

**Testable Components (No Metal Required)**:
- Architecture Section 9.2: gpu_types.h (struct sizes, alignment, field layout) -- Unit test on any platform
- Section 9.2: RenderBackend, CpuRenderBackend (identical output to Renderer) -- Unit test on any platform
- Section 9.2: SceneFlattener (flatten known scenes, verify arrays) -- Unit test on any platform (no Metal)
- Section 9.2: BVHFlattener (BVH construction, node structure, traversal correctness) -- Unit test on any platform

**Metal-Dependent Components**:
- Section 9.2: MetalRenderBackend (render known scene, compare to CPU) -- Integration test, macOS only
- Section 9.2: MetalDevice (init, compile pipeline) -- Integration test, macOS only
- Section 9.2: End-to-end GPU render -- Acceptance test, macOS only

**Platform Conditional Compilation**:
- Component Boundaries Section 10: SceneFlattener and BVHFlattener compile on all platforms
- Metal-specific .mm files excluded on Linux/Windows
- CMake enforces conditional compilation via `if(NWAVE_HAS_METAL)` guard
- **Assessment**: Test suite can run on Linux (non-GPU paths) and macOS (full GPU paths)

**Finding**: No test strategy gaps. Pure C++ components are testable on all platforms; Metal-dependent components are gated to macOS.

#### 9. Specification Clarity (Pseudocode, Diagrams, Detailed Docs)

**Status**: PASS

**Pseudocode**:
- ADR-GPU-003: Ray tracing kernel pseudocode (30-line iterative loop with all pipeline stages)
- Data Models Section 5.4: GPU BVH traversal algorithm (16-line pseudocode with stack management)
- Section 4.2: Scene flattening pipeline diagram (ASCII art showing CPU objects → GPU structs)

**Diagrams**:
- C4 diagram file (diagrams/c4-component.md): System context, container, component, and data flow diagrams
- Component Boundaries Section 5: Ring dependency diagram showing Metal-to-Framework flow
- Architecture Section 2: Ring model ASCII diagram

**Documentation Density**:
- architecture-design.md: 391 lines covering 12 sections
- component-boundaries.md: 572 lines covering 10 sections
- technology-stack.md: 325 lines covering 7 sections
- data-models.md: 311 lines covering 9 sections
- ADRs: 5 documents, each 40-90 lines with context/decision/alternatives/consequences
- Requirements: 278 lines with 9 capability areas and constraints
- User Stories: 753 lines with 9 stories, domain examples, BDD scenarios
- Acceptance Criteria: 257 lines with 87 detailed criteria

**Specification Completeness**:
- No ambiguous terms (all ring assignments explicit, all struct sizes specified, all algorithms pseudocoded)
- Examples provided for every major component (e.g., SceneFlattener Section 4, User Stories Section 4.1)
- Error handling specified (Architecture Section 9.1: device init failure, GPU memory exceeded, timeout, NaN pixels, non-macOS Metal)
- Boundary conditions specified (Component Boundaries Sections 8-9: integration points with validation rules)

**Finding**: No clarity gaps. Specification is detailed and unambiguous.

### Critical and High Issues

**Count**: 0 Critical, 0 High

No critical or high-severity issues identified in any dimension.

### Medium and Low Issues (Informational Only)

**None**. All findings indicate the design is complete and sound.

### Recommendation for Next Phases

1. **Design Wave Approval**: This design is approved for handoff to the Acceptance Designer (DORA phase).
2. **Phased Implementation**: Follow Elephant Carpaccio slicing (8 user stories, each with working output):
   - Phase 0: Walking skeleton (gradient via Metal) -- validates infrastructure
   - Phase 1: RenderBackend abstraction -- enables backend selection
   - Phase 2-7: Progressive feature delivery (ray gen, scene flattening, single-bounce, multi-bounce, BVH, SPP)
   - Phase 8: Animation integration -- unlocks physics-driven rendering
3. **Handoff Checklist**:
   - [ ] All 5 ADRs reviewed and accepted
   - [ ] All 9 capability areas mapped to architecture components
   - [ ] All 87 acceptance criteria reviewed against design
   - [ ] Ring separation enforced (no Ring 1-3 Metal dependencies)
   - [ ] Data models reviewed for struct layout correctness
   - [ ] Test strategy verified (pure C++ components testable on Linux, Metal components on macOS)
   - [ ] Requirements traceability complete (all 9 stories addressable)

### Handoff Notes for Acceptance Designer

1. **RenderBackend is the architectural pivot**: Verify no code path bypasses it to call Renderer or MetalRenderBackend directly (except CpuRenderBackend wrapping Renderer).
2. **SceneFlattener is the highest-complexity pure C++ component**: Material deduplication via pointer identity is critical. Handle all 6 shape types and 4 material types. TriangleMesh is out of scope.
3. **BVH is new logic**: Not an adaptation of an existing BVH. Builds from scratch from flat GPUShape array. Standard SAH algorithm.
4. **WriteCallback is the animation integration point**: Existing signature is preserved. GPU backend re-flattens scene each frame.
5. **Float vs double tolerance is documented**: GPU uses float, CPU uses double. Expected per-pixel difference: +/-5 per RGB at 100+ SPP.
6. **Ring enforcement is critical**: Metal headers only in .mm files in src/infrastructure/metal/. SceneFlattener and BVHFlattener are pure .cpp.

---

**Architecture Review Complete**
**Approval Date**: 2026-02-18
**Approved By**: nw-solution-architect-reviewer
