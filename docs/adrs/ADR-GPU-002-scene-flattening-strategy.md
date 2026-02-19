# ADR-GPU-002: Scene Flattening Strategy (Tagged Unions)

## Status

Accepted

## Context

The CPU renderer uses virtual dispatch to evaluate ray-shape intersections (`Shape::hit()`) and material scattering (`Material::scatter()`). GPUs cannot execute virtual dispatch -- they need flat, contiguous data arrays where each element encodes its type via an integer tag and a switch statement replaces the vtable lookup.

The CPU `Scene` contains `vector<shared_ptr<Shape>>` where each Shape is a heap-allocated polymorphic object (Sphere, Plane, Box, Cylinder, Triangle, or TransformedShape). Each Shape holds a raw `const Material*` pointer to a heap-allocated Material. These pointers are meaningless to the GPU.

We need a strategy to convert this pointer-based, polymorphic scene graph into flat, GPU-uploadable buffer arrays.

## Decision

**Tagged unions** with separate arrays for shapes, materials, and lights.

**GPUShape**: A fixed-size struct (128 bytes) containing:
- `uint32_t shape_type` (enum: Sphere=0, Plane=1, Box=2, Cylinder=3, Triangle=4)
- `uint32_t material_index` (index into GPUMaterial array)
- `float params[12]` (union data interpreted based on shape_type)
- `float inverse_transform[16]` (4x4 matrix for TransformedShape)

**GPUMaterial**: A fixed-size struct (48 bytes) containing:
- `uint32_t material_type` (enum: Lambertian=0, Metal=1, Dielectric=2, Emissive=3)
- All material parameters as named float fields (no actual C union)

**GPULight**: A fixed-size struct (64 bytes) with `uint32_t light_type` and all light parameters.

**SceneFlattener**: A pure C++ class (no Metal dependencies) that traverses the CPU Scene, uses `dynamic_cast` to identify shape/material/light subtypes, and produces `vector<GPUShape>`, `vector<GPUMaterial>`, `vector<GPULight>`. Material deduplication via `map<const Material*, uint32_t>` ensures shared materials produce one GPUMaterial entry.

**TransformedShape handling**: The inner shape's parameters are extracted and stored in `params[]`. The inverse transformation matrix is stored in `inverse_transform[]`. The GPU shader transforms the ray to local space before intersection, then transforms results back. This matches the CPU TransformedShape::hit() algorithm.

## Alternatives Considered

### Alternative 1: Structure of Arrays (SOA)
Separate arrays per shape type: `sphere_centers[]`, `sphere_radii[]`, `box_mins[]`, `box_maxs[]`, etc. Each array only contains shapes of that type.

**Rejected because**: SOA complicates BVH leaf references. A BVH leaf needs to reference "shapes 5-8" as a contiguous range, but in SOA, sphere 3 and box 2 are in different arrays with different indices. Maintaining cross-array index mappings adds complexity without meaningful performance benefit for a ray tracer (cache coherence matters less when each thread traces a different ray hitting different shapes). SOA excels for uniform workloads (particle systems), not heterogeneous workloads (mixed shape types).

### Alternative 2: Separate compute kernels per shape type
Instead of a tagged union, dispatch separate compute kernels for sphere intersection, plane intersection, etc. Each kernel operates only on its shape type.

**Rejected because**: Requires multiple dispatch passes per bounce, each producing partial results that must be merged. A single kernel with a switch over shape_type is simpler, has one dispatch per frame, and Metal's thread divergence handling is efficient for small switch bodies (5 cases). Multi-kernel adds dispatch overhead and synchronization complexity disproportionate to the 5-way branch cost.

### Alternative 3: GPU-side polymorphism via function pointer tables
Metal supports function pointers (visible function tables) in Metal 3. Use a vtable-like dispatch on GPU.

**Rejected because**: Visible function tables add complexity (function table setup, argument buffer encoding), require Metal 3 (not available on Intel Macs), and introduce indirect call overhead on every intersection. Tagged unions with switch statements are the standard GPU pattern, well-understood, and have predictable performance. The function pointer approach would be premature abstraction for 5 shape types.

## Consequences

- **Positive**: Single array per data type. BVH leaf nodes reference contiguous index ranges. GPU shader uses a simple switch. SceneFlattener is testable without Metal on any platform. Fixed-size structs simplify buffer allocation.
- **Negative**: Fixed-size structs waste space for small shapes (Sphere uses 4 of 12 params). TransformedShape's 64-byte inverse_transform is stored for all shapes (zeroed when unused). Total waste is ~80 bytes per non-transformed sphere out of 128 bytes. Acceptable: 500 shapes * 80 bytes = 40 KB overhead, negligible vs the 127 MB output buffer.
- **Trade-off**: TriangleMesh shapes cannot be represented in the fixed-size GPUShape. They require a separate vertex/index buffer strategy. This is explicitly out of scope for the current feature.
