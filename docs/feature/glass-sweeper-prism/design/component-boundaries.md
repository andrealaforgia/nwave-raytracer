# Glass Sweeper Prism -- Component Boundaries

## Changes by Architecture Ring

### Domain Ring

**File**: `src/domain/physics_properties.h`

- Add `std::optional<int> wake_frame` to `PhysicsProperties` struct
- Default: `std::nullopt` (no per-body wake, preserves existing behavior)
- Requires `#include <optional>`
- No behavior logic in domain -- just data

**Boundary rule**: PhysicsProperties is a value type. It holds no references to infrastructure or application types.

### Application Ring

**File**: `src/application/animation_renderer.cpp`

- In the per-frame render loop, after the existing global `wake_frame` check (line 232), add per-body wake logic
- For each body with a per-object `wake_frame` matching the current frame, activate that body individually
- Requires the `PhysicsSimulator` port to support individual body activation

**File**: `src/application/physics_simulator.h`

- Add `virtual void wake_body(int body_id) = 0` to the `PhysicsSimulator` interface
- This is the port through which the application ring requests individual body activation
- Alternative: reuse `wake_all()` -- rejected because it wakes every body, not just the target

**Boundary rule**: AnimationRenderer depends on PhysicsSimulator (port), not on JoltPhysicsSimulator (adapter). The per-body wake logic uses the port interface only.

### Infrastructure Ring

**File**: `src/infrastructure/jolt_physics_simulator.cpp`

Two changes:

1. **Kinematic velocity** (line 225): Widen the `if` condition to apply `mLinearVelocity` for both `DYNAMIC` and `KINEMATIC` body types
2. **Individual body wake**: Implement `wake_body(int body_id)` -- call `BodyInterface::ActivateBody` for the specified body

**File**: `src/infrastructure/jolt_physics_simulator.h`

- Add `void wake_body(int body_id) override` declaration

**File**: `src/infrastructure/yaml_scene_loader.cpp`

- In `parse_physics()`, add parsing for `wake_frame` field from the YAML physics block
- Store as `std::optional<int>` in `PhysicsProperties`

**Boundary rule**: JoltPhysicsSimulator is the only file that includes Jolt headers. YAML parsing stays in YamlSceneLoader. Neither leaks into application or domain.

### Content (YAML)

**File**: `scenes/nwave_bowling.yaml`

- Add `blue_glass` material definition (dielectric, IOR 1.5, tint [0.4, 0.4, 0.95])
- Add `sweeper_prism` box object with kinematic physics, initial_velocity, start_asleep, wake_frame
- No code changes required for material or box parsing -- existing infrastructure handles both

## What Does NOT Change

| Component | Reason |
|-----------|--------|
| `src/core/*` | No new math primitives needed |
| `src/domain/shapes/box.h` | Box shape already exists and handles arbitrary dimensions |
| `src/domain/materials/dielectric.h` | Dielectric already supports tint parameter |
| `src/domain/animation_config.h` | Global `wake_frame` preserved as-is; per-body wake is in PhysicsProperties |
| `src/domain/soft_body_desc.h` | Soft body not affected |
| `src/infrastructure/metal/*` | Dielectric rendering already works in Metal shaders |
| `src/infrastructure/gpu/bvh_flattener.cpp` | BVH already handles boxes |
| `src/infrastructure/gpu/scene_flattener.cpp` | Scene flattening already handles all shape types |
| `src/main.cpp` | No new CLI flags or startup changes |
| `tests/` for existing features | Existing tests must continue to pass unchanged |

## Dependency Graph (Change Propagation)

```
physics_properties.h (add field)
    |
    +-- yaml_scene_loader.cpp (parse new field)
    |
    +-- animation_renderer.cpp (read new field, call wake_body)
            |
            +-- physics_simulator.h (add wake_body port)
                    |
                    +-- jolt_physics_simulator.cpp (implement wake_body + fix kinematic velocity)
```

## File Count Summary

| Category | Files Modified | Files Created |
|----------|---------------|---------------|
| Domain | 1 | 0 |
| Application | 2 | 0 |
| Infrastructure | 3 | 0 |
| Content (YAML) | 1 | 0 |
| **Total** | **7** | **0** |

No new files created. All changes are extensions to existing files.
