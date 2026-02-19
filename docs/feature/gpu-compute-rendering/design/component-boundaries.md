# Component Boundaries: GPU Compute Rendering

**Document ID**: COMP-GPU-001
**Feature**: gpu-compute-rendering
**Date**: 2026-02-18
**Status**: Draft
**Extends**: COMP-SPA-001 (scene-physics-animation component boundaries)

---

## 1. Dependency Rule (Unchanged)

| Ring | Can Depend On | Cannot Depend On |
|---|---|---|
| **Ring 1 (Core/Math)** | C++ standard library only | Domain, Application, Infrastructure, any external library |
| **Ring 2 (Domain)** | Ring 1 (Core/Math) | Application, Infrastructure, any external library |
| **Ring 3 (Application)** | Ring 2 (Domain), Ring 1 (Core/Math) | Infrastructure, any external library |
| **Ring 4 (Infrastructure)** | All inner rings, external libraries | Nothing forbidden |

Metal.framework and Foundation.framework are external libraries. They are permitted ONLY in Ring 4.

---

## 2. Ring 1: Core (New Component)

### 2.1 gpu_types.h -- GPU Data Transfer Structs

These are plain C-compatible structs using `float` fields and `uint32_t` enums. They contain no behavior, no constructors beyond aggregate initialization, and no dependencies beyond `<cstdint>`.

**ShapeType Enum**:
```
enum GPUShapeType : uint32_t {
    GPU_SHAPE_SPHERE   = 0,
    GPU_SHAPE_PLANE    = 1,
    GPU_SHAPE_BOX      = 2,
    GPU_SHAPE_CYLINDER = 3,
    GPU_SHAPE_TRIANGLE = 4
};
```

**GPUShape** (tagged union, 16-byte aligned):
```
struct GPUShape {
    uint32_t shape_type;
    uint32_t material_index;
    uint32_t has_transform;     // 0 = no transform, 1 = use inverse_transform
    uint32_t _pad0;

    // Union data (largest member determines size)
    // Sphere: center(3f), radius(1f)
    // Plane: point(3f), normal(3f)
    // Box: box_min(3f), box_max(3f)
    // Cylinder: center(3f), radius(1f), height(1f)
    // Triangle: v0(3f), v1(3f), v2(3f)
    float params[12];           // 48 bytes covers all shape types

    // Optional inverse transform (identity if has_transform == 0)
    float inverse_transform[16]; // 4x4 matrix, column-major
};
// Total: 4+4+4+4 + 48 + 64 = 128 bytes (naturally 16-byte aligned)
```

**MaterialType Enum**:
```
enum GPUMaterialType : uint32_t {
    GPU_MAT_LAMBERTIAN = 0,
    GPU_MAT_METAL      = 1,
    GPU_MAT_DIELECTRIC = 2,
    GPU_MAT_EMISSIVE   = 3
};
```

**GPUMaterial** (tagged union, 16-byte aligned):
```
struct GPUMaterial {
    uint32_t material_type;
    float albedo[3];            // Lambertian/Metal: albedo; Dielectric: tint
    float fuzziness;            // Metal only
    float ior;                  // Dielectric only
    float emission_color[3];    // Emissive only
    float emission_intensity;   // Emissive only
    uint32_t _pad[2];
};
// Total: 48 bytes (16-byte aligned)
```

**LightType Enum**:
```
enum GPULightType : uint32_t {
    GPU_LIGHT_POINT       = 0,
    GPU_LIGHT_DIRECTIONAL = 1
};
```

**GPULight** (tagged union, 16-byte aligned):
```
struct GPULight {
    uint32_t light_type;
    float position[3];         // Point: position; Directional: unused
    float direction[3];        // Directional: direction; Point: unused
    float color[3];
    float intensity;
    uint32_t _pad[1];
};
// Total: 64 bytes (16-byte aligned)
```

**GPUCamera** (16-byte aligned):
```
struct GPUCamera {
    float lookfrom[3];
    float _pad0;
    float pixel00_loc[3];
    float _pad1;
    float pixel_delta_u[3];
    float _pad2;
    float pixel_delta_v[3];
    float _pad3;
    float background_top[3];
    float _pad4;
    float background_bottom[3];
    float _pad5;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t samples_per_pixel;
    uint32_t max_depth;
};
// Total: 112 bytes (16-byte aligned)
```

**LinearBVHNode** (16-byte aligned):
```
struct LinearBVHNode {
    float aabb_min[3];
    uint32_t offset;           // Interior: second child index; Leaf: first primitive index
    float aabb_max[3];
    uint32_t count;            // Interior: 0; Leaf: primitive count (>0)
};
// Total: 32 bytes (16-byte aligned)
```

**Boundary rules**:
- No `#include` outside `<cstdint>` and `<cstddef>`
- No constructors, destructors, or virtual methods
- No Metal types (no `float3`, no `packed_float3`)
- All fields explicitly sized (`float`, `uint32_t`)
- `alignas(16)` on each struct
- `static_assert(sizeof(GPUShape) == 128)` etc. for layout verification
- Compiles on Linux, Windows, macOS without any platform-specific headers

---

## 3. Ring 3: Application (New Components)

### 3.1 RenderBackend (Abstract Interface)

```
class RenderBackend {
public:
    virtual ~RenderBackend() = default;

    virtual std::vector<Color3> render(
        const Camera& camera,
        const Scene& scene,
        const RenderSettings& settings) = 0;
};
```

**Boundary rules**:
- Located in `src/application/render_backend.h`
- Depends on Ring 2: Camera, Scene, RenderSettings (via renderer.h), Color3
- Does NOT depend on Ring 4 or any external library
- Pure virtual class -- no data members
- This is the dependency inversion boundary for GPU rendering

### 3.2 CpuRenderBackend

```
class CpuRenderBackend : public RenderBackend {
public:
    std::vector<Color3> render(
        const Camera& camera,
        const Scene& scene,
        const RenderSettings& settings) override;

    void set_quiet(bool quiet);

private:
    Renderer renderer_;
};
```

**Boundary rules**:
- Located in `src/application/cpu_render_backend.h/.cpp`
- Depends on Ring 3: Renderer, RenderBackend
- Does NOT depend on Ring 4 or any external library
- `render()` delegates to `renderer_.render()` -- zero behavior change

---

## 4. Ring 4: Infrastructure (New Components)

### 4.1 SceneFlattener

```
struct FlatScene {
    std::vector<GPUShape> shapes;
    std::vector<GPUMaterial> materials;
    std::vector<GPULight> lights;
};

class SceneFlattener {
public:
    FlatScene flatten(const Scene& scene) const;

private:
    GPUShape flatten_sphere(const Sphere& s, uint32_t mat_idx) const;
    GPUShape flatten_plane(const Plane& p, uint32_t mat_idx) const;
    GPUShape flatten_box(const Box& b, uint32_t mat_idx) const;
    GPUShape flatten_cylinder(const Cylinder& c, uint32_t mat_idx) const;
    GPUShape flatten_triangle(const Triangle& t, uint32_t mat_idx) const;
    GPUShape flatten_transformed(const TransformedShape& ts, uint32_t mat_idx) const;

    GPUMaterial flatten_lambertian(const Lambertian& m) const;
    GPUMaterial flatten_metal(const Metal& m) const;
    GPUMaterial flatten_dielectric(const Dielectric& m) const;
    GPUMaterial flatten_emissive(const Emissive& m) const;

    GPULight flatten_point_light(const PointLight& l) const;
    GPULight flatten_directional_light(const DirectionalLight& l) const;
};
```

**Boundary rules**:
- Located in `src/infrastructure/gpu/scene_flattener.h/.cpp` (pure C++)
- Depends on Ring 2 (Scene, Shape subclasses, Material subclasses, Light subclasses) and Ring 1 (gpu_types.h)
- Does NOT include any Metal headers
- Uses `dynamic_cast` to identify Shape/Material/Light subtypes during flattening
- `double -> float` narrowing happens here (explicit casts)
- Material deduplication: `std::unordered_map<const Material*, uint32_t>`
- TriangleMesh shapes are skipped with a logged warning (out of scope)

### 4.2 BVHFlattener

```
class BVHFlattener {
public:
    std::vector<LinearBVHNode> build_and_flatten(
        const std::vector<GPUShape>& shapes) const;

private:
    // Internal BVH node (CPU-side, pointer-based)
    struct BVHBuildNode { ... };

    BVHBuildNode* build_recursive(
        std::vector<uint32_t>& shape_indices,
        int start, int end) const;

    int flatten_recursive(
        const BVHBuildNode* node,
        std::vector<LinearBVHNode>& linear_nodes) const;
};
```

**Boundary rules**:
- Located in `src/infrastructure/gpu/bvh_flattener.h/.cpp` (pure C++)
- Depends on Ring 1 (gpu_types.h for GPUShape, LinearBVHNode)
- Does NOT include any Metal headers
- BVH construction uses SAH (Surface Area Heuristic) or simple median split
- Computes AABB from GPUShape params[] (shape-type-specific bounds extraction)
- Fully testable on any platform

### 4.3 MetalRenderBackend

```
class MetalRenderBackend : public RenderBackend {
public:
    MetalRenderBackend();
    ~MetalRenderBackend();

    std::vector<Color3> render(
        const Camera& camera,
        const Scene& scene,
        const RenderSettings& settings) override;

private:
    std::unique_ptr<MetalDevice> device_;
    std::unique_ptr<MetalBufferManager> buffer_mgr_;
    SceneFlattener flattener_;
    BVHFlattener bvh_flattener_;
};
```

**Boundary rules**:
- Located in `src/infrastructure/metal/metal_render_backend.h/.mm`
- Objective-C++ file (.mm) -- the ONLY place where Objective-C syntax and Metal API appear
- Implements Ring 3 RenderBackend interface
- Owns Ring 4 Metal-specific components (MetalDevice, MetalBufferManager)
- Uses Ring 4 pure-C++ components (SceneFlattener, BVHFlattener)

### 4.4 MetalDevice

```
class MetalDevice {
public:
    MetalDevice();
    ~MetalDevice();

    bool is_available() const;
    std::string device_name() const;

    void dispatch_compute(
        uint32_t width, uint32_t height,
        id<MTLBuffer> shapes, id<MTLBuffer> materials,
        id<MTLBuffer> lights, id<MTLBuffer> bvh,
        id<MTLBuffer> camera, id<MTLBuffer> output,
        uint32_t shape_count, uint32_t light_count,
        uint32_t bvh_node_count);

private:
    id<MTLDevice> device_;
    id<MTLCommandQueue> queue_;
    id<MTLComputePipelineState> pipeline_;
    id<MTLLibrary> library_;
};
```

**Boundary rules**:
- Located in `src/infrastructure/metal/metal_device.h/.mm`
- `#import <Metal/Metal.h>` and `#import <Foundation/Foundation.h>` HERE ONLY
- Encapsulates all Metal object lifecycle
- Threadgroup size: 16x16 (256 threads) default, adjusted for non-power-of-2 images

### 4.5 MetalBufferManager

```
class MetalBufferManager {
public:
    MetalBufferManager(id<MTLDevice> device);

    void upload_shapes(const std::vector<GPUShape>& shapes);
    void upload_materials(const std::vector<GPUMaterial>& materials);
    void upload_lights(const std::vector<GPULight>& lights);
    void upload_bvh(const std::vector<LinearBVHNode>& nodes);
    void upload_camera(const GPUCamera& camera);
    void allocate_output(uint32_t width, uint32_t height);

    std::vector<Color3> readback_output(uint32_t width, uint32_t height);

    // Buffer accessors for dispatch
    id<MTLBuffer> shapes_buffer() const;
    id<MTLBuffer> materials_buffer() const;
    id<MTLBuffer> lights_buffer() const;
    id<MTLBuffer> bvh_buffer() const;
    id<MTLBuffer> camera_buffer() const;
    id<MTLBuffer> output_buffer() const;

private:
    id<MTLDevice> device_;
    // MTLBuffer instances
};
```

**Boundary rules**:
- Located in `src/infrastructure/metal/metal_buffer_manager.h/.mm`
- Metal types in interface (id<MTLBuffer>, id<MTLDevice>) -- this is internal to Ring 4
- Handles MTLResourceStorageModeShared for CPU/GPU shared buffers
- readback_output converts float4 (r,g,b,a) -> Color3 (r,g,b as double)

---

## 5. Ring 4: Metal Shader (ray_trace.metal)

The Metal Shading Language file is not a C++ component but is part of Ring 4 (Infrastructure). It runs on the GPU and reads the GPU data structs defined in Ring 1.

**Kernel function**:
```metal
kernel void ray_trace_kernel(
    device const GPUShape* shapes      [[buffer(0)]],
    device const GPUMaterial* materials [[buffer(1)]],
    device const GPULight* lights      [[buffer(2)]],
    device const LinearBVHNode* bvh    [[buffer(3)]],
    device const GPUCamera& camera     [[buffer(4)]],
    device float4* output              [[buffer(5)]],
    constant uint32_t& shape_count     [[buffer(6)]],
    constant uint32_t& light_count     [[buffer(7)]],
    constant uint32_t& bvh_node_count  [[buffer(8)]],
    uint2 gid                          [[thread_position_in_grid]])
```

**Boundary rules**:
- Located in `src/infrastructure/metal/shaders/ray_trace.metal`
- Compiled at build time via CMake custom command -> `.metallib`
- Reads Ring 1 GPU structs (GPUShape, GPUMaterial, etc.) via shared header or duplicated definitions (MSL uses same struct layout)
- No runtime dependency on CPU code beyond buffer contents

---

## 6. Dependency Diagram

```
                    SYSTEM FRAMEWORKS (macOS only)
                    ==============================
                    Metal.framework
                    Foundation.framework
                          |
                          v
    +--------------------------------------------------+
    |           Ring 4: Infrastructure                  |
    |                                                  |
    |  MetalRenderBackend (.mm) ---> [Metal.framework] |
    |  MetalDevice (.mm) ---> [Metal.framework]        |
    |  MetalBufferManager (.mm) ---> [Metal.framework] |
    |       |                                          |
    |       | implements                               |
    |       v                                          |
    |  SceneFlattener (.cpp) -- pure C++, no Metal     |
    |  BVHFlattener (.cpp) -- pure C++, no Metal       |
    |       |                                          |
    |  [EXISTING] YamlSceneLoader, Validator, etc.     |
    +-----|--------------------------------------------+
          | depends on
          v
    +--------------------------------------------------+
    |           Ring 3: Application                     |
    |                                                  |
    |  [GPU] RenderBackend (abstract interface)         |
    |       ^                    ^                     |
    |       |                    |                     |
    |  CpuRenderBackend    MetalRenderBackend (R4)     |
    |       |                                          |
    |       v                                          |
    |  [EXISTING] Renderer                              |
    |  [EXISTING] AnimationRenderer                     |
    +-----|--------------------------------------------+
          | depends on
          v
    +--------------------------------------------------+
    |           Ring 2: Domain                          |
    |                                                  |
    |  [EXISTING] All shapes, materials, lights, etc.  |
    +-----|--------------------------------------------+
          | depends on
          v
    +--------------------------------------------------+
    |           Ring 1: Core / Math                     |
    |                                                  |
    |  [EXISTING] Vec3, Point3, Color3, Ray, AABB, ... |
    |  [GPU] gpu_types.h (GPUShape, GPUMaterial, ...)  |
    +--------------------------------------------------+
```

---

## 7. File Organization (New Files)

```
src/
  core/                                    # Ring 1
    [EXISTING] vec3.h, ray.h, aabb.h/.cpp, math_utils.h, matrix4x4.h, quaternion.h
    [GPU] gpu_types.h                      # Plain C structs for GPU data transfer

  application/                             # Ring 3
    [EXISTING] renderer.h/.cpp, animation_renderer.h/.cpp
    [GPU] render_backend.h                 # Abstract RenderBackend interface
    [GPU] cpu_render_backend.h/.cpp        # Wraps existing Renderer

  infrastructure/                          # Ring 4
    [EXISTING] ppm_writer, yaml_scene_loader, cli_dispatcher, validator, etc.
    gpu/                                   # Pure C++ GPU data components
      [GPU] scene_flattener.h/.cpp         # Scene -> GPU struct arrays
      [GPU] bvh_flattener.h/.cpp           # Build + flatten BVH
    metal/                                 # Objective-C++ Metal components (macOS only)
      [GPU] metal_render_backend.h/.mm     # RenderBackend impl
      [GPU] metal_device.h/.mm             # Metal device/queue/pipeline
      [GPU] metal_buffer_manager.h/.mm     # Buffer allocation and upload
      shaders/
        [GPU] ray_trace.metal              # Compute kernel(s)
        [GPU] gradient.metal               # Walking skeleton shader

tests/
  core/
    [GPU] gpu_types_test.cpp               # Struct size/alignment assertions
  application/
    [GPU] cpu_render_backend_test.cpp      # Identical output to Renderer
  infrastructure/
    gpu/
      [GPU] scene_flattener_test.cpp       # Flatten known scenes
      [GPU] bvh_flattener_test.cpp         # BVH construction + traversal
    metal/
      [GPU] metal_render_backend_test.mm   # macOS-only integration tests
```

---

## 8. Integration Points

| Integration Point | From | To | Data Crossing | Validation |
|---|---|---|---|---|
| **CLI to Backend** | CliDispatcher (R4) | main.cpp backend factory | `command.backend` string | Unknown backend -> error with available list |
| **Backend to Renderer** | CpuRenderBackend (R3) | Renderer (R3) | Camera, Scene, RenderSettings | Produces identical output |
| **Scene to GPU structs** | SceneFlattener (R4) | GPUShape/GPUMaterial/GPULight (R1) | Shape* -> GPUShape via dynamic_cast dispatch | All shape/material types handled or warned |
| **GPU structs to Metal buffers** | MetalBufferManager (R4) | MTLBuffer (Metal framework) | memcpy of contiguous arrays | Buffer size matches array.size() * sizeof(struct) |
| **Metal buffers to shader** | MetalDevice dispatch (R4) | ray_trace.metal kernel | Buffer bindings [0-8] | Struct layout matches between C++ and MSL |
| **Shader output to pixels** | MetalBufferManager readback (R4) | vector<Color3> | float4 -> Color3 (double) | NaN guard, clamp to [0,1] |
| **WriteCallback to backend** | main.cpp (R4) | RenderBackend (R3) | Camera, Scene, RenderSettings | Same interface for CPU and GPU |
| **BVH shapes to linear nodes** | BVHFlattener (R4) | LinearBVHNode[] (R1) | GPUShape[] AABB bounds | Traversal produces identical hits to brute-force |

---

## 9. Boundary Enforcement (CMake Targets)

```cmake
# Ring 1: No external deps
target_link_libraries(nwave_core PRIVATE)  # nothing

# Ring 2: Only Ring 1
target_link_libraries(nwave_domain PUBLIC nwave_core)

# Ring 3: Only Ring 2 (transitively Ring 1)
target_link_libraries(nwave_application PUBLIC nwave_domain)

# Ring 4 (non-Metal): All inner rings + existing external libraries
target_link_libraries(nwave_infrastructure PUBLIC
    nwave_application
    yaml-cpp::yaml-cpp
    Jolt
)

# Ring 4 (Metal, macOS only): Additional Metal framework linking
if(APPLE AND NWAVE_ENABLE_GPU)
    target_link_libraries(nwave_infrastructure PUBLIC
        "-framework Metal"
        "-framework Foundation"
    )
    target_compile_definitions(nwave_infrastructure PUBLIC NWAVE_HAS_METAL=1)
endif()
```

An accidental `#import <Metal/Metal.h>` in any Ring 1-3 source file will produce a compilation error because the Metal include path is only available to Ring 4 `.mm` files.

---

## 10. Conditional Compilation Strategy

```
           macOS + NWAVE_ENABLE_GPU=ON          Linux / Windows / GPU=OFF
           ============================          ========================

Ring 1:    gpu_types.h compiled                  gpu_types.h compiled
           (plain C structs, no Metal)           (plain C structs, no Metal)

Ring 3:    RenderBackend compiled                RenderBackend compiled
           CpuRenderBackend compiled             CpuRenderBackend compiled

Ring 4:    SceneFlattener compiled               SceneFlattener compiled
           BVHFlattener compiled                 BVHFlattener compiled
           metal_render_backend.mm COMPILED      metal/*.mm NOT COMPILED
           metal_device.mm COMPILED              (CMake excludes .mm files)
           metal_buffer_manager.mm COMPILED
           ray_trace.metal COMPILED              .metal NOT COMPILED
           NWAVE_HAS_METAL=1 defined             NWAVE_HAS_METAL not defined

main.cpp:  #ifdef NWAVE_HAS_METAL               #ifdef NWAVE_HAS_METAL
             create MetalRenderBackend             // skipped
           #else                                 #else
             report error                          report error
           #endif                                #endif
```

The SceneFlattener and BVHFlattener compile on all platforms. Their unit tests run in CI on Linux. Only the Metal-specific `.mm` files and `.metal` shaders are macOS-gated.
