# Technology Stack: Scene Physics Animation

**Document ID**: TECH-SPA-001
**Feature**: scene-physics-animation
**Date**: 2026-02-17
**Status**: Draft
**Extends**: TECH-RAYTRACER-001 (existing technology stack)

---

## 1. New External Dependencies

This feature adds two external libraries. Both are open source with permissive licenses. Both integrate via CMake FetchContent, matching the existing GoogleTest integration pattern.

### 1.1 Jolt Physics (Rigid Body Simulation)

| Attribute | Value |
|---|---|
| **Purpose** | Rigid body dynamics: gravity, collisions, bouncing, resting |
| **Version** | v5.2.0 (or latest stable tag) |
| **License** | MIT |
| **C++ Standard** | C++17 (matches project) |
| **Dependencies** | STL only (no external dependencies) |
| **Platforms** | Windows, Linux, macOS, iOS, Android, FreeBSD, WebAssembly |
| **Integration** | CMake FetchContent |
| **Ring** | Infrastructure (Ring 4 only) |
| **Double Precision** | Enabled via `DOUBLE_PRECISION` CMake option (matches ray tracer's use of `double`) |
| **Multi-threading** | Built-in job system with thread pool |
| **RTTI/Exceptions** | Does not use either |
| **Source Repository** | https://github.com/jrouwe/JoltPhysics |

**Rationale**:
- Same C++17 standard as the ray tracer
- Zero external dependencies (STL only), matching the project's minimal dependency philosophy
- CMake build system with FetchContent support -- identical integration pattern to existing GoogleTest
- Double precision mode aligns with the ray tracer's use of `double` throughout (avoids float-to-double conversion noise)
- MIT license is maximally permissive
- Production-proven: used in Horizon Forbidden West, Death Stranding 2
- Collision shapes cover all ray tracer primitives: SphereShape, BoxShape, CylinderShape, PlaneShape, MeshShape

**Collision Shapes Available**:

| Ray Tracer Shape | Jolt Collision Shape | Mapping Complexity |
|---|---|---|
| Sphere | SphereShape(radius) | Direct 1:1 |
| Box | BoxShape(half_extents) | Convert min/max to center + half-extents |
| Cylinder | CylinderShape(half_height, radius) | Convert center/height to half-height |
| Plane | Large static BoxShape or PlaneShape | Static-only; represents ground |
| TriangleMesh | MeshShape(vertices, indices) | Static-only; concave meshes cannot be dynamic |

**Alternatives Considered**:

| Alternative | Evaluation | Rejection Reason |
|---|---|---|
| **ReactPhysics3D** | Simpler API, zero dependencies, ZLib license | No native CylinderShape (would need convex mesh approximation). No documented double precision mode. Fewer collision shapes (6 vs 11). |
| **Bullet Physics** | Most mature, used by Blender, ZLib license | Aging C++ style (not modern C++17). Heavier integration burden. Jolt was designed as its modern replacement. |
| **NVIDIA PhysX** | Most feature-complete, GPU acceleration | Substantially more complex to integrate. Heavy dependency footprint. Designed for real-time game engines, not offline rendering. |
| **Custom minimal physics** | Full control, educational | 3-6 weeks estimated effort for gravity + sphere/plane/box collisions + impulse solver. Blocks the feature for weeks. |

### 1.2 yaml-cpp (YAML Parsing)

| Attribute | Value |
|---|---|
| **Purpose** | Parse YAML scene files into C++ data structures |
| **Version** | 0.8.x (latest stable) |
| **License** | MIT |
| **C++ Standard** | C++11+ |
| **Dependencies** | None |
| **Integration** | CMake FetchContent |
| **Ring** | Infrastructure (Ring 4 only) |
| **Source Repository** | https://github.com/jbeder/yaml-cpp |

**Rationale**:
- Most widely used C++ YAML parser
- MIT license, well-maintained
- No external dependencies of its own
- Node-based API: `YAML::LoadFile(path)` returns a tree of nodes accessed by key or index
- Exception-based error reporting with line/column information (useful for user-facing error messages)

**Alternatives Considered**:

| Alternative | Evaluation | Rejection Reason |
|---|---|---|
| **rapidyaml (ryml)** | Faster parsing, header-only option | Less mature API, fewer examples. Scene files are small and parsed once -- parsing speed is irrelevant. |
| **nlohmann/json** | Would require switching scene format to JSON | JSON lacks comments, requires quoted keys, is more verbose. YAML was the user-selected format. |
| **Custom YAML parser** | Full control | Significant effort for a solved problem. YAML spec is complex (indentation, anchors, type coercion). Not core to the ray tracing domain. |

---

## 2. CMake FetchContent Configuration

### 2.1 Root CMakeLists.txt Additions

The following FetchContent declarations are added alongside the existing GoogleTest integration:

```cmake
# Jolt Physics
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.2.0
    SOURCE_SUBDIR  Build
)
set(DOUBLE_PRECISION ON CACHE BOOL "Use double precision in Jolt Physics" FORCE)
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JoltPhysics)

# yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)
```

### 2.2 src/CMakeLists.txt Additions

New source files and link targets:

```cmake
# Ring 1: Core (add new math types)
add_library(nwave_core
    core/aabb.cpp
    # NEW: matrix4x4.cpp, quaternion.cpp (if not header-only)
)

# Ring 2: Domain (add new domain types)
add_library(nwave_domain
    # ... existing sources ...
    # NEW:
    domain/shapes/transformed_shape.cpp
)

# Ring 3: Application (add AnimationRenderer)
add_library(nwave_application
    application/renderer.cpp
    # NEW:
    application/animation_renderer.cpp
)

# Ring 4: Infrastructure (add new adapters)
add_library(nwave_infrastructure
    infrastructure/ppm_writer.cpp
    # NEW:
    infrastructure/yaml_scene_loader.cpp
    infrastructure/validator.cpp
    infrastructure/jolt_physics_simulator.cpp
    infrastructure/cli_dispatcher.cpp
)
target_link_libraries(nwave_infrastructure PUBLIC nwave_application yaml-cpp::yaml-cpp Jolt)
```

Key points:
- `yaml-cpp::yaml-cpp` and `Jolt` are linked ONLY to `nwave_infrastructure` (Ring 4)
- `nwave_core`, `nwave_domain`, and `nwave_application` have NO new external dependencies
- The dependency rule is preserved: only the outermost ring touches external libraries

---

## 3. Jolt Physics Integration Details

### 3.1 Initialization Requirements

Jolt Physics requires several one-time setup steps before the physics system can be used:

1. **Register allocator**: `JPH::RegisterDefaultAllocator()` -- must be called before any Jolt object creation
2. **Create factory**: `JPH::Factory::sInstance = new JPH::Factory()` -- required for shape deserialization
3. **Register types**: `JPH::RegisterTypes()` -- registers all built-in shape types
4. **Temp allocator**: `JPH::TempAllocatorImpl(10 * 1024 * 1024)` -- 10 MB scratch memory for collision detection
5. **Job system**: `JPH::JobSystemThreadPool(...)` -- thread pool for multi-threaded simulation
6. **Physics system**: `JPH::PhysicsSystem` -- the main simulation object

### 3.2 Layer/Filter Callbacks

Jolt requires three callback interfaces for its broad-phase collision filtering:

- **BroadPhaseLayerInterface**: Maps object layers to broad-phase layers. For nwave, two layers suffice: NON_MOVING (static) and MOVING (dynamic/kinematic).
- **ObjectVsBroadPhaseLayerFilter**: Determines which object layers collide with which broad-phase layers.
- **ObjectLayerPairFilter**: Determines which specific object layer pairs can collide.

These are minimal implementations inside JoltPhysicsSimulator (Ring 4). They are Jolt-specific boilerplate, not domain logic.

### 3.3 Double Precision Considerations

With `DOUBLE_PRECISION` enabled:
- Jolt uses `JPH::DVec3` (double) instead of `JPH::Vec3` (float) for positions
- `JPH::RVec3` becomes `JPH::DVec3` (a typedef that switches based on the precision flag)
- Quaternions remain float-precision in Jolt (acceptable: quaternion components are always in [-1, 1])
- The adapter must convert Jolt's float quaternion to nwave's double quaternion (trivial widening cast, no precision loss)

### 3.4 Cleanup Requirements

When the JoltPhysicsSimulator is destroyed:
1. Remove all bodies from the physics system
2. Destroy all bodies via the BodyInterface
3. Unregister types: `JPH::UnregisterTypes()`
4. Delete the factory: `delete JPH::Factory::sInstance`

All cleanup is encapsulated in JoltPhysicsSimulator's destructor.

---

## 4. Updated Dependency Summary

```
Production Dependencies:
  Ring 1 (Core):           C++17 standard library only
  Ring 2 (Domain):         C++17 standard library only
  Ring 3 (Application):    C++17 standard library only
  Ring 4 (Infrastructure): yaml-cpp 0.8.x (MIT)
                           Jolt Physics 5.2.x (MIT)
                           stb_image_write (MIT/PD)

Test Dependencies:
  All rings:               GoogleTest 1.14.x (BSD 3-Clause)

Build Dependencies:
  CMake 3.16+
  C++17-compliant compiler
```

All external dependencies are open source with permissive licenses (MIT, Public Domain, BSD). No proprietary or copyleft libraries are used.

---

## 5. Build Time Impact

| Component | Estimated First-Build Time | Subsequent Builds |
|---|---|---|
| Jolt Physics (FetchContent) | ~60-90s (download + compile) | ~5s (incremental, only if Jolt headers change) |
| yaml-cpp (FetchContent) | ~15-20s (download + compile) | ~2s (incremental) |
| nwave sources | ~5-10s | ~2-5s |
| GoogleTest | ~10-15s | ~2s |

First build will be slower due to Jolt Physics compilation (~100k lines of C++). Subsequent builds only recompile changed nwave sources. Jolt and yaml-cpp are cached by CMake's FetchContent and only rebuilt when their version tags change.
