# ADR-GPU-001: RenderBackend Abstraction Location and Contract

## Status

Accepted

## Context

The nwave ray tracer needs to support both CPU and Metal GPU rendering for the same scene. Currently, `Renderer` is a concrete class in Ring 3 (Application) called directly by `main.cpp` and indirectly by `AnimationRenderer`'s `WriteCallback`. We need a polymorphic dispatch point that allows callers to use either backend without knowing which one is active.

Three design questions must be answered:
1. Where does the abstraction live in the ring model?
2. What is the interface contract?
3. How does the existing `Renderer` relate to the new abstraction?

## Decision

**Location**: `RenderBackend` is an abstract interface in Ring 3 (Application), defined in `src/application/render_backend.h`.

**Contract**: A single method: `virtual std::vector<Color3> render(const Camera& camera, const Scene& scene, const RenderSettings& settings) = 0;`

**Relationship to Renderer**: The existing `Renderer` class is NOT modified. A new `CpuRenderBackend` class in Ring 3 wraps `Renderer` via composition and delegates `render()` to it. `MetalRenderBackend` in Ring 4 implements the GPU path.

**Backend selection**: Determined in `main.cpp` based on `RenderCommand.backend` string. A simple conditional creates the appropriate backend. No factory pattern or registry needed for two backends.

## Alternatives Considered

### Alternative 1: Make Renderer virtual and add GPU methods
Modify the existing `Renderer` class to be abstract, adding a virtual `render()` method that the GPU backend overrides.

**Rejected because**: Violates Single Responsibility. `Renderer` currently owns CPU-specific logic (thread pool, scanline splitting, recursive `trace_ray`). Making it the base class for GPU rendering creates a misleading inheritance hierarchy where the GPU backend inherits thread pool management it does not use. Also requires modifying a class that all 243 existing tests depend on.

### Alternative 2: RenderBackend in Ring 2 (Domain)
Place the abstraction in the Domain ring, closer to Camera and Scene.

**Rejected because**: Rendering is an Application concern (use case orchestration), not a Domain concern (business rules). Domain objects (Camera, Scene, Shape, Material) describe the world; rendering is what we do with that description. Ring 3 is the correct location per Clean Architecture.

### Alternative 3: Strategy pattern with function pointer
Use `std::function<std::vector<Color3>(Camera, Scene, RenderSettings)>` instead of an abstract class.

**Rejected because**: An abstract class provides a named type that can be extended with additional methods later (e.g., `supports_feature()`, `device_info()`). A function pointer captures only one operation. The abstract class also enables type-safe runtime identification for diagnostics and error reporting.

## Consequences

- **Positive**: Existing `Renderer` untouched. All 243 tests continue passing without modification. New GPU backends (future Vulkan) add a Ring 4 implementation of the same Ring 3 interface. The `WriteCallback` in `main.cpp` simply captures a `RenderBackend*` instead of creating a `Renderer` directly.
- **Negative**: One additional indirection (virtual dispatch per frame). Cost is negligible: one virtual call per frame, not per ray. `CpuRenderBackend` is a thin wrapper that adds a class file but no logic.
