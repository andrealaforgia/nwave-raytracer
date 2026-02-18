# GPU Compute Rendering via Metal

**Date**: 2026-02-18
**Feature ID**: gpu-compute-rendering
**Status**: Complete

## Summary

Implemented GPU compute rendering via Apple Metal for the nwave C++17 Whitted-style ray tracer. The GPU backend achieves feature parity with the CPU renderer for all 5 shape types, 4 material types, 2 light types, BVH acceleration, multi-bounce ray tracing, anti-aliasing via SPP accumulation, and animation pipeline integration.

## Architecture

### Ring Structure (Hexagonal Architecture)

- **Ring 1 (Core)**: `gpu_types.h` — GPU data structs (GPUCamera, GPUShape, GPUMaterial, GPULight, LinearBVHNode) with static_assert size/alignment guards
- **Ring 3 (Application)**: `RenderBackend` abstract interface, `CpuRenderBackend` wrapping existing Renderer
- **Ring 4 (Infrastructure)**: Metal backend (`MetalDevice`, `MetalBufferManager`, `MetalRenderBackend`), pure C++ GPU utilities (`SceneFlattener`, `BVHFlattener`), Metal compute shaders

### Key Design Decisions

1. **Pimpl pattern** for Metal types — Objective-C++ hidden behind pure C++ headers
2. **Strategy pattern** — `RenderBackend` interface enables CPU/GPU polymorphism
3. **SceneFlattener** — pure C++ visitor converting polymorphic Scene into flat GPU arrays, testable on any platform
4. **BVHFlattener** — CPU-side BVH construction with median split, flattened to linear array for GPU traversal
5. **GTEST_SKIP()** — Metal tests gracefully skip when metallib unavailable (Command Line Tools only)

## Phases Delivered

| Phase | Description | Steps | Key Deliverable |
|-------|-------------|-------|-----------------|
| 01 | Walking Skeleton | 01-01 to 01-05 | End-to-end Metal gradient PPM via GPU dispatch |
| 02 | Render Backend Abstraction | 02-01 to 02-04 | RenderBackend interface, CPU/GPU polymorphism |
| 03 | Ray Gen + Sky on GPU | 03-01 to 03-02 | Pixel-identical sky gradient between CPU and GPU |
| 04 | Scene Data Packing | 04-01 to 04-04 | SceneFlattener: 5 shapes, 4 materials, 2 lights |
| 05 | Single-Bounce GPU Render | 05-01 to 05-02 | GPU intersection for all 5 shapes + Lambertian shading |
| 06 | Multi-Bounce Iterative | 06-01 to 06-02 | Iterative ray tracing, Metal/Dielectric/Emissive materials |
| 07 | Linear BVH on GPU | 07-01 to 07-03 | Stack-based BVH traversal, performance validation |
| 08 | SPP Accumulation | 08-01 to 08-02 | Anti-aliasing, gamma correction, NaN clamping |
| 09 | Animation Integration | 09-01 to 09-02 | Per-frame re-flattening, multi-frame rendering |

## Files Created

### Production (14 new files)
- `src/core/gpu_types.h`
- `src/application/render_backend.h`
- `src/application/cpu_render_backend.h`, `.cpp`
- `src/infrastructure/gpu/scene_flattener.h`, `.cpp`
- `src/infrastructure/gpu/bvh_flattener.h`, `.cpp`
- `src/infrastructure/metal/metal_device.h`, `.mm`
- `src/infrastructure/metal/metal_buffer_manager.h`, `.mm`
- `src/infrastructure/metal/metal_render_backend.h`, `.mm`
- `src/infrastructure/metal/shaders/gradient.metal`
- `src/infrastructure/metal/shaders/ray_trace.metal`

### Tests (6 new files)
- `tests/core/gpu_types_test.cpp`
- `tests/application/render_backend_test.cpp`
- `tests/application/cpu_render_backend_test.cpp`
- `tests/infrastructure/gpu/scene_flattener_test.cpp`
- `tests/infrastructure/gpu/bvh_flattener_test.cpp`
- `tests/infrastructure/metal/metal_render_backend_test.mm`

### Modified (5 files)
- `CMakeLists.txt` — NWAVE_ENABLE_GPU option, Metal detection
- `src/CMakeLists.txt` — GPU sources, Metal framework linking, shader compilation
- `src/main.cpp` — backend dispatch, WriteCallback integration
- `src/domain/camera.h` — public getters for GPU packing

## Metrics

- **26 TDD steps** executed across 9 phases
- **334 total tests** (276 platform-independent + 58 Metal)
- **27 feature commits** + 1 refactoring commit
- **~800 LOC** Metal shader (ray_trace.metal)
- **~220 LOC** SceneFlattener
- **~150 LOC** BVHFlattener
- **0 test regressions**

## GPU Rendering Pipeline

```
Scene → SceneFlattener → FlatScene → BVHFlattener → LinearBVH
                                                        ↓
Camera → pack_gpu_camera → GPUCamera ──────→ Metal Buffers → ray_trace_kernel → pixels
```

## Shader Capabilities

- Ray-shape intersection: Sphere (quadratic), Plane (dot product), Box (slab), Cylinder (quadratic+caps), Triangle (Moller-Trumbore)
- TransformedShape support via inverse matrix ray transformation
- Iterative multi-bounce ray tracing (replaces CPU recursion)
- Material evaluation: Lambertian (cosine scatter), Metal (reflect+fuzz), Dielectric (Snell+Schlick), Emissive
- Shadow rays with multi-light accumulation
- Per-pixel SPP accumulation with sub-pixel jitter (PCG RNG)
- Stack-based BVH traversal (64-entry stack)
- Gamma correction (sqrt) with NaN/inf clamping
