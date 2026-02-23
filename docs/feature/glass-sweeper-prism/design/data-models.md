# Glass Sweeper Prism -- Data Models

## Overview

Two data model changes are required. One new field in the domain layer (`PhysicsProperties`), one new YAML key parsed in the infrastructure layer. No new structs, no new enums, no schema migrations.

---

## 1. PhysicsProperties Struct

**File**: `src/domain/physics_properties.h`

### Current State (line 15-22)

```cpp
struct PhysicsProperties {
    BodyType body_type{BodyType::STATIC};
    double mass{1.0};
    Vec3 initial_velocity{0.0, 0.0, 0.0};
    double friction{0.5};
    double restitution{0.3};
    bool start_asleep{false};
};
```

### Required Change

Add one field: `std::optional<int> wake_frame`.

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `wake_frame` | `std::optional<int>` | `std::nullopt` | Frame at which this specific body is activated. Nullopt means no per-body wake (uses global `AnimationConfig::wake_frame` if applicable). |

### Behavioral Contract

- When `wake_frame` has a value AND `start_asleep` is `true`: the body starts deactivated and is individually activated when the animation frame counter equals `wake_frame.value()`.
- When `wake_frame` is `nullopt` AND `start_asleep` is `true`: the body relies on the existing global `AnimationConfig::wake_frame` mechanism or contact-based waking.
- When `start_asleep` is `false`: `wake_frame` is ignored (body is active from frame 0).

### Backward Compatibility

The field defaults to `std::nullopt`. Existing YAML files that omit `wake_frame` produce identical behavior to current code. No existing test or scene file needs modification.

### Include Dependency

Requires `#include <optional>` added to `physics_properties.h`.

---

## 2. YAML Schema Additions

**File**: `src/infrastructure/yaml_scene_loader.cpp`, function `parse_physics()` (line 93-117)

### New Material Entry (no parser change)

```yaml
- name: blue_glass
  type: dielectric
  ior: 1.5
  tint: [0.4, 0.4, 0.95]
```

The existing `create_dielectric()` function (line 35-42) already parses `ior` and optional `tint`. No code change needed.

### New Physics Field: `wake_frame`

```yaml
physics:
  body_type: kinematic
  initial_velocity: [0, 0, -3.44]
  start_asleep: true
  wake_frame: 60        # <-- NEW FIELD
  friction: 0.3
  restitution: 0.2
```

| YAML Key | Type | Required | Default | Constraint |
|----------|------|----------|---------|------------|
| `wake_frame` | int | No | Not set (nullopt) | Must be >= 0 when present |

### Sweeper Prism Object Entry (no parser change)

```yaml
- name: sweeper_prism
  type: box
  min: [-4, 0.01, 4.0]
  max: [4, 1.5, 4.3]
  material: blue_glass
  physics:
    body_type: kinematic
    initial_velocity: [0, 0, -3.44]
    start_asleep: true
    wake_frame: 60
    friction: 0.3
    restitution: 0.2
```

The existing `create_shape()` function (line 149-153) already handles `type: box` with `min`/`max` fields. No code change needed for the shape itself.

---

## 3. Jolt Physics Kinematic Body Configuration

**File**: `src/infrastructure/jolt_physics_simulator.cpp`, function `add_body()` (line 209-250)

### Current Kinematic Body Creation Flow

1. `map_body_type_to_motion(BodyType::KINEMATIC)` returns `{EMotionType::Kinematic, LAYER_DYNAMIC}` -- already correct (line 201).
2. `BodyCreationSettings` is created with `EMotionType::Kinematic` -- already correct (line 221).
3. `mRestitution` and `mFriction` are set -- already correct (lines 222-223).
4. `mLinearVelocity` is set ONLY for `BodyType::DYNAMIC` (line 225) -- **this is the gap**.
5. Activation: `start_asleep: true` causes `DontActivate` (line 236) -- already correct for the sweeper.

### Required Change

The velocity assignment condition at line 225 must include `BodyType::KINEMATIC`:

**Current** (line 225):
```cpp
if (desc.properties.body_type == BodyType::DYNAMIC) {
```

**Required**: The condition should also match `BodyType::KINEMATIC`.

### Jolt Kinematic Body Behavior (no configuration needed)

These are Jolt Physics engine defaults that require no explicit configuration:

| Property | Kinematic Default | Effect |
|----------|-------------------|--------|
| Gravity | Not applied | Body moves at constant velocity without falling |
| Collision response | Pushes dynamic bodies | Kinematic acts as immovable mover |
| Self-response to collision | None | Not displaced by contact with dynamic bodies |
| Layer | LAYER_DYNAMIC | Collides with static and dynamic layers |
| Sleep on inactivity | Yes (Jolt default) | Relevant: body starts asleep via `DontActivate` |

### Velocity Application Timing

`mLinearVelocity` is set on `BodyCreationSettings` BEFORE `CreateAndAddBody()`. For a sleeping kinematic body:
- The velocity is stored in the body's motion properties at creation.
- When the body is activated (via per-body `wake_frame`), Jolt begins applying the stored velocity in subsequent `PhysicsSystem::Update()` calls.
- No separate "set velocity after wake" step is needed.

---

## 4. PhysicsSimulator Port Change

**File**: `src/application/physics_simulator.h`

### Required New Method

The `PhysicsSimulator` interface needs a method to activate a single body by its ID, enabling per-body wake. The existing `wake_all()` method activates all bodies and cannot target individuals.

| Method | Purpose |
|--------|---------|
| `wake_all()` (existing) | Activate all sleeping bodies at global wake_frame |
| `wake_body(int body_id)` (new) | Activate a single sleeping body at its per-body wake_frame |

The `AnimationRenderer` calls `wake_body(body_id)` when the current frame matches a body's `PhysicsProperties::wake_frame`.

---

## 5. Data Flow Summary

```
YAML wake_frame: 60
       |
       v
parse_physics() --> PhysicsProperties { wake_frame = 60, start_asleep = true }
       |
       v
add_body() --> Jolt body created with DontActivate + mLinearVelocity set
       |
       v
render loop frame 60 --> wake_body(body_id) --> BodyInterface::ActivateBody()
       |
       v
physics step() --> body moves at stored velocity
```
