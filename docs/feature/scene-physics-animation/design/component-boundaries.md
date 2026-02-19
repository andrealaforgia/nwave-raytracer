# Component Boundaries: Scene Physics Animation

**Document ID**: COMP-SPA-001
**Feature**: scene-physics-animation
**Date**: 2026-02-17
**Status**: Draft
**Extends**: COMP-RAYTRACER-001 (existing component boundaries)

---

## 1. Dependency Rule (Unchanged)

Dependencies point inward only. This table is identical to COMP-RAYTRACER-001 and applies to all new components.

| Ring | Can Depend On | Cannot Depend On |
|---|---|---|
| **Ring 1 (Core/Math)** | C++ standard library only | Domain, Application, Infrastructure, any external library |
| **Ring 2 (Domain)** | Ring 1 (Core/Math) | Application, Infrastructure, any external library |
| **Ring 3 (Application)** | Ring 2 (Domain), Ring 1 (Core/Math) | Infrastructure, any external library |
| **Ring 4 (Infrastructure)** | All inner rings, external libraries | Nothing forbidden |

---

## 2. Ring 1: Core / Math (New Components)

### 2.1 Matrix4x4

```
class Matrix4x4 {
    // Storage
    double m[4][4];

    // Construction
    static Matrix4x4 identity();
    static Matrix4x4 translation(double dx, double dy, double dz);
    static Matrix4x4 from_translation_rotation(Vec3 translation, Quaternion rotation);

    // Operations
    Matrix4x4 operator*(const Matrix4x4& other) const;
    Matrix4x4 inverse() const;
    Matrix4x4 transpose() const;

    // Transform operations
    Point3 transform_point(const Point3& p) const;
    Vec3 transform_vector(const Vec3& v) const;
    Vec3 transform_normal(const Vec3& n) const;  // uses inverse-transpose
};
```

**Boundary rules**:
- No `#include` outside `src/core/`
- Pure value type, no heap allocation
- No I/O operations
- No virtual methods

### 2.2 Quaternion

```
class Quaternion {
    double x, y, z, w;

    // Construction
    static Quaternion identity();
    static Quaternion from_axis_angle(Vec3 axis, double angle_radians);

    // Operations
    Quaternion operator*(const Quaternion& other) const;
    Quaternion conjugate() const;
    Quaternion normalized() const;

    // Conversion
    Matrix4x4 to_matrix() const;

    // Interpolation
    static Quaternion slerp(const Quaternion& a, const Quaternion& b, double t);
};
```

**Boundary rules**: Same as Matrix4x4. Pure math, no dependencies beyond C++ standard library.

---

## 3. Ring 2: Domain (New Components)

### 3.1 PhysicsProperties

```
enum class BodyType { STATIC, DYNAMIC, KINEMATIC };

struct PhysicsProperties {
    BodyType body_type = BodyType::STATIC;
    double mass = 1.0;
    Vec3 initial_velocity{0, 0, 0};
    double friction = 0.5;
    double restitution = 0.3;
};
```

**Boundary rules**:
- Depends only on Ring 1 (Vec3)
- Pure data struct, no behavior
- No physics engine types

### 3.2 AnimationConfig

```
struct AnimationConfig {
    double duration = 5.0;
    double physics_timestep = 1.0 / 60.0;
    int render_fps = 30;
    std::string output_directory = "frames/";

    // Derived
    int total_frames() const;
    int steps_per_frame() const;
    double render_dt() const;
};
```

**Boundary rules**: Same as PhysicsProperties. Pure data with derived computations.

### 3.3 TransformedShape

```
class TransformedShape : public Shape {
    // Construction
    TransformedShape(std::shared_ptr<Shape> inner, Matrix4x4 transform);

    // Shape interface
    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;

    // Transform update (called per physics frame)
    void set_transform(const Matrix4x4& transform);

    // Access
    const Shape& inner_shape() const;
    const Matrix4x4& transform() const;

private:
    std::shared_ptr<Shape> inner_;
    Matrix4x4 transform_;
    Matrix4x4 inverse_transform_;
    Matrix4x4 inverse_transpose_;
};
```

**hit() algorithm**:
1. Transform incoming ray to inner shape's local space using `inverse_transform_`
2. Call `inner_->hit(local_ray, t_min, t_max, rec)`
3. If hit: transform `rec.point` back to world space via `transform_`; transform `rec.normal` via `inverse_transpose_`; re-normalize normal
4. Return hit result

**Boundary rules**:
- Depends on Ring 1 (Matrix4x4, Vec3, Ray) and Ring 2 (Shape, HitRecord)
- Implements Shape interface (is a Shape)
- Does NOT know about physics or physics engines

---

## 4. Ring 3: Application (New Components)

### 4.1 PhysicsSimulator (Abstract Interface)

```
using BodyId = uint32_t;

struct PhysicsTransform {
    Point3 position;
    Quaternion rotation;
};

enum class PhysicsShapeType {
    SPHERE, BOX, CYLINDER, PLANE, MESH
};

struct PhysicsBodyDesc {
    PhysicsShapeType shape_type;
    Point3 position;
    Quaternion rotation;
    // Shape-specific dimensions
    double sphere_radius;
    Vec3 box_half_extents;
    double cylinder_radius;
    double cylinder_half_height;
    // Physics properties
    PhysicsProperties properties;
};

class PhysicsSimulator {
    virtual ~PhysicsSimulator() = default;

    virtual BodyId add_body(const PhysicsBodyDesc& desc) = 0;
    virtual void step(double dt) = 0;
    virtual PhysicsTransform get_transform(BodyId id) const = 0;
    virtual void set_gravity(const Vec3& gravity) = 0;
};
```

**Boundary rules**:
- Depends on Ring 2 (PhysicsProperties) and Ring 1 (Vec3, Point3, Quaternion)
- Does NOT `#include` any Jolt/physics-engine headers
- All types used are nwave types, not third-party types
- This interface is the dependency inversion boundary

### 4.2 AnimationRenderer

```
class AnimationRenderer {
    void render_animation(
        Scene& scene,
        const Camera& camera,
        const AnimationConfig& config,
        const RenderSettings& settings,
        PhysicsSimulator& physics,
        std::function<void(const std::string&, const std::vector<Color3>&, int, int)> write_frame,
        std::function<void(int current, int total, double elapsed)> progress
    );

private:
    Renderer renderer_;  // Composition: uses existing Renderer for per-frame rendering
};
```

**Boundary rules**:
- Depends on Ring 2 (Scene, Camera, AnimationConfig, RenderSettings, TransformedShape) and Ring 1 (math types)
- Depends on Ring 3 (PhysicsSimulator interface, Renderer)
- Does NOT depend on Ring 4 -- file writing and physics implementation are injected via callbacks and interface
- Does NOT `#include` yaml-cpp, Jolt, or any infrastructure header

### 4.3 Integration with Existing Renderer

AnimationRenderer uses the existing Renderer via composition:

```
For each frame:
    // Update physics
    for (int s = 0; s < config.steps_per_frame(); ++s)
        physics.step(config.physics_timestep);

    // Update transforms
    for each (body_id, transformed_shape) pair:
        PhysicsTransform t = physics.get_transform(body_id);
        Matrix4x4 matrix = Matrix4x4::from_translation_rotation(t.position, t.rotation);
        transformed_shape->set_transform(matrix);

    // Render using existing Renderer (unchanged)
    auto pixels = renderer_.render(camera, scene, settings);

    // Write frame (delegated to caller via callback)
    write_frame(frame_filename, pixels, width, height);
```

The existing Renderer sees TransformedShapes as ordinary Shapes (they implement the Shape interface). No changes to Renderer are needed.

---

## 5. Ring 4: Infrastructure (New Components)

### 5.1 YamlSceneLoader

```
struct SceneLoadResult {
    Scene scene;
    Camera camera;
    RenderSettings settings;
    std::optional<AnimationConfig> animation_config;
    // PhysicsProperties associated with shapes by index
    std::vector<std::pair<int, PhysicsProperties>> physics_properties;
    std::vector<std::string> warnings;
};

class YamlSceneLoader {
    SceneLoadResult load(const std::string& yaml_path);

private:
    // Per-type parsing (internal)
    std::shared_ptr<Material> parse_material(const YAML::Node& node);
    std::shared_ptr<Shape> parse_shape(const YAML::Node& node, material_map);
    std::shared_ptr<Light> parse_light(const YAML::Node& node);
    Camera parse_camera(const YAML::Node& node, int width_override);
    PhysicsProperties parse_physics(const YAML::Node& node);
    AnimationConfig parse_animation(const YAML::Node& node);
};
```

**Boundary rules**:
- Can `#include` yaml-cpp headers
- Constructs Ring 2 (Domain) objects from parsed YAML
- Does NOT construct Ring 3 (Application) objects -- no Renderer or AnimationRenderer creation here

### 5.2 Validator

```
struct ValidationCheck {
    std::string category;
    bool passed;
    std::string summary;
    std::vector<std::string> errors;
};

struct ValidationResult {
    bool valid;
    std::vector<ValidationCheck> checks;
    int error_count;
};

class Validator {
    ValidationResult validate(
        const Scene& scene,
        const std::optional<AnimationConfig>& animation_config,
        bool physics_animate_requested
    );

private:
    void check_structure(const Scene& scene, std::vector<ValidationCheck>& checks);
    void check_materials(const Scene& scene, std::vector<ValidationCheck>& checks);
    void check_physics_properties(/* ... */);
    void check_animation_config(/* ... */);
    // Edit distance for material name suggestions
    int levenshtein_distance(const std::string& a, const std::string& b);
};
```

**Boundary rules**:
- Inspects Ring 2 (Domain) objects
- Can use C++ standard library for string operations
- No external library dependency needed (edit distance is a simple algorithm)

### 5.3 JoltPhysicsSimulator

```
class JoltPhysicsSimulator : public PhysicsSimulator {
    JoltPhysicsSimulator();
    ~JoltPhysicsSimulator() override;

    BodyId add_body(const PhysicsBodyDesc& desc) override;
    void step(double dt) override;
    PhysicsTransform get_transform(BodyId id) const override;
    void set_gravity(const Vec3& gravity) override;

private:
    // Jolt-specific members
    std::unique_ptr<JPH::PhysicsSystem> physics_system_;
    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator_;
    std::unique_ptr<JPH::JobSystemThreadPool> job_system_;
    // Layer interfaces (Jolt boilerplate)
    // Body ID mapping: nwave BodyId -> JPH::BodyID
    std::unordered_map<BodyId, JPH::BodyID> body_map_;
    BodyId next_id_ = 0;
};
```

**Boundary rules**:
- `#include` Jolt headers here and ONLY here
- Converts between nwave types and Jolt types internally
- Exposes only the PhysicsSimulator interface (Ring 3 types) to callers
- Encapsulates all Jolt initialization, simulation, and cleanup

### 5.4 CLI Dispatcher

```
struct CLIConfig {
    std::string command;  // "validate", "render", or "help"
    std::string scene_file;
    bool physics_animate = false;
    std::optional<int> width_override;
    std::optional<int> spp_override;
    std::optional<int> max_depth_override;
    std::optional<int> fps_override;
    std::optional<std::string> output_override;
    std::optional<std::string> output_dir_override;
};

CLIConfig parse_cli(int argc, char* argv[]);
void print_usage();
```

**Boundary rules**: Ring 4, can access all inner rings. Produces configuration that drives the pipeline.

---

## 6. Dependency Diagram (New + Existing Components)

```
                    EXTERNAL LIBRARIES
                    ==================
                    Jolt Physics (MIT)
                    yaml-cpp (MIT)
                          |
                          v
    +--------------------------------------------------+
    |           Ring 4: Infrastructure                  |
    |                                                  |
    |  JoltPhysicsSimulator ---> [Jolt Physics]        |
    |       |                                          |
    |       | implements                               |
    |       v                                          |
    |  YamlSceneLoader ---> [yaml-cpp]                 |
    |       |                                          |
    |  Validator                                       |
    |       |                                          |
    |  CLI Dispatcher                                  |
    |       |                                          |
    |  [EXISTING] PPMWriter                            |
    +-----|--------------------------------------------+
          | depends on
          v
    +--------------------------------------------------+
    |           Ring 3: Application                     |
    |                                                  |
    |  PhysicsSimulator (abstract interface)            |
    |       ^                                          |
    |       | used by                                  |
    |  AnimationRenderer -----> [EXISTING] Renderer     |
    +-----|--------------------------------------------+
          | depends on
          v
    +--------------------------------------------------+
    |           Ring 2: Domain                          |
    |                                                  |
    |  [NEW] TransformedShape -----> [EXISTING] Shape   |
    |  [NEW] PhysicsProperties                         |
    |  [NEW] AnimationConfig                           |
    |                                                  |
    |  [EXISTING] Sphere, Box, Plane, Cylinder, ...    |
    |  [EXISTING] Lambertian, Metal, Dielectric, ...   |
    |  [EXISTING] PointLight, DirectionalLight         |
    |  [EXISTING] Camera, Scene, HitRecord             |
    +-----|--------------------------------------------+
          | depends on
          v
    +--------------------------------------------------+
    |           Ring 1: Core / Math                     |
    |                                                  |
    |  [NEW] Matrix4x4                                 |
    |  [NEW] Quaternion                                |
    |                                                  |
    |  [EXISTING] Vec3, Point3, Color3, Ray, AABB      |
    |  [EXISTING] MathUtils                            |
    +--------------------------------------------------+
```

---

## 7. File Organization (New Files)

```
src/
  core/                              # Ring 1
    [EXISTING] vec3.h, ray.h, aabb.h/.cpp, math_utils.h
    [NEW] matrix4x4.h                # 4x4 transform matrix
    [NEW] quaternion.h               # Unit quaternion for rotation

  domain/                            # Ring 2
    [EXISTING] hit_record.h, scene.h/.cpp, camera.h/.cpp
    [NEW] physics_properties.h       # BodyType enum + PhysicsProperties struct
    [NEW] animation_config.h         # AnimationConfig struct
    shapes/
      [EXISTING] shape.h, sphere.h/.cpp, plane.h/.cpp, box.h/.cpp, ...
      [NEW] transformed_shape.h/.cpp # Shape decorator with Matrix4x4 transform

  application/                       # Ring 3
    [EXISTING] renderer.h/.cpp
    [NEW] physics_simulator.h        # Abstract interface + PhysicsTransform + PhysicsBodyDesc
    [NEW] animation_renderer.h/.cpp  # Orchestrates physics + render loop

  infrastructure/                    # Ring 4
    [EXISTING] ppm_writer.h/.cpp
    [NEW] yaml_scene_loader.h/.cpp   # YAML parsing into domain objects
    [NEW] validator.h/.cpp           # Scene validation with error reporting
    [NEW] jolt_physics_simulator.h/.cpp  # Jolt adapter implementing PhysicsSimulator
    [NEW] cli_dispatcher.h/.cpp      # Subcommand routing and flag parsing

  main.cpp                           # Modified: uses CLI dispatcher, supports YAML + physics paths

tests/
  core/
    [NEW] matrix4x4_test.cpp
    [NEW] quaternion_test.cpp
  domain/
    [NEW] transformed_shape_test.cpp
    [NEW] physics_properties_test.cpp
  application/
    [NEW] animation_renderer_test.cpp
  infrastructure/
    [NEW] yaml_scene_loader_test.cpp
    [NEW] validator_test.cpp
    [NEW] jolt_physics_simulator_test.cpp
    [NEW] cli_dispatcher_test.cpp

scenes/
  [NEW] nwave_bowling.yaml          # Demo scene: ball hits W letter blocks
  [NEW] simple.yaml                 # Minimal test scene: sphere on plane
```

---

## 8. Integration Points (New Boundaries)

| Integration Point | From | To | Data Crossing | Validation |
|---|---|---|---|---|
| **YAML to Domain** | YamlSceneLoader (R4) | Scene, Camera, Materials, Shapes, Lights, PhysicsProperties, AnimationConfig (R2) | Parsed YAML nodes to domain constructors | Material ref resolution, shape type dispatch, parameter validation |
| **Domain to Physics** | AnimationRenderer (R3) | PhysicsSimulator (R3 interface, R4 impl) | PhysicsBodyDesc (shape type + dimensions + properties) | Shape-type-to-collision-shape mapping; concave mesh constraint |
| **Physics to Domain** | JoltPhysicsSimulator (R4) | TransformedShape (R2) via AnimationRenderer (R3) | PhysicsTransform (position + quaternion) -> Matrix4x4 -> TransformedShape | NaN guard on positions; quaternion normalization |
| **Domain to Renderer** | TransformedShape (R2) | Renderer (R3) | Shape::hit() interface (transparent to Renderer) | Renderer sees TransformedShape as any Shape |
| **CLI to Pipeline** | CLI Dispatcher (R4) | SceneLoader, Validator, Renderer, AnimationRenderer | CLIConfig struct with overrides | Subcommand validation, flag type checking |

---

## 9. Boundary Enforcement (CMake Targets)

```cmake
# Ring 1: No external deps
target_link_libraries(nwave_core PRIVATE) # nothing

# Ring 2: Only Ring 1
target_link_libraries(nwave_domain PUBLIC nwave_core)

# Ring 3: Only Ring 2 (transitively Ring 1)
target_link_libraries(nwave_application PUBLIC nwave_domain)

# Ring 4: All inner rings + external libraries
target_link_libraries(nwave_infrastructure PUBLIC
    nwave_application
    yaml-cpp::yaml-cpp
    Jolt
)
```

An accidental `#include <Jolt/Jolt.h>` in any Ring 1-3 source file will produce a compilation error because the Jolt include path is only available to `nwave_infrastructure`.
