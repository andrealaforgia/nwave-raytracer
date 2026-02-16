# nwave-raytracer -- Component Boundaries

**Document ID**: COMP-RAYTRACER-001
**Date**: 2026-02-16
**Status**: Draft

---

## 1. Dependency Rule

Dependencies point inward only. The following table defines what each ring can and cannot depend on.

| Ring | Can Depend On | Cannot Depend On |
|---|---|---|
| **Ring 1 (Core/Math)** | C++ standard library only | Domain, Application, Infrastructure, any external library |
| **Ring 2 (Domain)** | Ring 1 (Core/Math) | Application, Infrastructure, any external library |
| **Ring 3 (Application)** | Ring 2 (Domain), Ring 1 (Core/Math) | Infrastructure, any external library |
| **Ring 4 (Infrastructure)** | Ring 3 (Application), Ring 2 (Domain), Ring 1 (Core/Math), external libraries | Nothing forbidden (outermost ring) |

**Enforcement mechanism**: CMake `target_link_libraries` restricts which library targets each ring can access. An `#include` of a header from a forbidden ring will produce a compilation error.

---

## 2. Ring 1: Core / Math

### 2.1 Public Interface

All types in this ring are value types (structs/classes with no virtual methods) designed for by-value passing or const-reference.

**Vec3 / Point3 / Color3**

```
Constructors: default (0,0,0), (x, y, z)
Accessors: x(), y(), z(); operator[] for indexed access
Arithmetic: +, -, * (scalar and component-wise), / (scalar and component-wise), unary -
Functions: dot(a, b), cross(a, b), length(), length_squared(), normalize(), near_zero()
Random: random(), random(min, max), random_in_unit_sphere(), random_unit_vector(), random_in_unit_disk()
Stream: operator<< for debug output
```

Point3 and Color3 are type aliases for Vec3 (semantic distinction only) or distinct types wrapping Vec3 if stricter type safety is desired. The crafter decides.

**Ray**

```
Constructor: (origin: Point3, direction: Vec3)
Accessors: origin(), direction()
Functions: at(t: double) -> Point3  // returns origin + t * direction
```

**AABB**

```
Constructor: (min: Point3, max: Point3)
Accessors: min(), max()
Functions: hit(ray, t_min, t_max) -> bool
Static: surrounding_box(box_a, box_b) -> AABB  // merge two AABBs
```

**Matrix4x4**

```
Constructor: identity(), from 16 doubles
Functions: multiply(Matrix4x4) -> Matrix4x4
           transform_point(Point3) -> Point3
           transform_vector(Vec3) -> Vec3
           transform_normal(Vec3) -> Vec3  // uses inverse-transpose
           inverse() -> Matrix4x4
Static: translate(dx, dy, dz), rotate_x/y/z(angle), scale(sx, sy, sz)
```

**MathUtils**

```
Constants: pi, infinity, epsilon (0.001)
Functions: degrees_to_radians(degrees) -> double
           clamp(value, min, max) -> double
           random_double() -> double  // [0, 1)
           random_double(min, max) -> double  // [min, max)
```

### 2.2 Boundary Rules

- No `#include` of any header outside `src/core/`
- No heap allocation (all types are stack-friendly value types)
- No I/O operations (no `std::cout`, no file access)
- No virtual methods (these are concrete value types, not polymorphic)

---

## 3. Ring 2: Domain

### 3.1 Public Interfaces

**Shape (abstract base)**

```
virtual bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const = 0;
virtual AABB bounding_box() const = 0;
virtual ~Shape() = default;
```

All concrete shapes (Sphere, Plane, Triangle, TriangleMesh, Box) implement this interface. The BVH in Ring 3 also implements this interface.

**Material (abstract base)**

```
virtual bool scatter(const Ray& ray_in, const HitRecord& rec,
                     Color3& attenuation, Ray& scattered) const = 0;
virtual Color3 emit(double u, double v, const Point3& point) const;
    // Default implementation returns Color3(0,0,0) -- non-emissive.
virtual ~Material() = default;
```

Concrete materials: Lambertian, Metal, Dielectric, Emissive.

**Light (abstract base)**

```
struct IlluminationResult {
    Color3 color;         // light color * intensity
    Vec3 direction;       // direction FROM hit point TOWARD light
    double distance;      // distance to light (infinity for directional)
};

virtual IlluminationResult illuminate(const Point3& point) const = 0;
virtual int shadow_sample_count() const;  // default 1; area lights return > 1
virtual Point3 sample_point() const;      // for area lights: random point on surface
virtual ~Light() = default;
```

Concrete lights: PointLight, DirectionalLight, AreaLight.

**Camera**

```
Constructor: (lookfrom, lookat, vup, vfov, aspect_ratio, aperture, focus_distance)
Functions: generate_ray(u: double, v: double) -> Ray
    // u, v in [0, 1] representing position on the image plane
    // When aperture > 0, ray origin is randomly offset on the lens disc
```

**HitRecord (struct, not abstract)**

```
Point3 point;            // world-space intersection point
Vec3 normal;             // surface normal (always outward-facing)
double t;                // ray parameter at intersection
double u, v;             // texture coordinates / barycentric coords
bool front_face;         // true if ray hit the outside of the surface
const Material* material;  // pointer to the material at the hit point

void set_face_normal(const Ray& ray, const Vec3& outward_normal);
    // Sets normal and front_face based on ray direction vs outward_normal
```

**Scene**

```
Constructor: (shapes, lights, camera, settings)
Functions: hit(ray, t_min, t_max, rec) -> bool  // delegates to shape list
Accessors: lights(), camera(), settings(), shapes()
```

**RenderSettings (struct)**

```
int image_width;
int image_height;
int samples_per_pixel;
int max_depth;
std::string output_filename;
enum SamplerType { RANDOM, STRATIFIED };
SamplerType sampler_type;
```

### 3.2 Boundary Rules

- Can `#include` headers from `src/core/` only
- Cannot `#include` any header from `src/application/` or `src/infrastructure/`
- Cannot use yaml-cpp, stb_image_write, or any external library
- Cannot perform I/O (no file reads/writes, no stdout)
- Concrete shapes, materials, and lights are defined in this ring (not in infrastructure)

### 3.3 Extension Points

Adding a new shape: Create a new class in `src/domain/shapes/` that inherits from Shape and implements `hit()` and `bounding_box()`. No other changes required. The BVH and renderer work against the Shape interface.

Adding a new material: Create a new class in `src/domain/materials/` that inherits from Material and implements `scatter()` and optionally `emit()`. No other changes required. Hit records carry a Material pointer; the renderer calls scatter generically.

Adding a new light: Create a new class in `src/domain/lights/` that inherits from Light and implements `illuminate()`. The shading loop in the renderer iterates all lights generically.

---

## 4. Ring 3: Application

### 4.1 Public Interfaces

**Renderer**

```
struct RenderResult {
    std::vector<Color3> pixels;  // row-major pixel buffer
    int width;
    int height;
    double elapsed_seconds;
    size_t total_rays;
};

RenderResult render(const Scene& scene, const RenderSettings& settings,
                    std::function<void(int current_row, int total_rows)> progress_callback);
```

The Renderer returns a pixel buffer (vector of Color3). It does NOT write files. File writing is the responsibility of Infrastructure (ImageWriter). The progress_callback enables the CLI to display a progress bar without the Renderer knowing about terminal I/O.

**trace_ray (internal to Renderer, not a public interface)**

```
Color3 trace_ray(const Ray& ray, const Scene& scene, int depth);
```

This is the recursive core of the renderer. It is not exposed outside the Application ring.

**BVH**

```
Constructor: (shapes: std::vector<std::shared_ptr<Shape>>, start, end)
Implements Shape interface:
    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;
    AABB bounding_box() const override;
```

BVH implements Shape so the renderer can use it transparently. The Scene can hold either a flat list of shapes (brute force) or a BVH root node.

**Sampler (abstract)**

```
struct SampleOffset { double u, v; };  // offset within [0, 1) for the pixel

virtual std::vector<SampleOffset> generate_samples(int samples_per_pixel) const = 0;
virtual ~Sampler() = default;
```

Concrete samplers: RandomSampler, StratifiedSampler.

### 4.2 Boundary Rules

- Can `#include` headers from `src/domain/` and `src/core/`
- Cannot `#include` any header from `src/infrastructure/`
- Cannot use yaml-cpp, stb_image_write, or any external library
- Cannot perform file I/O
- The Renderer communicates progress via callback, not by writing to stdout directly

### 4.3 Key Design Constraint: No I/O

The Application ring produces a pixel buffer and render statistics. It does not know:
- Where the Scene came from (YAML file? Hardcoded? Network?)
- Where the pixels go (PPM? PNG? Network? Display?)
- What the CLI looks like (progress bar format? Verbosity level?)

This separation enables:
- Testing the renderer with programmatic scenes and pixel buffer inspection
- Embedding the renderer in other applications (GUI, web service) without CLI dependencies
- Swapping input/output formats without touching rendering logic

---

## 5. Ring 4: Infrastructure

### 5.1 Public Interfaces

**CLI**

```
struct CLIConfig {
    std::string command;          // "render" or "validate"
    std::string scene_file;       // path to YAML file
    std::optional<int> samples_override;
    std::optional<int> width_override;
    std::optional<int> height_override;
    std::optional<int> depth_override;
    std::optional<std::string> output_override;
};

CLIConfig parse_arguments(int argc, char* argv[]);
void print_usage();
```

**SceneLoader**

```
struct LoadResult {
    Scene scene;
    std::vector<std::string> warnings;
};

LoadResult load_scene(const std::string& yaml_path);
    // Throws or returns error on parse failure or invalid structure.
    // Resolves material references by name.
    // Applies CLI overrides to RenderSettings.
```

**Validator**

```
struct ValidationResult {
    bool valid;
    struct Check {
        std::string category;  // "Camera", "Materials", "Objects", "Lights", "References"
        bool passed;
        std::vector<std::string> errors;
    };
    std::vector<Check> checks;
};

ValidationResult validate(const Scene& scene);
```

**ImageWriter (abstract)**

```
virtual void write(const std::string& filename,
                   const std::vector<Color3>& pixels,
                   int width, int height) const = 0;
virtual ~ImageWriter() = default;
```

Concrete writers: PPMWriter (P3 text and P6 binary), PNGWriter.

**ProgressReporter**

```
void report_progress(int current_row, int total_rows, double elapsed_seconds);
    // Displays: [=================>    ] 58%  row 348/600  elapsed 12.4s  eta 8.9s
```

### 5.2 Boundary Rules

- Can `#include` any inner ring header
- Can use external libraries (yaml-cpp, stb_image_write)
- This is the only ring that performs file I/O and terminal I/O
- This ring "wires" everything together: main.cpp lives here and constructs the pipeline

### 5.3 main.cpp Wiring

main.cpp (in `src/main.cpp`, part of Ring 4) is the composition root:

1. Parse CLI arguments
2. If command is "validate": load scene, validate, print report, exit
3. If command is "render":
   a. Load scene from YAML (SceneLoader)
   b. Apply CLI overrides to RenderSettings
   c. Validate scene (Validator); exit on failure
   d. Select ImageWriter based on output file extension
   e. Create Renderer
   f. Call render(scene, settings, progress_callback)
   g. Write pixel buffer to file via ImageWriter
   h. Print render statistics

---

## 6. Integration Points

These are the boundaries where data crosses rings. Each is a potential failure point and should be integration-tested.

| Integration Point | From | To | Data Crossing |
|---|---|---|---|
| **Scene loading** | YAML file (external) | Infrastructure -> Domain | SceneLoader constructs Shape, Material, Light, Camera, Scene objects from parsed YAML |
| **Validation** | Domain (Scene) | Infrastructure (Validator) | Validator inspects Scene for integrity; returns ValidationResult |
| **Render invocation** | Infrastructure (CLI) | Application (Renderer) | CLI passes Scene + RenderSettings to Renderer; receives RenderResult (pixel buffer) |
| **Progress reporting** | Application (Renderer) | Infrastructure (ProgressReporter) | Renderer calls progress callback with (current_row, total_rows); Infrastructure formats terminal output |
| **Image writing** | Application (RenderResult) | Infrastructure (ImageWriter) | Pixel buffer (vector of Color3) passed to writer with dimensions |
| **BVH wrapping** | Domain (Shape list) | Application (BVH) | BVH takes ownership of Shape pointers and presents them as a single Shape interface |
| **Material dispatch** | Application (trace_ray) | Domain (Material::scatter) | trace_ray calls scatter on the material referenced in HitRecord |

---

## 7. File Organization

```
src/
  main.cpp                           # Ring 4: composition root
  core/                              # Ring 1
    vec3.h
    ray.h
    color.h
    matrix4.h
    aabb.h
    math_utils.h
  domain/                            # Ring 2
    hit_record.h
    scene.h / scene.cpp
    render_settings.h
    shapes/
      shape.h                        # Abstract base
      sphere.h / sphere.cpp
      plane.h / plane.cpp
      triangle.h / triangle.cpp
      triangle_mesh.h / triangle_mesh.cpp
      box.h / box.cpp
    materials/
      material.h                     # Abstract base
      lambertian.h / lambertian.cpp
      metal.h / metal.cpp
      dielectric.h / dielectric.cpp
      emissive.h / emissive.cpp
    lights/
      light.h                        # Abstract base
      point_light.h / point_light.cpp
      directional_light.h / directional_light.cpp
      area_light.h / area_light.cpp
    camera.h / camera.cpp
  application/                       # Ring 3
    renderer.h / renderer.cpp
    bvh.h / bvh.cpp
    sampler.h                        # Abstract base
    random_sampler.h / random_sampler.cpp
    stratified_sampler.h / stratified_sampler.cpp
  infrastructure/                    # Ring 4
    cli.h / cli.cpp
    scene_loader.h / scene_loader.cpp
    validator.h / validator.cpp
    image_writer.h                   # Abstract base
    ppm_writer.h / ppm_writer.cpp
    png_writer.h / png_writer.cpp
    progress_reporter.h / progress_reporter.cpp

tests/
  core/
    vec3_test.cpp
    ray_test.cpp
    aabb_test.cpp
    matrix4_test.cpp
  domain/
    sphere_test.cpp
    plane_test.cpp
    triangle_test.cpp
    triangle_mesh_test.cpp
    box_test.cpp
    lambertian_test.cpp
    metal_test.cpp
    dielectric_test.cpp
    camera_test.cpp
  application/
    renderer_test.cpp
    bvh_test.cpp
    sampler_test.cpp
  infrastructure/
    scene_loader_test.cpp
    validator_test.cpp
    ppm_writer_test.cpp

scenes/
  cornell_box.yaml
  three_spheres.yaml
  random_spheres.yaml

third_party/
  stb/
    stb_image_write.h
```

---

## 8. Dependency Violation Detection

To catch accidental dependency violations during development:

1. **CMake target separation**: Each ring is a separate CMake library target. A `#include` across ring boundaries without the proper `target_link_libraries` will fail at compile time.

2. **Include path discipline**: Each ring's CMake target exposes only its own headers via `target_include_directories(... PUBLIC ...)`. Inner ring headers are available only through the link chain.

3. **Code review checklist item**: Every PR should verify that no new cross-ring includes were added in the wrong direction.
