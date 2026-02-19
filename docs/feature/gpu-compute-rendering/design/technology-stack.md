# Technology Stack: GPU Compute Rendering

**Document ID**: TECH-GPU-001
**Feature**: gpu-compute-rendering
**Date**: 2026-02-18
**Status**: Draft
**Extends**: TECH-SPA-001 (scene-physics-animation technology stack)

---

## 1. New Technology Dependencies

This feature adds no new external libraries. It uses Apple's Metal framework (a system framework on macOS) and extends the existing CMake build system.

### 1.1 Metal Framework (GPU Compute)

| Attribute | Value |
|---|---|
| **Purpose** | GPU compute shader dispatch for ray tracing |
| **Version** | Metal 3 (available on Apple Silicon; Metal 2 on Intel Macs) |
| **License** | Apple system framework -- no separate license; ships with macOS |
| **Platform** | macOS only (Apple Silicon and Intel Macs with Metal support) |
| **Integration** | CMake `find_library(Metal)` and `find_library(Foundation)` |
| **Ring** | Infrastructure (Ring 4 only) |
| **Language** | Objective-C++ (.mm) for API calls; Metal Shading Language (.metal) for shaders |
| **Cost** | Free (system framework) |

**Rationale**:
- Metal is the only GPU compute API on macOS. There is no alternative for Apple Silicon GPUs.
- Compute shaders are the appropriate Metal feature for offline ray tracing (not render pipeline, not ray tracing API).
- System framework: zero additional dependency, no download, no license cost.
- Metal Shading Language (MSL) is C++14-based -- familiar syntax for the existing C++17 codebase.
- Apple's unified memory architecture (Apple Silicon) means zero explicit CPU-GPU data transfer overhead -- shared buffers are visible to both.

**Alternatives Considered**:

| Alternative | Evaluation | Rejection Reason |
|---|---|---|
| **OpenCL** | Cross-platform GPU compute | Deprecated on macOS since macOS 10.14. Apple actively pushes Metal as replacement. No future investment from Apple. |
| **Vulkan (MoltenVK)** | Cross-platform via Metal translation layer | Adds MoltenVK dependency (~200MB). Overhead of Vulkan-to-Metal translation. Over-engineered for a macOS-only first implementation. Future cross-platform story, not this one. |
| **CUDA** | Industry standard for GPU compute | Not available on macOS. NVIDIA GPUs not supported on Apple Silicon. |
| **WebGPU (Dawn)** | Emerging cross-platform GPU standard | Immature for compute workloads. Adds large dependency (Dawn). Not production-ready for ray tracing compute. |

### 1.2 Foundation Framework

| Attribute | Value |
|---|---|
| **Purpose** | NSString, NSError handling required by Metal API |
| **Version** | Ships with macOS |
| **License** | Apple system framework |
| **Platform** | macOS only |
| **Integration** | CMake `find_library(Foundation)` |

Required because Metal API is Objective-C based. Needed for `MTLCreateSystemDefaultDevice()`, `NSError*` handling, and `.metallib` loading.

### 1.3 Metal Shading Language (MSL)

| Attribute | Value |
|---|---|
| **Purpose** | Compute shader source language |
| **Version** | MSL 3.0 (Apple Silicon) / MSL 2.4 (Intel Macs) |
| **Compiler** | `xcrun -sdk macosx metal` (part of Xcode Command Line Tools) |
| **Linker** | `xcrun -sdk macosx metallib` |
| **Output** | `.air` (intermediate) -> `.metallib` (final shader library) |
| **C++ Compatibility** | C++14-based syntax with GPU-specific extensions |

MSL supports:
- `float` arithmetic (no `double` in compute shaders)
- Structs with explicit buffer binding
- Threadgroup shared memory
- Atomic operations
- Math functions: `sqrt`, `sin`, `cos`, `fma`, `rsqrt`, `normalize`, `dot`, `cross`, `reflect`, `refract`

---

## 2. Build System Additions

### 2.1 Root CMakeLists.txt Changes

```cmake
# GPU support option (defaults to OFF -- opt-in)
option(NWAVE_ENABLE_GPU "Enable Metal GPU compute backend (macOS only)" OFF)

# Metal detection (only on macOS with GPU enabled)
if(APPLE AND NWAVE_ENABLE_GPU)
    find_library(METAL_FRAMEWORK Metal)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_program(METAL_COMPILER xcrun)

    if(METAL_FRAMEWORK AND FOUNDATION_FRAMEWORK AND METAL_COMPILER)
        set(NWAVE_HAS_METAL TRUE)
        message(STATUS "Metal GPU support: ENABLED")
        message(STATUS "  Metal framework: ${METAL_FRAMEWORK}")
        message(STATUS "  Foundation framework: ${FOUNDATION_FRAMEWORK}")
    else()
        set(NWAVE_HAS_METAL FALSE)
        message(WARNING "NWAVE_ENABLE_GPU=ON but Metal not found. GPU disabled.")
    endif()
elseif(NWAVE_ENABLE_GPU AND NOT APPLE)
    message(WARNING "NWAVE_ENABLE_GPU=ON ignored on non-macOS. Metal requires macOS.")
    set(NWAVE_HAS_METAL FALSE)
else()
    set(NWAVE_HAS_METAL FALSE)
    message(STATUS "Metal GPU support: NOT AVAILABLE (NWAVE_ENABLE_GPU=OFF)")
endif()
```

### 2.2 Metal Shader Compilation (CMake Custom Commands)

```cmake
if(NWAVE_HAS_METAL)
    # Compile .metal -> .air (intermediate representation)
    set(METAL_SHADER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/metal/shaders/ray_trace.metal
        ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/metal/shaders/gradient.metal
    )
    set(METAL_AIR_FILES "")
    foreach(SHADER ${METAL_SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
        set(AIR_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.air)
        add_custom_command(
            OUTPUT ${AIR_FILE}
            COMMAND xcrun -sdk macosx metal
                    -c ${SHADER}
                    -o ${AIR_FILE}
                    -std=metal3.0
            DEPENDS ${SHADER}
            COMMENT "Compiling Metal shader: ${SHADER_NAME}.metal"
        )
        list(APPEND METAL_AIR_FILES ${AIR_FILE})
    endforeach()

    # Link .air -> .metallib
    set(METALLIB_FILE ${CMAKE_CURRENT_BINARY_DIR}/nwave_shaders.metallib)
    add_custom_command(
        OUTPUT ${METALLIB_FILE}
        COMMAND xcrun -sdk macosx metallib
                ${METAL_AIR_FILES}
                -o ${METALLIB_FILE}
        DEPENDS ${METAL_AIR_FILES}
        COMMENT "Linking Metal library: nwave_shaders.metallib"
    )
    add_custom_target(nwave_metal_shaders DEPENDS ${METALLIB_FILE})
endif()
```

### 2.3 src/CMakeLists.txt Changes

```cmake
# GPU data components (pure C++, compile on all platforms)
set(GPU_COMMON_SOURCES
    infrastructure/gpu/scene_flattener.cpp
    infrastructure/gpu/bvh_flattener.cpp
)

# Metal-specific components (macOS only, Objective-C++)
if(NWAVE_HAS_METAL)
    set(METAL_SOURCES
        infrastructure/metal/metal_render_backend.mm
        infrastructure/metal/metal_device.mm
        infrastructure/metal/metal_buffer_manager.mm
    )
    set_source_files_properties(${METAL_SOURCES}
        PROPERTIES COMPILE_FLAGS "-x objective-c++"
    )
else()
    set(METAL_SOURCES "")
endif()

# Add to infrastructure library
add_library(nwave_infrastructure
    # ... existing sources ...
    ${GPU_COMMON_SOURCES}
    ${METAL_SOURCES}
)

# Conditional Metal framework linking
if(NWAVE_HAS_METAL)
    target_link_libraries(nwave_infrastructure PUBLIC
        ${METAL_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )
    target_compile_definitions(nwave_infrastructure PUBLIC NWAVE_HAS_METAL=1)
    add_dependencies(nwave_infrastructure nwave_metal_shaders)

    # Copy metallib alongside binary
    add_custom_command(TARGET nwave POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${METALLIB_FILE}
            $<TARGET_FILE_DIR:nwave>/nwave_shaders.metallib
        COMMENT "Copying Metal shader library"
    )
endif()
```

---

## 3. Objective-C++ Integration Details

### 3.1 File Naming Convention

| Extension | Language | Ring | Purpose |
|---|---|---|---|
| `.h` | C++ header | 1-4 | All pure C++ interfaces and types |
| `.cpp` | C++ source | 1-4 | All pure C++ implementations |
| `.mm` | Objective-C++ | 4 only | Metal API calls, Foundation types |
| `.metal` | Metal Shading Language | 4 only | GPU compute kernels |

### 3.2 Objective-C++ Bridging Pattern

The `.mm` files use a "C++ facade" pattern: the public header (`.h`) exposes a pure C++ interface (no Objective-C types). The implementation (`.mm`) internally uses Objective-C objects via `id<MTLDevice>` etc. This keeps the Objective-C boundary inside the implementation file.

Example:
```
// metal_device.h (pure C++ interface)
class MetalDevice {
public:
    MetalDevice();
    ~MetalDevice();
    bool is_available() const;
    // ... pure C++ types only in interface
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;  // pimpl hides Objective-C types
};
```

```
// metal_device.mm (Objective-C++ implementation)
#import <Metal/Metal.h>

struct MetalDevice::Impl {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLComputePipelineState> pipeline;
    id<MTLLibrary> library;
};
// ... Objective-C code here
```

The pimpl pattern ensures that Metal/Foundation headers are never visible in `.h` files, preventing accidental inclusion in Ring 1-3 code.

---

## 4. Metal Compute Pipeline Configuration

### 4.1 Threadgroup Sizing

| Parameter | Value | Rationale |
|---|---|---|
| Threadgroup width | 16 | Standard for Metal compute; 16x16 = 256 threads matches Apple GPU wavefront size |
| Threadgroup height | 16 | 256 threads per threadgroup is optimal for most Metal GPUs |
| Grid size | ceil(width/16) x ceil(height/16) | Covers full image; shader checks bounds for non-divisible dimensions |

### 4.2 Buffer Binding Indices

| Index | Buffer | Contents |
|---|---|---|
| 0 | shapes | GPUShape[] |
| 1 | materials | GPUMaterial[] |
| 2 | lights | GPULight[] |
| 3 | bvh | LinearBVHNode[] |
| 4 | camera | GPUCamera (single struct) |
| 5 | output | float4[] (width * height) |
| 6 | shape_count | uint32_t |
| 7 | light_count | uint32_t |
| 8 | bvh_node_count | uint32_t |

### 4.3 Memory Mode

All buffers use `MTLResourceStorageModeShared` (Apple Silicon unified memory). This allows CPU write + GPU read with zero explicit copy on Apple Silicon. On Intel Macs, the driver handles coherence automatically.

---

## 5. Updated Dependency Summary

```
Production Dependencies:
  Ring 1 (Core):           C++17 standard library only
  Ring 2 (Domain):         C++17 standard library only
  Ring 3 (Application):    C++17 standard library only
  Ring 4 (Infrastructure): yaml-cpp 0.8.x (MIT)
                           Jolt Physics 5.2.x (MIT)
                           stb_image_write (MIT/PD)
                           [GPU, macOS only] Metal.framework (system)
                           [GPU, macOS only] Foundation.framework (system)

Test Dependencies:
  All rings:               GoogleTest 1.14.x (BSD 3-Clause)

Build Dependencies:
  CMake 3.16+
  C++17-compliant compiler
  [GPU, macOS only] Xcode Command Line Tools (provides 'metal' and 'metallib' compilers)
```

All external dependencies remain open source with permissive licenses. Metal.framework and Foundation.framework are Apple system frameworks (no separate license, no download, no cost).

---

## 6. Build Time Impact

| Component | First Build Time | Subsequent Builds |
|---|---|---|
| Metal shader compilation (.metal -> .metallib) | ~2-5s | ~1-2s (only if .metal files change) |
| Objective-C++ compilation (3 .mm files) | ~3-5s | ~1-2s (incremental) |
| SceneFlattener + BVHFlattener (.cpp) | ~1-2s | ~1s (incremental) |
| Existing sources | Unchanged | Unchanged |

GPU support adds approximately 10 seconds to the first build and 2-3 seconds to incremental builds when shader or .mm files change. Non-GPU incremental builds are unaffected.

---

## 7. Metal Shader Language Constraints

| Constraint | Impact on Architecture |
|---|---|
| No `double` in compute shaders | All GPU data uses `float`. SceneFlattener narrows double->float. Tolerance documented. |
| No recursion | Ray tracing loop is iterative with explicit throughput accumulation. |
| No virtual dispatch | Shape/material types use tagged union enums with switch statements. |
| No heap allocation | All data in pre-allocated Metal buffers. BVH traversal uses fixed-size stack (64 entries). |
| No C++ STL | Math reimplemented in MSL (built-in functions: `normalize`, `dot`, `cross`, `reflect`, `refract`, `sqrt`, `clamp`). |
| No exceptions | Error handling via return values and NaN guards. |
| Struct alignment | 16-byte alignment required for buffer access. All GPU structs use `alignas(16)`. |
