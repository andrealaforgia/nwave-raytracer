# Data Models: GPU Compute Rendering

**Document ID**: DATA-GPU-001
**Feature**: gpu-compute-rendering
**Date**: 2026-02-18
**Status**: Draft

---

## 1. Overview

This document defines the GPU buffer layouts, tagged union structs, and linear BVH node format. These data structures form the contract between the CPU-side SceneFlattener/BVHFlattener and the GPU-side Metal compute shader.

All structs use `float` (32-bit) for numeric values and `uint32_t` for enums and indices. All structs are 16-byte aligned for Metal buffer compatibility. The structs are defined in `src/core/gpu_types.h` as plain C compatible types with no Metal dependencies.

---

## 2. CPU-to-GPU Type Mapping

### 2.1 Domain Objects to GPU Structs

| CPU Type (Ring 2) | GPU Struct (Ring 1) | Key Transformation |
|---|---|---|
| `Sphere` (center: Point3, radius: double) | `GPUShape` (type=SPHERE, params: float[12]) | double->float narrowing; center in params[0-2], radius in params[3] |
| `Plane` (point: Point3, normal: Vec3) | `GPUShape` (type=PLANE, params: float[12]) | point in params[0-2], normal in params[3-5] |
| `Box` (box_min: Point3, box_max: Point3) | `GPUShape` (type=BOX, params: float[12]) | box_min in params[0-2], box_max in params[3-5] |
| `Cylinder` (center: Point3, radius: double, height: double) | `GPUShape` (type=CYLINDER, params: float[12]) | center in params[0-2], radius in params[3], height in params[4] |
| `Triangle` (v0, v1, v2: Point3) | `GPUShape` (type=TRIANGLE, params: float[12]) | v0 in params[0-2], v1 in params[3-5], v2 in params[6-8] |
| `TransformedShape` (inner + Matrix4x4) | `GPUShape` (type=inner_type, has_transform=1) | Inner shape params + inverse_transform[16] populated |
| `Lambertian` (albedo: Color3) | `GPUMaterial` (type=LAMBERTIAN) | albedo[3] |
| `Metal` (albedo: Color3, fuzziness: double) | `GPUMaterial` (type=METAL) | albedo[3], fuzziness |
| `Dielectric` (ior: double, tint: Color3) | `GPUMaterial` (type=DIELECTRIC) | ior, albedo[3] = tint |
| `Emissive` (color: Color3, intensity: double) | `GPUMaterial` (type=EMISSIVE) | emission_color[3], emission_intensity |
| `PointLight` (pos, color, intensity) | `GPULight` (type=POINT) | position[3], color[3], intensity |
| `DirectionalLight` (dir, color, intensity) | `GPULight` (type=DIRECTIONAL) | direction[3], color[3], intensity |
| `Camera` (all members) | `GPUCamera` | Precomputed pixel00, delta_u, delta_v (same math, float precision) |

### 2.2 Pointer-to-Index Translation

| CPU Concept | GPU Concept | Mechanism |
|---|---|---|
| `const Material*` (raw pointer on HitRecord) | `uint32_t material_index` | SceneFlattener builds `map<const Material*, uint32_t>` during traversal |
| `shared_ptr<Shape>` (Scene.shapes_) | Array index into GPUShape[] | Shapes flattened in scene order; BVH leaf nodes reference these indices |
| `BVHNode* left, right` (pointer tree) | `uint32_t offset` (array index) | BVHFlattener converts pointer tree to contiguous array; first child = node+1 (implicit), second child = offset field |

---

## 3. GPUShape Params Layout

The `params[12]` array is interpreted differently based on `shape_type`:

### 3.1 Sphere (type = 0)

| Index | Field | Description |
|---|---|---|
| params[0] | center.x | Sphere center X |
| params[1] | center.y | Sphere center Y |
| params[2] | center.z | Sphere center Z |
| params[3] | radius | Sphere radius |
| params[4-11] | unused | Zero-initialized |

### 3.2 Plane (type = 1)

| Index | Field | Description |
|---|---|---|
| params[0] | point.x | Point on plane X |
| params[1] | point.y | Point on plane Y |
| params[2] | point.z | Point on plane Z |
| params[3] | normal.x | Plane normal X |
| params[4] | normal.y | Plane normal Y |
| params[5] | normal.z | Plane normal Z |
| params[6-11] | unused | Zero-initialized |

### 3.3 Box (type = 2)

| Index | Field | Description |
|---|---|---|
| params[0] | box_min.x | Box minimum corner X |
| params[1] | box_min.y | Box minimum corner Y |
| params[2] | box_min.z | Box minimum corner Z |
| params[3] | box_max.x | Box maximum corner X |
| params[4] | box_max.y | Box maximum corner Y |
| params[5] | box_max.z | Box maximum corner Z |
| params[6-11] | unused | Zero-initialized |

### 3.4 Cylinder (type = 3)

| Index | Field | Description |
|---|---|---|
| params[0] | center.x | Base center X |
| params[1] | center.y | Base center Y |
| params[2] | center.z | Base center Z |
| params[3] | radius | Cylinder radius |
| params[4] | height | Cylinder height |
| params[5-11] | unused | Zero-initialized |

### 3.5 Triangle (type = 4)

| Index | Field | Description |
|---|---|---|
| params[0] | v0.x | Vertex 0 X |
| params[1] | v0.y | Vertex 0 Y |
| params[2] | v0.z | Vertex 0 Z |
| params[3] | v1.x | Vertex 1 X |
| params[4] | v1.y | Vertex 1 Y |
| params[5] | v1.z | Vertex 1 Z |
| params[6] | v2.x | Vertex 2 X |
| params[7] | v2.y | Vertex 2 Y |
| params[8] | v2.z | Vertex 2 Z |
| params[9-11] | unused | Zero-initialized |

---

## 4. Inverse Transform Matrix Layout

The `inverse_transform[16]` array stores a 4x4 matrix in **column-major** order (matching Metal's `float4x4` convention):

```
Column 0: [0]  [1]  [2]  [3]
Column 1: [4]  [5]  [6]  [7]
Column 2: [8]  [9]  [10] [11]
Column 3: [12] [13] [14] [15]
```

When `has_transform == 0`, the GPU shader skips the transform and uses the ray directly. When `has_transform == 1`, the shader transforms the ray to local space before intersection:

```
local_origin = inverse_transform * world_origin
local_direction = inverse_transform * world_direction  (as vector, w=0)
```

After intersection, the hit point is transformed back to world space and the normal is transformed via the transpose of the upper-left 3x3 of the inverse matrix (which equals the cofactor matrix for normal transformation).

---

## 5. Linear BVH Node Format

### 5.1 Node Layout

```
struct LinearBVHNode {         // 32 bytes total
    float aabb_min[3];         // 12 bytes: AABB minimum corner
    uint32_t offset;           //  4 bytes: interior: second child; leaf: first shape index
    float aabb_max[3];         // 12 bytes: AABB maximum corner
    uint32_t count;            //  4 bytes: interior: 0; leaf: shape count (>0)
};
```

### 5.2 Node Type Determination

- `count == 0` -> **Interior node**: has two children. First child is at `node_index + 1` (implicit). Second child is at `offset`.
- `count > 0` -> **Leaf node**: contains `count` shapes starting at index `offset` in the GPUShape array.

### 5.3 Tree Layout in Linear Array

The BVH is stored in **depth-first, left-child-first** order:

```
Index:  0     1     2     3     4     5     6     7
Node:  [root] [L]   [LL]  [LR]  [R]   [RL]  [RR]  ...
                          ^           ^
                          |           |
                     root.offset=4  L.offset=3
```

- Root (index 0): first child is at index 1 (implicit); second child at `root.offset`
- Interior node at index i: first child at i+1; second child at `nodes[i].offset`
- The implicit first-child-at-i+1 eliminates one pointer per node (50% storage savings vs storing both children)

### 5.4 GPU Traversal Algorithm

```
stack[64];       // fixed-size stack (supports 2^64 node trees)
stack_ptr = 0;
stack[stack_ptr++] = 0;  // push root

while (stack_ptr > 0) {
    node_idx = stack[--stack_ptr];  // pop
    node = bvh[node_idx];

    if (!intersect_aabb(ray, node.aabb_min, node.aabb_max, closest_t))
        continue;  // ray misses this node's bounding box

    if (node.count > 0) {
        // LEAF: test all shapes in this node
        for (i = node.offset; i < node.offset + node.count; i++)
            test_shape_intersection(ray, shapes[i], closest_t, hit_record);
    } else {
        // INTERIOR: push both children (order matters for early termination)
        stack[stack_ptr++] = node.offset;     // second child (far)
        stack[stack_ptr++] = node_idx + 1;    // first child (near, popped first)
    }
}
```

### 5.5 AABB Computation per Shape Type

The BVHFlattener computes AABBs from GPUShape params:

| Shape Type | AABB Min | AABB Max |
|---|---|---|
| Sphere | center - radius | center + radius |
| Plane | Large extent (effectively infinite, clamped to scene bounds) | Large extent |
| Box | box_min | box_max |
| Cylinder | (center.x-r, center.y, center.z-r) | (center.x+r, center.y+h, center.z+r) |
| Triangle | min(v0, v1, v2) per axis | max(v0, v1, v2) per axis |

For TransformedShape (has_transform == 1): the AABB is computed from the **world-space** bounds of the transformed shape. This requires transforming all 8 corners of the local AABB through the (forward) transform and taking the axis-aligned bounds of the result.

---

## 6. GPU Camera Buffer Layout

The GPUCamera struct packs all precomputed camera parameters needed for ray generation:

| Offset | Field | Size | Description |
|---|---|---|---|
| 0 | lookfrom[3] | 12B | Camera position (float3) |
| 12 | _pad0 | 4B | Alignment padding |
| 16 | pixel00_loc[3] | 12B | World position of pixel (0,0) center |
| 28 | _pad1 | 4B | Alignment padding |
| 32 | pixel_delta_u[3] | 12B | World-space offset per pixel in U direction |
| 44 | _pad2 | 4B | Alignment padding |
| 48 | pixel_delta_v[3] | 12B | World-space offset per pixel in V direction |
| 60 | _pad3 | 4B | Alignment padding |
| 64 | background_top[3] | 12B | Sky gradient top color |
| 76 | _pad4 | 4B | Alignment padding |
| 80 | background_bottom[3] | 12B | Sky gradient bottom color |
| 92 | _pad5 | 4B | Alignment padding |
| 96 | image_width | 4B | Image width in pixels |
| 100 | image_height | 4B | Image height in pixels |
| 104 | samples_per_pixel | 4B | SPP count |
| 108 | max_depth | 4B | Maximum ray bounce depth |
| **Total** | | **112B** | 16-byte aligned |

**Ray generation formula** (GPU, per pixel):
```
pixel_pos = pixel00_loc + px * pixel_delta_u + py * pixel_delta_v
            + jitter_u * pixel_delta_u + jitter_v * pixel_delta_v  // SPP jitter
ray_direction = pixel_pos - lookfrom
ray = Ray(lookfrom, ray_direction)
```

This matches the CPU Camera::generate_ray() and Camera::generate_ray_random() math exactly, narrowed to float precision.

---

## 7. Output Buffer Layout

The output buffer is a flat array of `float4` (RGBA), one per pixel:

```
output[y * image_width + x] = float4(r, g, b, 1.0)
```

- R, G, B are gamma-corrected values in [0.0, 1.0]
- Alpha is always 1.0 (unused, present for Metal alignment)
- Gamma correction: `output = sqrt(clamp(linear_color, 0.0, 1.0))` per channel
- NaN guard: if any channel is NaN, replace with 0.0
- Buffer size: `width * height * sizeof(float4)` = `width * height * 16` bytes

Readback converts `float4` -> `Color3`:
```
Color3 color(
    static_cast<double>(output[i].x),
    static_cast<double>(output[i].y),
    static_cast<double>(output[i].z)
);
```

The Color3 values are already gamma-corrected and in [0.0, 1.0] range, ready for PPMWriter.

---

## 8. Buffer Size Estimates

For Sofia's production scene (500 spheres, 4 materials, 1 light, 3840x2160):

| Buffer | Struct Size | Count | Total Size |
|---|---|---|---|
| GPUShape[] | 128 B | 500 | 64 KB |
| GPUMaterial[] | 48 B | 4 | 192 B |
| GPULight[] | 64 B | 1 | 64 B |
| LinearBVHNode[] | 32 B | ~999 (2N-1) | ~32 KB |
| GPUCamera | 112 B | 1 | 112 B |
| Output float4[] | 16 B | 8,294,400 | ~127 MB |
| **Total** | | | **~127 MB** |

The output buffer dominates. For 4K rendering, 127 MB is well within the 32 GB unified memory of M2 Max (0.4%). Even on 8 GB M1 machines, this is under 2%.

---

## 9. Struct Alignment Verification

Each struct must include a static assertion to catch layout mismatches between C++ and MSL:

```cpp
static_assert(sizeof(GPUShape) == 128, "GPUShape must be 128 bytes for Metal buffer compatibility");
static_assert(sizeof(GPUMaterial) == 48, "GPUMaterial must be 48 bytes");
static_assert(sizeof(GPULight) == 64, "GPULight must be 64 bytes");
static_assert(sizeof(GPUCamera) == 112, "GPUCamera must be 112 bytes");
static_assert(sizeof(LinearBVHNode) == 32, "LinearBVHNode must be 32 bytes");
static_assert(alignof(GPUShape) >= 16, "GPUShape must be 16-byte aligned");
static_assert(alignof(GPUMaterial) >= 16, "GPUMaterial must be 16-byte aligned");
static_assert(alignof(GPULight) >= 16, "GPULight must be 16-byte aligned");
static_assert(alignof(GPUCamera) >= 16, "GPUCamera must be 16-byte aligned");
static_assert(alignof(LinearBVHNode) >= 16, "LinearBVHNode must be 16-byte aligned");
```

These assertions compile on every platform, catching size/alignment regressions before they reach the GPU.
