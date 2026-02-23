# Gap Analysis: nwave-raytracer vs "Ray Tracing in One Weekend" (Peter Shirley)

**Date**: 2026-02-22
**Source**: https://raytracing.github.io/books/RayTracingInOneWeekend.html (v4.0.1)
**Codebase**: nwave-raytracer (C++ with Metal GPU backend)

---

## Executive Summary

The nwave-raytracer is **almost perfectly aligned** with "Ray Tracing in One Weekend." Every major concept from the tutorial is implemented, and the codebase significantly exceeds the tutorial in most areas. There is **one meaningful gap**: the camera lacks **defocus blur (depth of field)** support.

---

## Chapter-by-Chapter Comparison

### 1. Output an Image (PPM Format)
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| P3 text PPM format | P6 binary PPM (superior) |
| RGB 0-255 scaling | `255.999 * clamp(value, 0, 1)` |
| Progress to stderr | ProgressReporter class |

- File: `src/infrastructure/ppm_writer.cpp`
- Exceeds tutorial: Binary P6 format is more compact than text P3.

### 2. The vec3 Class
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| `vec3` with e[3] doubles | `Vec3` with x_, y_, z_ doubles |
| `point3`, `color` aliases | `Point3`, `Color3` aliases |
| dot, cross, normalize | `dot()`, `cross()`, `normalize()` |
| `random()`, `random(min,max)` | `random()`, `random(min,max)` |
| `random_in_unit_sphere()` | `random_in_unit_sphere()` |
| `random_unit_vector()` | `random_unit_vector()` |
| `random_in_unit_disk()` | `random_in_unit_disk()` (defined but unused by camera) |
| `near_zero()` | `near_zero()` |
| `reflect()`, `refract()` | `reflect()`, `refract()` |

- File: `src/core/vec3.h`
- Exceeds tutorial: Also has `Matrix4x4`, `Quaternion`, `AABB` types.

### 3. Rays, a Simple Camera, and Background
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| `ray(origin, direction)` | `Ray(origin, direction)` |
| `ray.at(t)` | `Ray::at(t)` |
| Sky gradient (blue-white lerp) | Background gradient (configurable top/bottom colors) |
| Pinhole camera | Pinhole camera (Camera class) |

- Files: `src/core/ray.h`, `src/domain/camera.h`
- Exceeds tutorial: Background colors are configurable per-scene via `RenderSettings`.

### 4-5. Adding a Sphere / Surface Normals and Multiple Objects
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| Sphere with quadratic formula | `Sphere::hit()` with quadratic formula |
| `hit_record` (p, normal, t, front_face) | `HitRecord` (point, normal, t, u, v, front_face, material) |
| `set_face_normal()` | `set_face_normal()` |
| `hittable` abstract class | `Shape` abstract class with virtual `hit()` |
| `hittable_list` (vector) | `Scene` with `vector<shared_ptr<Shape>>` |
| Closest-hit tracking | Linear scan with closest t tracking |

- Files: `src/domain/shapes/sphere.h`, `src/domain/hit_record.h`, `src/domain/scene.h`
- Exceeds tutorial: Also has Plane, Box, Cylinder, Triangle, TriangleMesh, DeformableMesh, TransformedShape.

### 6. Moving Camera Code into its Own Class
**Status**: ALIGNED

- Camera class encapsulates render parameters and ray generation.
- File: `src/domain/camera.h`

### 7. Antialiasing
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| `samples_per_pixel` parameter | `samples_per_pixel` in RenderSettings |
| Random pixel offsets [-0.5, +0.5] | `generate_ray_random()` with `random_double() - 0.5` offsets |
| Average color / SPP | Color averaging in renderer loop |

- Files: `src/application/renderer.cpp`, `src/domain/camera.cpp:45-53`
- Exceeds tutorial: GPU shader also supports configurable SPP with batched progressive rendering.

### 8. Diffuse Materials (Lambertian)
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| Lambertian with random hemisphere scatter | `Lambertian::scatter()` with random hemisphere |
| `0.5 * color` energy absorption | Albedo-based attenuation |
| Gamma correction (`sqrt`) | Gamma 2.0 via `sqrt()` in renderer |
| Max depth recursion limit | `max_depth` in RenderSettings |
| Near-zero direction guard | `near_zero()` check available |

- File: `src/domain/materials/lambertian.h`

### 9. Metal
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| `reflect(v, n)` | `reflect()` in vec3.h |
| `metal(albedo, fuzz)` | `Metal(albedo, fuzziness)` |
| Fuzziness: random perturbation of reflected ray | Fuzz * random_in_unit_sphere added to reflection |
| Fuzz clamped to [0, 1] | Fuzziness parameter |

- File: `src/domain/materials/metal.h`

### 10. Dielectrics (Glass)
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| Snell's Law refraction | `refract()` in vec3.h |
| Total internal reflection check | sin_theta > 1.0 check |
| Schlick approximation | `schlick_reflectance()` private method |
| `dielectric(ior)` | `Dielectric(ior, tint)` |
| Hollow glass sphere (negative radius trick) | Supported via scene configuration |

- File: `src/domain/materials/dielectric.h`
- Exceeds tutorial: Adds optional `tint` color for colored glass.

### 11. Positionable Camera
**Status**: ALIGNED

| Tutorial | Codebase |
|----------|----------|
| `lookfrom`, `lookat`, `vup` | `lookfrom_`, `lookat_`, `vup_` |
| `vfov` (vertical field of view) | `vfov_` (in degrees) |
| Orthonormal basis (u, v, w) | `u_`, `v_`, `w_` computed from lookfrom/lookat/vup |
| Viewport scaled by focus distance | `viewport_height = 2.0 * h * focus_dist` |

- File: `src/domain/camera.cpp:8-35`

### 12. Defocus Blur (Depth of Field)
**Status**: GAP - NOT IMPLEMENTED

This is the **only gap** between the codebase and the tutorial.

**What the tutorial implements**:
- `defocus_angle` parameter: Controls the cone angle of defocus (0 = no blur)
- `focus_dist` parameter: Distance from camera to the plane of perfect focus
- `defocus_disk_u`, `defocus_disk_v` vectors: Define the lens disk orientation
- Ray origin sampling: Instead of all rays originating from `lookfrom` (pinhole), rays originate from random points on a disk centered at `lookfrom`
- Uses `random_in_unit_disk()` to sample the disk

**What the codebase has**:
- `random_in_unit_disk()` is defined in `src/core/vec3.h:159` but **never used by Camera**
- `focus_dist` is computed in `camera.cpp:21` but only used for viewport sizing, NOT for lens simulation
- Both `generate_ray()` and `generate_ray_random()` always originate from `lookfrom_` (pinhole)
- No `defocus_angle` or aperture parameter exists

**Algorithm to implement** (from the tutorial):
```
// In Camera constructor:
defocus_disk_u = focus_dist * tan(defocus_angle/2) * u
defocus_disk_v = focus_dist * tan(defocus_angle/2) * v

// In ray generation:
function defocus_disk_sample():
    p = random_in_unit_disk()
    return lookfrom + (p.x * defocus_disk_u) + (p.y * defocus_disk_v)

function get_ray(px, py):
    pixel_sample = pixel00_loc + ((px + offset_x) * pixel_delta_u)
                                + ((py + offset_y) * pixel_delta_v)
    ray_origin = (defocus_angle <= 0) ? lookfrom : defocus_disk_sample()
    ray_direction = pixel_sample - ray_origin
    return Ray(ray_origin, ray_direction)
```

**Impact on GPU**:
- `GPUCamera` struct needs `defocus_disk_u[3]`, `defocus_disk_v[3]`, `defocus_angle` fields
- Metal shader's ray generation must sample from disk when defocus_angle > 0

**Implementation effort**: Low
**Priority**: Medium - Adds a powerful photographic effect (bokeh, selective focus)

### 13. Final Render (Random Scene)
**Status**: ALIGNED (via YAML scene files)

The tutorial generates a random scene with many spheres programmatically. The codebase uses YAML scene files instead, which is a more flexible approach. The codebase supports all the material types needed for the tutorial's final scene (Lambertian, Metal, Dielectric).

---

## Summary Table

| Feature | Tutorial | Codebase | Status |
|---------|----------|----------|--------|
| PPM output | P3 text | P6 binary | EXCEEDS |
| Vec3 math | Full | Full + Matrix4x4, Quaternion, AABB | EXCEEDS |
| Ray class | Basic | Basic | ALIGNED |
| Ray-Sphere intersection | Quadratic | Quadratic | ALIGNED |
| Hit record | p, normal, t, front_face | + u, v, material pointer | EXCEEDS |
| Scene container | hittable_list | Scene + BVH (GPU) | EXCEEDS |
| Antialiasing | Pixel jitter + SPP | Pixel jitter + SPP + GPU batching | EXCEEDS |
| Lambertian material | Random hemisphere | Random hemisphere | ALIGNED |
| Metal material | Reflect + fuzz | Reflect + fuzz | ALIGNED |
| Dielectric material | Snell + Schlick | Snell + Schlick + tint | EXCEEDS |
| Gamma correction | sqrt (gamma 2) | sqrt (gamma 2) | ALIGNED |
| Positionable camera | lookfrom/lookat/vfov | lookfrom/lookat/vfov | ALIGNED |
| **Defocus blur (DoF)** | **Aperture + focus dist** | **Not implemented** | **GAP** |
| Emissive materials | Not covered | Implemented | EXCEEDS |
| Texture mapping | Not covered | Image + Procedural (20 patterns) | EXCEEDS |
| GPU acceleration | Not covered | Metal compute shaders | EXCEEDS |
| BVH acceleration | Not covered | Linear BVH for GPU | EXCEEDS |
| Physics simulation | Not covered | JoltPhysics integration | EXCEEDS |
| Animation | Not covered | Frame-by-frame with physics | EXCEEDS |
| Additional shapes | Sphere only | Sphere, Plane, Box, Cylinder, Triangle, Mesh | EXCEEDS |
| Additional lights | Not covered | Point + Directional lights | EXCEEDS |

---

## Recommended Change

### Add Defocus Blur (Depth of Field) to Camera

**Files to modify**:
1. `src/domain/camera.h` - Add `defocus_angle`, `focus_dist` parameters; add `defocus_disk_u_`, `defocus_disk_v_` members
2. `src/domain/camera.cpp` - Compute defocus disk vectors; modify `generate_ray_random()` to sample from disk
3. `src/core/gpu_types.h` - Add defocus fields to `GPUCamera` struct
4. `src/infrastructure/metal/shaders/ray_trace.metal` - Sample ray origin from disk in GPU kernel
5. `src/infrastructure/yaml_scene_loader.cpp` - Add `defocus_angle` and `focus_dist` YAML fields
6. `src/infrastructure/gpu/scene_flattener.cpp` - Pass defocus params to GPU camera

**Backward compatible**: When `defocus_angle` is 0 (default), behavior is identical to current pinhole camera.

---

## References

- Shirley, Peter. "Ray Tracing in One Weekend" v4.0.1. https://raytracing.github.io/books/RayTracingInOneWeekend.html
  - Chapter 13: Defocus Blur (the only gap identified)
