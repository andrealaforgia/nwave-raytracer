# nwave-raytracer -- Data Models

**Document ID**: DATA-RAYTRACER-001
**Date**: 2026-02-16
**Status**: Draft

---

## 1. Ring 1: Core / Math Data Structures

These are value types with no external dependencies. All use `double` precision floating point.

### 1.1 Vec3

The fundamental 3D vector type. Also used as Point3 (spatial position) and Color3 (RGB color).

| Field | Type | Description |
|---|---|---|
| `e[0]` | double | x component (or r for color) |
| `e[1]` | double | y component (or g for color) |
| `e[2]` | double | z component (or b for color) |

**Operations**:
- Arithmetic: `+`, `-`, `*` (scalar and component-wise), `/` (scalar and component-wise), unary `-`
- Geometric: `dot(a, b)`, `cross(a, b)`, `length()`, `length_squared()`, `normalize()`
- Utility: `near_zero()` (true if all components are near zero), `operator[]`
- Random generation: `random()`, `random(min, max)`, `random_in_unit_sphere()`, `random_unit_vector()`, `random_in_unit_disk()`

**Type Aliases**:
- `Point3` = Vec3 (semantic: spatial position)
- `Color3` = Vec3 (semantic: RGB color, components in [0, 1])

### 1.2 Ray

A parametric ray: P(t) = origin + t * direction.

| Field | Type | Description |
|---|---|---|
| `origin` | Point3 | Ray starting point in world space |
| `direction` | Vec3 | Ray direction (not necessarily normalized, depending on crafter's choice) |

**Operations**:
- `at(t: double) -> Point3`: returns `origin + t * direction`

### 1.3 AABB (Axis-Aligned Bounding Box)

A box aligned to the coordinate axes, defined by two corner points.

| Field | Type | Description |
|---|---|---|
| `minimum` | Point3 | Corner with smallest x, y, z values |
| `maximum` | Point3 | Corner with largest x, y, z values |

**Operations**:
- `hit(ray, t_min, t_max) -> bool`: Slab method intersection test
- `surrounding_box(box_a, box_b) -> AABB`: Computes the smallest AABB enclosing both inputs
- `longest_axis() -> int`: Returns axis index (0=x, 1=y, 2=z) with the largest extent

### 1.4 Matrix4x4

A 4x4 homogeneous transformation matrix for translate, rotate, scale.

| Field | Type | Description |
|---|---|---|
| `m[4][4]` | double | 16-element matrix in row-major order |

**Operations**:
- Static factories: `identity()`, `translate(dx, dy, dz)`, `rotate_x(angle)`, `rotate_y(angle)`, `rotate_z(angle)`, `scale(sx, sy, sz)`
- `multiply(other: Matrix4x4) -> Matrix4x4`
- `inverse() -> Matrix4x4`
- `transform_point(p: Point3) -> Point3`: Applies full transformation including translation
- `transform_vector(v: Vec3) -> Vec3`: Applies rotation/scale only (ignores translation)
- `transform_normal(n: Vec3) -> Vec3`: Uses inverse-transpose for correct normal transformation

### 1.5 Math Constants and Utilities

| Constant / Function | Type | Value / Description |
|---|---|---|
| `pi` | double | 3.14159265358979323846 |
| `infinity` | double | `std::numeric_limits<double>::infinity()` |
| `epsilon` | double | 0.001 (shadow acne prevention offset) |
| `degrees_to_radians(deg)` | double -> double | `deg * pi / 180.0` |
| `clamp(val, min, max)` | double -> double | Restricts val to [min, max] |
| `random_double()` | -> double | Uniform random in [0, 1) |
| `random_double(min, max)` | -> double | Uniform random in [min, max) |

---

## 2. Ring 2: Domain Data Structures

### 2.1 HitRecord

Stores the result of a ray-shape intersection. Passed by reference and filled in by Shape::hit.

| Field | Type | Description |
|---|---|---|
| `point` | Point3 | World-space intersection point |
| `normal` | Vec3 | Surface normal at intersection (unit length, always facing outward relative to the hit) |
| `t` | double | Ray parameter at intersection (P = origin + t * direction) |
| `u` | double | First texture/barycentric coordinate [0, 1] |
| `v` | double | Second texture/barycentric coordinate [0, 1] |
| `front_face` | bool | True if the ray struck the outside of the surface |
| `material` | const Material* | Pointer to the material at the hit point (non-owning) |

**Helper method**:
- `set_face_normal(ray, outward_normal)`: Computes `front_face` from `dot(ray.direction, outward_normal) < 0` and sets `normal` to always face against the ray direction.

### 2.2 Shape Hierarchy

```
Shape (abstract)
  |-- Sphere
  |-- Plane
  |-- Triangle
  |-- TriangleMesh
  |-- Box
  |-- (future: Cylinder, Cone)
```

**Sphere**

| Field | Type | Description |
|---|---|---|
| `center` | Point3 | Center of the sphere |
| `radius` | double | Radius (negative values invert the normal for hollow glass) |
| `material` | shared_ptr\<Material\> | Material applied to this sphere |

Intersection: Quadratic formula. Discriminant determines hit/miss/tangent. Returns nearest positive t in [t_min, t_max].
Normal: `(hit_point - center) / radius` (sign of radius handles normal inversion).
Bounding box: `center - (|radius|, |radius|, |radius|)` to `center + (|radius|, |radius|, |radius|)`.

**Plane**

| Field | Type | Description |
|---|---|---|
| `point` | Point3 | Any point on the plane |
| `normal` | Vec3 | Plane normal (unit length) |
| `material` | shared_ptr\<Material\> | Material applied to this plane |

Intersection: `t = dot(normal, point - ray.origin) / dot(normal, ray.direction)`. Returns no hit if denominator is near zero (parallel).
Normal: Always the stored normal (with front_face handling).
Bounding box: Infinite plane has no finite bounding box. Returns a very large AABB or is excluded from BVH (tested via brute force separately).

**Triangle**

| Field | Type | Description |
|---|---|---|
| `v0, v1, v2` | Point3 | Three vertex positions |
| `material` | shared_ptr\<Material\> | Material applied to this triangle |

Intersection: Moller-Trumbore algorithm. Returns t, u (barycentric), v (barycentric) if hit.
Normal: `normalize(cross(v1 - v0, v2 - v0))` (face normal) or interpolated from per-vertex normals if available.
Bounding box: AABB enclosing all three vertices with epsilon padding.

**TriangleMesh**

| Field | Type | Description |
|---|---|---|
| `vertices` | vector\<Point3\> | Shared vertex positions |
| `normals` | vector\<Vec3\> | Per-vertex normals (optional; empty if flat shading) |
| `indices` | vector\<int\> | Triangle index list (groups of 3; indices[i*3+0..2] = triangle i) |
| `material` | shared_ptr\<Material\> | Material applied to the mesh |

Each triangle in the mesh is tested individually (using Moller-Trumbore). When per-vertex normals are available, the hit normal is interpolated: `N = (1-u-v)*N0 + u*N1 + v*N2`, then re-normalized.
Bounding box: AABB enclosing all vertices.

**Box (Axis-Aligned)**

| Field | Type | Description |
|---|---|---|
| `box_min` | Point3 | Minimum corner |
| `box_max` | Point3 | Maximum corner |
| `material` | shared_ptr\<Material\> | Material applied to this box |

Intersection: Slab method (same as AABB::hit but also determines which face was struck for normal computation).
Normal: Determined by which face the hit point is closest to (one of six axis-aligned unit vectors).
Bounding box: Itself.

### 2.3 Material Hierarchy

```
Material (abstract)
  |-- Lambertian
  |-- Metal
  |-- Dielectric
  |-- Emissive
```

**Lambertian (Diffuse)**

| Field | Type | Description |
|---|---|---|
| `albedo` | Color3 | Surface color (RGB, each in [0, 1]) |

Scatter: `scattered_direction = hit_normal + random_unit_vector()`. If near_zero, fall back to hit_normal. Attenuation = albedo.

**Metal**

| Field | Type | Description |
|---|---|---|
| `albedo` | Color3 | Reflection tint color |
| `fuzziness` | double | Reflection perturbation (0 = mirror, 1 = very rough), clamped to [0, 1] |

Scatter: `reflected = reflect(ray_in.direction, normal)`, then `scattered = reflected + fuzziness * random_in_unit_sphere()`. If `dot(scattered, normal) <= 0`, the ray is absorbed (return false). Attenuation = albedo.

**Dielectric (Glass)**

| Field | Type | Description |
|---|---|---|
| `index_of_refraction` | double | Refractive index (e.g., 1.5 for glass, 2.42 for diamond) |

Scatter:
1. Determine eta ratio: if front_face, eta = 1.0 / ior; else eta = ior.
2. Compute cos_theta = min(dot(-unit_direction, normal), 1.0).
3. Compute sin_theta = sqrt(1.0 - cos_theta^2).
4. If `eta * sin_theta > 1.0` (total internal reflection) OR `schlick(cos_theta, eta) > random_double()`, reflect.
5. Otherwise, refract using Snell's law.
6. Attenuation = Color3(1, 1, 1) (glass does not absorb in the basic model).

Schlick approximation: `F0 = ((1 - eta) / (1 + eta))^2`; `F = F0 + (1 - F0) * (1 - cos_theta)^5`.

**Emissive**

| Field | Type | Description |
|---|---|---|
| `emit_color` | Color3 | Emission color |
| `intensity` | double | Emission intensity multiplier |

Scatter: Returns false (emissive materials do not scatter).
Emit: Returns `emit_color * intensity`.

### 2.4 Light Hierarchy

```
Light (abstract)
  |-- PointLight
  |-- DirectionalLight
  |-- AreaLight
```

**PointLight**

| Field | Type | Description |
|---|---|---|
| `position` | Point3 | Light position in world space |
| `color` | Color3 | Light color |
| `intensity` | double | Light intensity multiplier |

Illuminate: direction = normalize(position - point); distance = length(position - point); color = color * intensity.
Shadow samples: 1.

**DirectionalLight**

| Field | Type | Description |
|---|---|---|
| `direction` | Vec3 | Light direction (the direction light travels, normalized) |
| `color` | Color3 | Light color |
| `intensity` | double | Light intensity multiplier |

Illuminate: direction = -direction (toward light); distance = infinity; color = color * intensity.
Shadow samples: 1.
No distance attenuation.

**AreaLight**

| Field | Type | Description |
|---|---|---|
| `position` | Point3 | Center of the light surface |
| `u_axis` | Vec3 | Local u-axis of the light surface (half-width direction) |
| `v_axis` | Vec3 | Local v-axis of the light surface (half-height direction) |
| `color` | Color3 | Light color |
| `intensity` | double | Light intensity multiplier |
| `shadow_samples` | int | Number of shadow ray samples (default 16) |

Illuminate: Returns a random point on the light surface as `position + random(-1,1)*u_axis + random(-1,1)*v_axis`. Direction and distance computed from hit point to sample point.
Shadow samples: Returns `shadow_samples`.

### 2.5 Camera

| Field | Type | Description |
|---|---|---|
| `lookfrom` | Point3 | Camera position |
| `lookat` | Point3 | Target point |
| `vup` | Vec3 | World up direction |
| `vfov` | double | Vertical field of view in degrees |
| `aspect_ratio` | double | Image width / height |
| `aperture` | double | Lens diameter (0 = pinhole) |
| `focus_distance` | double | Distance to focal plane |

Derived (computed at construction):
- `u, v, w`: Orthonormal basis vectors
- `viewport_width, viewport_height`: Viewport dimensions at focus_distance
- `pixel_delta_u, pixel_delta_v`: Per-pixel step vectors
- `viewport_upper_left`: World position of the upper-left corner of the viewport
- `lens_radius`: aperture / 2

`generate_ray(s, t)`: s, t in [0, 1] across image plane. Computes pixel center, optionally offsets ray origin for depth of field, returns Ray.

### 2.6 Scene

| Field | Type | Description |
|---|---|---|
| `shapes` | vector\<shared_ptr\<Shape\>\> | All geometry in the scene |
| `lights` | vector\<shared_ptr\<Light\>\> | All lights in the scene |
| `camera` | Camera | The camera for this scene |
| `settings` | RenderSettings | Render configuration |
| `background` | Color3 | Background color for rays that miss all geometry (or a gradient function) |

### 2.7 RenderSettings

| Field | Type | Default | Description |
|---|---|---|---|
| `image_width` | int | 400 | Image width in pixels |
| `image_height` | int | 225 | Image height in pixels (derived from width and aspect ratio) |
| `samples_per_pixel` | int | 10 | Number of samples per pixel for anti-aliasing |
| `max_depth` | int | 10 | Maximum recursion depth for reflection/refraction |
| `output_filename` | string | "output.ppm" | Output file path |
| `sampler_type` | enum | RANDOM | Sampling strategy (RANDOM or STRATIFIED) |
| `gamma` | double | 2.0 | Gamma correction value |
| `tone_mapping` | enum | NONE | Tone mapping (NONE or REINHARD) |

---

## 3. Data Flow Summary

```
YAML File
  --> SceneLoader parses into:
       Scene {
         shapes: [Sphere, Plane, Triangle, ...]
         lights: [PointLight, DirectionalLight, ...]
         camera: Camera { lookfrom, lookat, ... }
         settings: RenderSettings { width, height, spp, ... }
       }

  --> BVH constructed from shapes:
       BVH { root_node: BVHNode { aabb, left, right | primitives } }

  --> Renderer iterates pixels:
       for (x, y):
         for sample s:
           (u, v) = Sampler.generate_offset(s)
           Ray = Camera.generate_ray(u, v)
           Color3 = trace_ray(Ray, Scene/BVH, depth=0)
             --> Shape::hit() -> HitRecord
             --> Material::scatter() -> attenuation, scattered_ray
             --> Light::illuminate() -> direct illumination
             --> recurse with scattered_ray at depth+1
           accumulate color
         average, gamma correct, clamp
         store in pixel_buffer[y * width + x]

  --> ImageWriter writes pixel_buffer to file:
       PPMWriter: "P3\nW H\n255\n" + R G B triplets
       PNGWriter: stbi_write_png(...)
```

---

## 4. Ownership Model

| Object | Owned By | Lifetime |
|---|---|---|
| Scene | main.cpp (stack or unique_ptr) | Entire program run |
| Shapes | Scene (vector of shared_ptr) | Scene lifetime; shared_ptr because BVH holds references |
| Materials | Scene (vector of shared_ptr) | Scene lifetime; shared_ptr because multiple shapes may reference the same material |
| Lights | Scene (vector of shared_ptr) | Scene lifetime |
| Camera | Scene (value member) | Scene lifetime |
| RenderSettings | Scene (value member) | Scene lifetime |
| BVH nodes | BVH root (unique_ptr tree) | Renderer invocation lifetime |
| Pixel buffer | RenderResult (vector by value) | Returned to caller, written to file, then destroyed |
| HitRecord | Stack-allocated | Per-ray lifetime (passed by reference, overwritten on each intersection) |
