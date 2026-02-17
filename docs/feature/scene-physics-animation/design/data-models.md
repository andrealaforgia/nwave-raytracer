# Data Models: Scene Physics Animation

**Document ID**: DATA-SPA-001
**Feature**: scene-physics-animation
**Date**: 2026-02-17
**Status**: Draft
**Extends**: DATA-RAYTRACER-001 (existing data models)

---

## 1. Ring 1: Core / Math (New Data Structures)

### 1.1 Quaternion

Unit quaternion representing 3D rotation. Components stored as doubles.

| Field | Type | Description |
|---|---|---|
| `x` | double | i component |
| `y` | double | j component |
| `z` | double | k component |
| `w` | double | scalar component |

**Operations**:
- Static factories: `identity()` returns (0, 0, 0, 1); `from_axis_angle(axis, angle)` constructs from rotation axis and angle in radians
- `conjugate()`: returns (−x, −y, −z, w)
- `normalized()`: returns unit quaternion
- `to_matrix()`: converts to Matrix4x4 rotation matrix
- `operator*(Quaternion)`: Hamilton product for combining rotations
- `slerp(a, b, t)`: spherical linear interpolation for smooth rotation blending

**Invariant**: Must be unit quaternion (length = 1) for use in rotation. `normalized()` enforces this.

### 1.2 Matrix4x4

4x4 homogeneous transformation matrix stored in row-major order.

| Field | Type | Description |
|---|---|---|
| `m[4][4]` | double | 16-element matrix, row-major |

**Operations**:
- Static factories:
  - `identity()`: returns I4
  - `translation(dx, dy, dz)`: translation-only matrix
  - `from_translation_rotation(pos, quat)`: combined translation + rotation from quaternion
- `operator*(Matrix4x4)`: matrix multiplication
- `inverse()`: computes the inverse matrix (required for TransformedShape ray transformation)
- `transpose()`: transposes the matrix
- `transform_point(Point3)`: applies full 4x4 transform (includes translation). Divides by w.
- `transform_vector(Vec3)`: applies upper-left 3x3 (rotation/scale only, no translation)
- `transform_normal(Vec3)`: uses inverse-transpose of upper-left 3x3 for correct normal transformation under non-uniform scale

**from_translation_rotation formula**:
Given position `p` and unit quaternion `q = (x, y, z, w)`:
```
| 1-2(y^2+z^2)   2(xy-wz)     2(xz+wy)    px |
| 2(xy+wz)     1-2(x^2+z^2)   2(yz-wx)    py |
| 2(xz-wy)       2(yz+wx)   1-2(x^2+y^2)  pz |
| 0               0            0            1  |
```

---

## 2. Ring 2: Domain (New Data Structures)

### 2.1 PhysicsProperties

Pure data struct describing physics behavior of a scene object. No physics engine types.

| Field | Type | Default | Valid Range | Description |
|---|---|---|---|---|
| `body_type` | BodyType enum | STATIC | {STATIC, DYNAMIC, KINEMATIC} | How the physics engine treats this body |
| `mass` | double | 1.0 | > 0 (for DYNAMIC) | Mass in kg. Ignored for STATIC bodies. |
| `initial_velocity` | Vec3 | (0, 0, 0) | any | Starting linear velocity in m/s |
| `friction` | double | 0.5 | [0, 1] | Surface friction coefficient |
| `restitution` | double | 0.3 | [0, 1] | Bounciness coefficient. 0 = no bounce, 1 = perfect bounce |

**BodyType Enum**:
- `STATIC`: Never moves. Infinite mass. Collides with dynamic bodies only. Lowest cost.
- `DYNAMIC`: Fully simulated. Affected by gravity, forces, collisions. Finite mass.
- `KINEMATIC`: Moved programmatically. Not affected by physics forces. Can push dynamic bodies.

### 2.2 AnimationConfig

Configuration for physics-driven animation rendering.

| Field | Type | Default | Valid Range | Description |
|---|---|---|---|---|
| `duration` | double | 5.0 | > 0 | Total simulation time in seconds |
| `physics_timestep` | double | 1.0/60.0 | > 0, typically 1/30 to 1/120 | Fixed timestep for physics simulation |
| `render_fps` | int | 30 | > 0 | Frames per second for rendered output |
| `output_directory` | string | "frames/" | valid path | Directory for frame image files |

**Derived values**:
- `total_frames()`: `ceil(duration * render_fps)` -- number of rendered frames
- `render_dt()`: `1.0 / render_fps` -- time between rendered frames
- `steps_per_frame()`: `round(render_dt() / physics_timestep)` -- physics steps between each render

### 2.3 PhysicsTransform

Per-body transform output from physics simulation. Defined in Ring 3 (Application) alongside PhysicsSimulator.

| Field | Type | Description |
|---|---|---|
| `position` | Point3 | World-space center of mass position |
| `rotation` | Quaternion | Orientation as unit quaternion |

---

## 3. YAML Schema Definition

### 3.1 Top-Level Structure

```yaml
scene:
  gravity: [x, y, z]         # Optional. Default: [0, -9.81, 0]

  materials:                  # Required. List of named materials.
    - name: <string>          # Unique material identifier
      type: <material_type>   # lambertian | metal | dielectric | emissive
      # ... type-specific fields

  objects:                    # Required. At least one object.
    - name: <string>          # Optional but recommended. Used in error messages.
      type: <shape_type>      # sphere | plane | box | cylinder | triangle | triangle_mesh
      material: <string>      # Reference to a named material
      # ... type-specific geometry fields
      physics:                # Optional. Absent = static (no physics).
        body_type: <string>   # static | dynamic | kinematic
        mass: <number>        # Required for dynamic. kg, must be > 0.
        initial_velocity: [x, y, z]  # Optional. Default: [0, 0, 0]
        friction: <number>    # Optional. Default: 0.5. Range [0, 1].
        restitution: <number> # Optional. Default: 0.3. Range [0, 1].

  lights:                     # Required. At least one light.
    - type: <light_type>      # point | directional
      # ... type-specific fields

  camera:                     # Required. Exactly one camera.
    lookfrom: [x, y, z]
    lookat: [x, y, z]
    vup: [x, y, z]           # Optional. Default: [0, 1, 0]
    vfov: <number>            # Vertical FOV in degrees. Range [1, 179].
    image_width: <int>        # Optional. Default: 800.

  animation:                  # Optional. Required when --physics-animate is used.
    duration: <number>        # Seconds. Must be > 0.
    physics_timestep: <number> # Optional. Default: 0.01667 (1/60).
    render_fps: <int>         # Optional. Default: 30.
    output_directory: <string> # Optional. Default: "frames/".
```

### 3.2 Material Types

**Lambertian**:
```yaml
- name: red_rubber
  type: lambertian
  albedo: [0.85, 0.15, 0.15]   # RGB, each in [0, 1]
```

**Metal**:
```yaml
- name: floor_metal
  type: metal
  albedo: [0.9, 0.9, 0.9]      # RGB, each in [0, 1]
  fuzz: 0.05                     # Optional. Default: 0. Range [0, 1].
```

**Dielectric (Glass)**:
```yaml
- name: green_glass
  type: dielectric
  ior: 1.5                       # Index of refraction. Typical: 1.0 (air), 1.5 (glass), 2.42 (diamond).
  tint: [0.4, 0.95, 0.4]        # Optional. Default: [1, 1, 1] (clear).
```

**Emissive**:
```yaml
- name: neon_green
  type: emissive
  color: [0.2, 1.0, 0.2]        # Emission color
  intensity: 3.0                  # Intensity multiplier
```

### 3.3 Shape Types

**Sphere**:
```yaml
- name: ball
  type: sphere
  center: [0, 0.5, 0]
  radius: 0.5
  material: red_rubber
```

**Plane**:
```yaml
- name: floor
  type: plane
  point: [0, 0, 0]
  normal: [0, 1, 0]
  material: floor_metal
```

**Box**:
```yaml
- name: block_1
  type: box
  min: [0, 0, 0]
  max: [1, 1, 1]
  material: red_rubber
```

**Cylinder**:
```yaml
- name: pillar
  type: cylinder
  center: [3, 0, 0]
  radius: 0.3
  height: 2.0
  material: floor_metal
```

**Triangle**:
```yaml
- name: face
  type: triangle
  v0: [0, 0, 0]
  v1: [1, 0, 0]
  v2: [0.5, 1, 0]
  material: red_rubber
```

**TriangleMesh**:
```yaml
- name: ramp
  type: triangle_mesh
  vertices: [[0,0,0], [1,0,0], [0.5,1,0], [0.5,0,1]]
  indices: [0, 1, 2, 0, 2, 3]
  material: floor_metal
```

### 3.4 Light Types

**Point Light**:
```yaml
- type: point
  position: [0, 10, 5]
  color: [1.0, 1.0, 1.0]        # RGB
  intensity: 0.8                  # Multiplier
```

**Directional Light**:
```yaml
- type: directional
  direction: [0.5, -1, 0.3]     # Direction light travels
  color: [1.0, 0.95, 0.8]
  intensity: 0.6
```

### 3.5 Camera

```yaml
camera:
  lookfrom: [2, 3, 6]
  lookat: [0, 0.3, 0.5]
  vup: [0, 1, 0]
  vfov: 38
  image_width: 800               # Optional, overridable by CLI --width
```

### 3.6 Animation

```yaml
animation:
  duration: 5.0
  physics_timestep: 0.01667      # 1/60 second
  render_fps: 30
  output_directory: frames/
```

---

## 4. Validation Rules

| Rule | Field | Condition | Error Message Pattern |
|---|---|---|---|
| V-001 | materials | At least 1 defined | `No materials defined. Add a materials section.` |
| V-002 | objects | At least 1 defined | `No objects defined. Add an objects section.` |
| V-003 | lights | At least 1 defined | `No lights defined. Add a lights section.` |
| V-004 | camera | Present | `Camera section is required.` |
| V-005 | object.material | References a defined material name | `Object "NAME": unknown material "REF". Available: A, B, C.` |
| V-006 | object.material (close match) | Edit distance <= 2 from a defined name | `Did you mean: "SUGGESTION"?` |
| V-007 | physics.mass | > 0 when body_type is dynamic | `Object "NAME": physics.mass must be positive, got VALUE.` |
| V-008 | physics.friction | In [0, 1] | `Object "NAME": physics.friction must be in [0, 1], got VALUE.` |
| V-009 | physics.restitution | In [0, 1] | `Object "NAME": physics.restitution must be in [0, 1], got VALUE.` |
| V-010 | camera.vfov | In [1, 179] | `Camera vfov must be in [1, 179], got VALUE.` |
| V-011 | animation | Present when --physics-animate used | `Animation config required when --physics-animate is used.` |
| V-012 | animation.duration | > 0 | `animation.duration must be positive, got VALUE.` |
| V-013 | animation.render_fps | > 0 | `animation.render_fps must be positive, got VALUE.` |
| V-014 | object.type | One of supported types | `Object "NAME": unknown shape type "TYPE". Supported: sphere, plane, box, cylinder, triangle, triangle_mesh.` |
| V-015 | triangle_mesh + dynamic | Concave mesh cannot be dynamic | `Object "NAME": concave triangle meshes must be static. Change body_type to static.` |

---

## 5. Bowling Demo Scene (Complete YAML)

This scene reproduces the existing hardcoded `build_scene()` from `main.cpp` in YAML format, with physics properties added for the bowling demo. Block positions are derived from the 5x7 bitmap font definitions in `main.cpp`.

**Layout parameters** (from main.cpp):
- block_size = 0.12
- letter_width = 5 * 0.12 = 0.60
- gap = 0.12
- total_width = 5 * 0.60 + 4 * 0.12 = 3.48
- start_x = -3.48 / 2.0 = -1.74
- letter_z = 0.5

**Letter start X positions**:
- n: -1.74
- W: -1.74 + 0.60 + 0.12 = -1.02
- a: -1.02 + 0.60 + 0.12 = -0.30
- v: -0.30 + 0.60 + 0.12 = 0.42
- e: 0.42 + 0.60 + 0.12 = 1.14

```yaml
# nWave Bowling Demo Scene
# A bowling ball rolls toward the "nWave" text and hits the 'W'.
# The W is made of glass (Dielectric) cubes that are dynamic physics bodies.
# All other letters are static. The ball scatters the W blocks.

scene:
  gravity: [0, -9.81, 0]

  materials:
    - name: white_metal
      type: metal
      albedo: [0.9, 0.9, 0.9]
      fuzz: 0.05

    - name: black_metal
      type: metal
      albedo: [0.1, 0.1, 0.1]
      fuzz: 0.05

    - name: green_glass
      type: dielectric
      ior: 1.5
      tint: [0.4, 0.95, 0.4]

    - name: red_mat
      type: lambertian
      albedo: [0.85, 0.15, 0.15]

    - name: blue_mat
      type: lambertian
      albedo: [0.15, 0.25, 0.85]

    - name: orange_mat
      type: lambertian
      albedo: [0.9, 0.55, 0.1]

    - name: purple_mat
      type: lambertian
      albedo: [0.6, 0.2, 0.8]

  objects:
    # =====================
    # Chessboard floor (static, 8x8 grid of boxes)
    # =====================
    - name: chess_0_0
      type: box
      min: [-4.0, -0.15, -4.0]
      max: [-3.0, 0.0, -3.0]
      material: white_metal
      physics: { body_type: static }

    - name: chess_0_1
      type: box
      min: [-3.0, -0.15, -4.0]
      max: [-2.0, 0.0, -3.0]
      material: black_metal
      physics: { body_type: static }

    - name: chess_0_2
      type: box
      min: [-2.0, -0.15, -4.0]
      max: [-1.0, 0.0, -3.0]
      material: white_metal
      physics: { body_type: static }

    - name: chess_0_3
      type: box
      min: [-1.0, -0.15, -4.0]
      max: [0.0, 0.0, -3.0]
      material: black_metal
      physics: { body_type: static }

    - name: chess_0_4
      type: box
      min: [0.0, -0.15, -4.0]
      max: [1.0, 0.0, -3.0]
      material: white_metal
      physics: { body_type: static }

    - name: chess_0_5
      type: box
      min: [1.0, -0.15, -4.0]
      max: [2.0, 0.0, -3.0]
      material: black_metal
      physics: { body_type: static }

    - name: chess_0_6
      type: box
      min: [2.0, -0.15, -4.0]
      max: [3.0, 0.0, -3.0]
      material: white_metal
      physics: { body_type: static }

    - name: chess_0_7
      type: box
      min: [3.0, -0.15, -4.0]
      max: [4.0, 0.0, -3.0]
      material: black_metal
      physics: { body_type: static }

    # (Rows 1-7 follow the same pattern -- alternating white/black metal)
    # For brevity, the remaining 56 chess tiles follow the identical pattern:
    # row r, col c: min=[-4+c, -0.15, -4+r], max=[-3+c, 0.0, -3+r]
    # material: white_metal if (r+c)%2==0, else black_metal
    # All chess tiles have physics: { body_type: static }
    # Full 64-tile listing omitted for document readability.
    # The SceneLoader must generate all 64 tiles from the pattern.

    # =====================
    # Bowling ball (dynamic, rolling toward the W)
    # =====================
    - name: ball
      type: sphere
      center: [-5, 0.5, 0.62]
      radius: 0.5
      material: red_mat
      physics:
        body_type: dynamic
        mass: 2.0
        initial_velocity: [8, 0, 0]
        friction: 0.3
        restitution: 0.4

    # =====================
    # Letter 'n' -- static, red (start_x = -1.74)
    # Bitmap: row0=".....", row1=".....", row2=".##..", row3="#..#.", row4="#..#.", row5="#..#.", row6="#..#."
    # Filled positions (col, row from bottom): (1,4)(2,4) (0,3)(3,3) (0,2)(3,2) (0,1)(3,1) (0,0)(3,0)
    # =====================
    - name: n_0
      type: box
      min: [-1.62, 0.48, 0.5]
      max: [-1.50, 0.60, 0.62]
      material: red_mat

    - name: n_1
      type: box
      min: [-1.50, 0.48, 0.5]
      max: [-1.38, 0.60, 0.62]
      material: red_mat

    - name: n_2
      type: box
      min: [-1.74, 0.36, 0.5]
      max: [-1.62, 0.48, 0.62]
      material: red_mat

    - name: n_3
      type: box
      min: [-1.38, 0.36, 0.5]
      max: [-1.26, 0.48, 0.62]
      material: red_mat

    - name: n_4
      type: box
      min: [-1.74, 0.24, 0.5]
      max: [-1.62, 0.36, 0.62]
      material: red_mat

    - name: n_5
      type: box
      min: [-1.38, 0.24, 0.5]
      max: [-1.26, 0.36, 0.62]
      material: red_mat

    - name: n_6
      type: box
      min: [-1.74, 0.12, 0.5]
      max: [-1.62, 0.24, 0.62]
      material: red_mat

    - name: n_7
      type: box
      min: [-1.38, 0.12, 0.5]
      max: [-1.26, 0.24, 0.62]
      material: red_mat

    - name: n_8
      type: box
      min: [-1.74, 0.0, 0.5]
      max: [-1.62, 0.12, 0.62]
      material: red_mat

    - name: n_9
      type: box
      min: [-1.38, 0.0, 0.5]
      max: [-1.26, 0.12, 0.62]
      material: red_mat

    # =====================
    # Letter 'W' -- DYNAMIC, green glass (start_x = -1.02)
    # Bitmap: row0="#...#", row1="#...#", row2="#...#", row3="#.#.#", row4="#.#.#", row5="##.##", row6="#...#"
    # These are the blocks the ball hits. Each is a dynamic physics body.
    # =====================
    # Row 6 (top, y=0.72): "#...#" -> cols 0, 4
    - name: w_block_0
      type: box
      min: [-1.02, 0.72, 0.5]
      max: [-0.90, 0.84, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_1
      type: box
      min: [-0.54, 0.72, 0.5]
      max: [-0.42, 0.84, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    # Row 5 (y=0.60): "#...#" -> cols 0, 4
    - name: w_block_2
      type: box
      min: [-1.02, 0.60, 0.5]
      max: [-0.90, 0.72, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_3
      type: box
      min: [-0.54, 0.60, 0.5]
      max: [-0.42, 0.72, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    # Row 4 (y=0.48): "#...#" -> cols 0, 4
    - name: w_block_4
      type: box
      min: [-1.02, 0.48, 0.5]
      max: [-0.90, 0.60, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_5
      type: box
      min: [-0.54, 0.48, 0.5]
      max: [-0.42, 0.60, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    # Row 3 (y=0.36): "#.#.#" -> cols 0, 2, 4
    - name: w_block_6
      type: box
      min: [-1.02, 0.36, 0.5]
      max: [-0.90, 0.48, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_7
      type: box
      min: [-0.78, 0.36, 0.5]
      max: [-0.66, 0.48, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_8
      type: box
      min: [-0.54, 0.36, 0.5]
      max: [-0.42, 0.48, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    # Row 2 (y=0.24): "#.#.#" -> cols 0, 2, 4
    - name: w_block_9
      type: box
      min: [-1.02, 0.24, 0.5]
      max: [-0.90, 0.36, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_10
      type: box
      min: [-0.78, 0.24, 0.5]
      max: [-0.66, 0.36, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_11
      type: box
      min: [-0.54, 0.24, 0.5]
      max: [-0.42, 0.36, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    # Row 1 (y=0.12): "##.##" -> cols 0, 1, 3, 4
    - name: w_block_12
      type: box
      min: [-1.02, 0.12, 0.5]
      max: [-0.90, 0.24, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_13
      type: box
      min: [-0.90, 0.12, 0.5]
      max: [-0.78, 0.24, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_14
      type: box
      min: [-0.66, 0.12, 0.5]
      max: [-0.54, 0.24, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_15
      type: box
      min: [-0.54, 0.12, 0.5]
      max: [-0.42, 0.24, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    # Row 0 (bottom, y=0.0): "#...#" -> cols 0, 4
    - name: w_block_16
      type: box
      min: [-1.02, 0.0, 0.5]
      max: [-0.90, 0.12, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    - name: w_block_17
      type: box
      min: [-0.54, 0.0, 0.5]
      max: [-0.42, 0.12, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2

    # =====================
    # Letter 'a' -- static, blue (start_x = -0.30)
    # Bitmap: row0=".....", row1=".....", row2=".###.", row3="...#.", row4=".###.", row5="#..#.", row6=".##.#"
    # =====================
    - name: a_0
      type: box
      min: [-0.18, 0.48, 0.5]
      max: [-0.06, 0.60, 0.62]
      material: blue_mat

    - name: a_1
      type: box
      min: [-0.06, 0.48, 0.5]
      max: [0.06, 0.60, 0.62]
      material: blue_mat

    - name: a_2
      type: box
      min: [0.06, 0.48, 0.5]
      max: [0.18, 0.60, 0.62]
      material: blue_mat

    - name: a_3
      type: box
      min: [0.06, 0.36, 0.5]
      max: [0.18, 0.48, 0.62]
      material: blue_mat

    - name: a_4
      type: box
      min: [-0.18, 0.24, 0.5]
      max: [-0.06, 0.36, 0.62]
      material: blue_mat

    - name: a_5
      type: box
      min: [-0.06, 0.24, 0.5]
      max: [0.06, 0.36, 0.62]
      material: blue_mat

    - name: a_6
      type: box
      min: [0.06, 0.24, 0.5]
      max: [0.18, 0.36, 0.62]
      material: blue_mat

    - name: a_7
      type: box
      min: [-0.30, 0.12, 0.5]
      max: [-0.18, 0.24, 0.62]
      material: blue_mat

    - name: a_8
      type: box
      min: [0.06, 0.12, 0.5]
      max: [0.18, 0.24, 0.62]
      material: blue_mat

    - name: a_9
      type: box
      min: [-0.18, 0.0, 0.5]
      max: [-0.06, 0.12, 0.62]
      material: blue_mat

    - name: a_10
      type: box
      min: [-0.06, 0.0, 0.5]
      max: [0.06, 0.12, 0.62]
      material: blue_mat

    - name: a_11
      type: box
      min: [0.18, 0.0, 0.5]
      max: [0.30, 0.12, 0.62]
      material: blue_mat

    # =====================
    # Letter 'v' -- static, orange (start_x = 0.42)
    # Bitmap: row0=".....", row1=".....", row2="#...#", row3="#...#", row4=".#.#.", row5=".#.#.", row6="..#.."
    # =====================
    - name: v_0
      type: box
      min: [0.42, 0.48, 0.5]
      max: [0.54, 0.60, 0.62]
      material: orange_mat

    - name: v_1
      type: box
      min: [0.90, 0.48, 0.5]
      max: [1.02, 0.60, 0.62]
      material: orange_mat

    - name: v_2
      type: box
      min: [0.42, 0.36, 0.5]
      max: [0.54, 0.48, 0.62]
      material: orange_mat

    - name: v_3
      type: box
      min: [0.90, 0.36, 0.5]
      max: [1.02, 0.48, 0.62]
      material: orange_mat

    - name: v_4
      type: box
      min: [0.54, 0.24, 0.5]
      max: [0.66, 0.36, 0.62]
      material: orange_mat

    - name: v_5
      type: box
      min: [0.78, 0.24, 0.5]
      max: [0.90, 0.36, 0.62]
      material: orange_mat

    - name: v_6
      type: box
      min: [0.54, 0.12, 0.5]
      max: [0.66, 0.24, 0.62]
      material: orange_mat

    - name: v_7
      type: box
      min: [0.78, 0.12, 0.5]
      max: [0.90, 0.24, 0.62]
      material: orange_mat

    - name: v_8
      type: box
      min: [0.66, 0.0, 0.5]
      max: [0.78, 0.12, 0.62]
      material: orange_mat

    # =====================
    # Letter 'e' -- static, purple (start_x = 1.14)
    # Bitmap: row0=".....", row1=".....", row2=".##..", row3="#..#.", row4="####.", row5="#....", row6=".###."
    # =====================
    - name: e_0
      type: box
      min: [1.26, 0.48, 0.5]
      max: [1.38, 0.60, 0.62]
      material: purple_mat

    - name: e_1
      type: box
      min: [1.38, 0.48, 0.5]
      max: [1.50, 0.60, 0.62]
      material: purple_mat

    - name: e_2
      type: box
      min: [1.14, 0.36, 0.5]
      max: [1.26, 0.48, 0.62]
      material: purple_mat

    - name: e_3
      type: box
      min: [1.50, 0.36, 0.5]
      max: [1.62, 0.48, 0.62]
      material: purple_mat

    - name: e_4
      type: box
      min: [1.14, 0.24, 0.5]
      max: [1.26, 0.36, 0.62]
      material: purple_mat

    - name: e_5
      type: box
      min: [1.26, 0.24, 0.5]
      max: [1.38, 0.36, 0.62]
      material: purple_mat

    - name: e_6
      type: box
      min: [1.38, 0.24, 0.5]
      max: [1.50, 0.36, 0.62]
      material: purple_mat

    - name: e_7
      type: box
      min: [1.50, 0.24, 0.5]
      max: [1.62, 0.36, 0.62]
      material: purple_mat

    - name: e_8
      type: box
      min: [1.14, 0.12, 0.5]
      max: [1.26, 0.24, 0.62]
      material: purple_mat

    - name: e_9
      type: box
      min: [1.26, 0.0, 0.5]
      max: [1.38, 0.12, 0.62]
      material: purple_mat

    - name: e_10
      type: box
      min: [1.38, 0.0, 0.5]
      max: [1.50, 0.12, 0.62]
      material: purple_mat

    - name: e_11
      type: box
      min: [1.50, 0.0, 0.5]
      max: [1.62, 0.12, 0.62]
      material: purple_mat

  lights:
    - type: point
      position: [-4, 10, 2]
      color: [1.0, 0.97, 0.9]
      intensity: 0.7

  camera:
    lookfrom: [2, 3, 6]
    lookat: [0, 0.3, 0.5]
    vup: [0, 1, 0]
    vfov: 38
    image_width: 800

  animation:
    duration: 5.0
    physics_timestep: 0.01667
    render_fps: 30
    output_directory: frames/
```

### 5.1 Scene Summary

| Category | Count | Details |
|---|---|---|
| Materials | 7 | white_metal, black_metal, green_glass, red_mat, blue_mat, orange_mat, purple_mat |
| Chessboard tiles | 64 | 8x8 grid, alternating white/black metal, all static |
| Letter n blocks | 10 | red, static (no physics block) |
| Letter W blocks | 18 | green glass, dynamic (mass 0.3, friction 0.5, restitution 0.2) |
| Letter a blocks | 12 | blue, static |
| Letter v blocks | 9 | orange, static |
| Letter e blocks | 12 | purple, static |
| Bowling ball | 1 | red, dynamic (mass 2.0, velocity [8,0,0]) |
| Total objects | 126 | 64 chess + 61 letter blocks + 1 ball |
| Dynamic bodies | 19 | 18 W blocks + 1 ball |
| Static bodies | 107 | 64 chess + 43 letter blocks (n, a, v, e) |
| Lights | 1 | Point light overhead |
| Frames | 150 | 5.0s at 30 fps |

### 5.2 Physics Timeline (Expected)

| Time (s) | Frame | Event |
|---|---|---|
| 0.0 | 0 | Ball at [-5, 0.5, 0.62], velocity [8, 0, 0]. All W blocks at rest. |
| ~0.5 | 15 | Ball approaching W letter from the left. |
| ~0.6 | 18 | Ball reaches x ~ -1.0, first contact with W blocks. |
| 0.6-1.5 | 18-45 | Collision cascade. W blocks scatter outward and fall. |
| 1.5-3.0 | 45-90 | Blocks bounce on floor (restitution 0.2 = low bounce). Ball decelerates. |
| 3.0-5.0 | 90-150 | Blocks settle. Ball rolls to a stop. Letters n, a, v, e remain static. |

---

## 6. Ownership Model (New Components)

| Object | Owned By | Lifetime |
|---|---|---|
| PhysicsProperties | SceneLoadResult (by value in vector) | Scene load to animation end |
| AnimationConfig | SceneLoadResult (by value) | Scene load to animation end |
| TransformedShape | Scene (shared_ptr, replaces original shape) | Scene lifetime |
| Matrix4x4 | TransformedShape (by value) | Updated per physics frame |
| Quaternion | PhysicsTransform (by value) | Per physics step, transient |
| PhysicsSimulator (Jolt) | main.cpp (unique_ptr) | Animation lifetime; destroyed after last frame |
