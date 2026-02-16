# Class Hierarchy Diagrams

## 1. Shape Hierarchy

```mermaid
classDiagram
    class Shape {
        <<abstract>>
        +hit(ray: Ray, t_min: double, t_max: double, rec: HitRecord) bool*
        +bounding_box() AABB*
    }

    class Sphere {
        -center: Point3
        -radius: double
        -material: shared_ptr~Material~
        +hit(ray, t_min, t_max, rec) bool
        +bounding_box() AABB
    }

    class Plane {
        -point: Point3
        -normal: Vec3
        -material: shared_ptr~Material~
        +hit(ray, t_min, t_max, rec) bool
        +bounding_box() AABB
    }

    class Triangle {
        -v0: Point3
        -v1: Point3
        -v2: Point3
        -material: shared_ptr~Material~
        +hit(ray, t_min, t_max, rec) bool
        +bounding_box() AABB
    }

    class TriangleMesh {
        -vertices: vector~Point3~
        -normals: vector~Vec3~
        -indices: vector~int~
        -material: shared_ptr~Material~
        +hit(ray, t_min, t_max, rec) bool
        +bounding_box() AABB
    }

    class Box {
        -box_min: Point3
        -box_max: Point3
        -material: shared_ptr~Material~
        +hit(ray, t_min, t_max, rec) bool
        +bounding_box() AABB
    }

    class BVHNode {
        -aabb: AABB
        -left: shared_ptr~Shape~
        -right: shared_ptr~Shape~
        +hit(ray, t_min, t_max, rec) bool
        +bounding_box() AABB
    }

    Shape <|-- Sphere
    Shape <|-- Plane
    Shape <|-- Triangle
    Shape <|-- TriangleMesh
    Shape <|-- Box
    Shape <|-- BVHNode

    note for BVHNode "BVH implements Shape interface\n(Composite Pattern).\nDefined in Ring 3 (Application)\nbut implements Ring 2 interface."
```

### Shape Design Notes

- All shapes implement the same two-method interface: `hit` and `bounding_box`
- BVHNode implements Shape (Composite Pattern), so the renderer treats a BVH-accelerated scene identically to a flat shape list
- Plane has an infinite extent, so its bounding_box returns a very large AABB or requires special handling in BVH construction
- Sphere supports negative radius for normal inversion (hollow glass effect)
- TriangleMesh contains multiple triangles internally; each is tested via Moller-Trumbore

---

## 2. Material Hierarchy

```mermaid
classDiagram
    class Material {
        <<abstract>>
        +scatter(ray_in: Ray, rec: HitRecord, attenuation: Color3, scattered: Ray) bool*
        +emit(u: double, v: double, point: Point3) Color3
    }

    class Lambertian {
        -albedo: Color3
        +scatter(ray_in, rec, attenuation, scattered) bool
    }

    class Metal {
        -albedo: Color3
        -fuzziness: double
        +scatter(ray_in, rec, attenuation, scattered) bool
    }

    class Dielectric {
        -index_of_refraction: double
        +scatter(ray_in, rec, attenuation, scattered) bool
        -schlick_reflectance(cosine: double, ref_idx: double) double
    }

    class Emissive {
        -emit_color: Color3
        -intensity: double
        +scatter(ray_in, rec, attenuation, scattered) bool
        +emit(u, v, point) Color3
    }

    Material <|-- Lambertian
    Material <|-- Metal
    Material <|-- Dielectric
    Material <|-- Emissive

    note for Material "emit() has default implementation\nreturning Color3(0,0,0).\nOnly Emissive overrides it."
    note for Lambertian "Strategy Pattern:\nscatter() determines HOW\nthe ray bounces off the surface."
    note for Metal "fuzziness clamped to [0, 1].\n0 = perfect mirror.\n1 = very rough."
    note for Dielectric "Handles both reflection\nand refraction based on\nSchlick/Fresnel + TIR."
```

### Material Design Notes

- Material uses the **Strategy Pattern**: each material defines a different scattering strategy
- `scatter` returns `false` when the ray is absorbed (Emissive always returns false; Metal may return false if perturbed ray goes below surface)
- `emit` has a default no-op implementation; only Emissive overrides it
- Dielectric's `scatter` always returns `true` (either reflects or refracts) with attenuation Color3(1,1,1) (no absorption in the basic model)
- Metal's fuzziness parameter is clamped to [0, 1] at construction time

---

## 3. Light Hierarchy

```mermaid
classDiagram
    class Light {
        <<abstract>>
        +illuminate(point: Point3) IlluminationResult*
        +shadow_sample_count() int
        +sample_point() Point3
    }

    class IlluminationResult {
        +color: Color3
        +direction: Vec3
        +distance: double
    }

    class PointLight {
        -position: Point3
        -color: Color3
        -intensity: double
        +illuminate(point) IlluminationResult
        +shadow_sample_count() int
    }

    class DirectionalLight {
        -direction: Vec3
        -color: Color3
        -intensity: double
        +illuminate(point) IlluminationResult
        +shadow_sample_count() int
    }

    class AreaLight {
        -position: Point3
        -u_axis: Vec3
        -v_axis: Vec3
        -color: Color3
        -intensity: double
        -shadow_samples: int
        +illuminate(point) IlluminationResult
        +shadow_sample_count() int
        +sample_point() Point3
    }

    Light <|-- PointLight
    Light <|-- DirectionalLight
    Light <|-- AreaLight
    Light --> IlluminationResult : returns

    note for PointLight "shadow_sample_count() = 1\nSingle shadow ray.\nHard shadows."
    note for DirectionalLight "shadow_sample_count() = 1\nParallel shadow rays.\ndistance = infinity."
    note for AreaLight "shadow_sample_count() = N (configurable)\nsample_point() returns random\npoint on light surface.\nSoft shadows."
```

### Light Design Notes

- `shadow_sample_count` defaults to 1 (point and directional lights)
- Area lights override to return a configurable sample count (default 16)
- `sample_point` is meaningful only for area lights; it returns a random point on the light surface for each shadow sample
- The renderer's shading loop iterates shadow samples for each light, averaging the occluded/unoccluded results for soft shadow computation
- DirectionalLight sets distance to infinity for shadow rays, ensuring distant occluders are detected

---

## 4. Infrastructure: Writer and Loader

```mermaid
classDiagram
    class ImageWriter {
        <<abstract>>
        +write(filename: string, pixels: vector~Color3~, width: int, height: int) void*
    }

    class PPMWriter {
        -binary_mode: bool
        +write(filename, pixels, width, height) void
    }

    class PNGWriter {
        +write(filename, pixels, width, height) void
    }

    ImageWriter <|-- PPMWriter
    ImageWriter <|-- PNGWriter

    note for PPMWriter "Supports P3 (text) and P6 (binary).\nZero external dependencies."
    note for PNGWriter "Uses stb_image_write.\nSingle-header library."
```

---

## 5. Application: Sampler

```mermaid
classDiagram
    class Sampler {
        <<abstract>>
        +generate_samples(samples_per_pixel: int) vector~SampleOffset~*
    }

    class SampleOffset {
        +u: double
        +v: double
    }

    class RandomSampler {
        +generate_samples(spp) vector~SampleOffset~
    }

    class StratifiedSampler {
        +generate_samples(spp) vector~SampleOffset~
    }

    Sampler <|-- RandomSampler
    Sampler <|-- StratifiedSampler
    Sampler --> SampleOffset : returns

    note for RandomSampler "Uniform random offsets\nwithin [0, 1) x [0, 1).\nSimple but may cluster."
    note for StratifiedSampler "Divides pixel into\nsqrt(spp) x sqrt(spp) strata.\nOne jittered sample per stratum.\nBetter coverage than random."
```

---

## 6. Composite Pattern: BVH as Shape

```mermaid
graph TB
    subgraph "BVH Tree (all nodes implement Shape::hit)"
        root["BVH Root<br/>AABB: entire scene"]
        left["BVH Node<br/>AABB: left half"]
        right["BVH Node<br/>AABB: right half"]
        ll["BVH Leaf<br/>AABB: local<br/>Primitives: [Sphere A, Sphere B]"]
        lr["BVH Leaf<br/>AABB: local<br/>Primitives: [Triangle C]"]
        rl["BVH Leaf<br/>AABB: local<br/>Primitives: [Box D, Sphere E]"]
        rr["BVH Leaf<br/>AABB: local<br/>Primitives: [Plane F]"]
    end

    root --> left
    root --> right
    left --> ll
    left --> lr
    right --> rl
    right --> rr

    style root fill:#E8D44D,stroke:#C9B72C
    style left fill:#E8D44D,stroke:#C9B72C
    style right fill:#E8D44D,stroke:#C9B72C
    style ll fill:#438DD5,stroke:#3C7FC0,color:#fff
    style lr fill:#438DD5,stroke:#3C7FC0,color:#fff
    style rl fill:#438DD5,stroke:#3C7FC0,color:#fff
    style rr fill:#438DD5,stroke:#3C7FC0,color:#fff
```

The Renderer calls `bvh_root.hit(ray, ...)` which recursively:
1. Tests the ray against the node's AABB
2. If miss, returns immediately (skips entire subtree)
3. If hit, recursively tests left and right children
4. Leaf nodes test their primitives directly

This is transparent to the Renderer because BVHNode implements the Shape interface.
