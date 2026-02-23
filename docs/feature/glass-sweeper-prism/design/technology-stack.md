# Glass Sweeper Prism -- Technology Stack

## Overview

This feature requires zero new dependencies. All technology is already present in the project. This document records what is reused and why no additions are needed.

---

## Existing Technology Reused

### 1. Jolt Physics -- Kinematic Body API

| Property | Value |
|----------|-------|
| Library | Jolt Physics |
| License | MIT |
| Version | Already in project (vendored/submodule) |
| New dependency | No |

**What we use**: `JPH::EMotionType::Kinematic` on `BodyCreationSettings`.

Jolt's kinematic bodies have exactly the behavior the sweeper needs:
- Move at specified linear velocity without gravity influence.
- Push dynamic bodies on contact via collision response.
- Are not displaced by collisions themselves.
- Support `DontActivate` at creation for delayed start.
- Resume stored velocity when activated via `BodyInterface::ActivateBody()`.

**Alternatives considered and rejected**:

| Alternative | Rejection Reason |
|-------------|------------------|
| Bullet Physics | Not in project. Would require new dependency for functionality Jolt already provides. |
| Manual transform animation (no physics) | Cannot push dynamic bodies. Kinematic collision response is the core requirement. |

### 2. Jolt Physics -- Per-Body Activation API

| Method | Purpose |
|--------|---------|
| `BodyInterface::ActivateBody(BodyID)` | Wake a single sleeping body. Already used in `wake_all()` loop (line 294 of `jolt_physics_simulator.cpp`). |

The existing `wake_all()` method already iterates body IDs and calls `ActivateBody()`. The new `wake_body(int)` method uses the same Jolt API call but targets a single body. No new Jolt API surface is needed.

### 3. Dielectric Material Pipeline

| Component | File | Status |
|-----------|------|--------|
| YAML parser | `yaml_scene_loader.cpp:35-42` | Already parses `ior` + `tint` |
| Material class | `domain/materials/dielectric.h` | Already supports colored glass |
| GPU shader | `ray_trace.metal` | Already renders dielectric with tint |
| Scene flattener | `scene_flattener.cpp` | Already flattens dielectric materials |
| BVH | `bvh_flattener.cpp` | Already handles box shapes |

The blue glass sweeper is a box with a `dielectric` material. Every component in the rendering pipeline already handles this combination. The existing `green_glass` material in `nwave_bowling.yaml` proves the pipeline works end-to-end.

### 4. YAML-CPP Parser

| Property | Value |
|----------|-------|
| Library | yaml-cpp |
| License | MIT |
| New dependency | No |

Adding the `wake_frame` field to YAML parsing uses the same `YAML::Node::as<int>()` pattern already used for `wake_frame` in `parse_animation_config()` (line 131 of `yaml_scene_loader.cpp`). No new yaml-cpp features needed.

### 5. Box Shape and Physics

| Component | Existing Support |
|-----------|-----------------|
| Box shape (`domain/shapes/box.h`) | Renders boxes with min/max corners |
| Box collision (`Jolt BoxShape`) | Created in `create_collision_shape()` line 174 |
| Box in BVH | Handled by `bvh_flattener.cpp` |
| Box transform tracking | `AnimationRenderer` wraps in `TransformedShape` |

The sweeper is geometrically identical to the existing W-letter blocks and chessboard tiles (all boxes). No new shape handling needed.

---

## Dependencies Summary

| Dependency | License | New? | Purpose |
|------------|---------|------|---------|
| Jolt Physics | MIT | No | Kinematic body creation and collision |
| yaml-cpp | MIT | No | Parse `wake_frame` field |
| Metal Shading Language | Apple | No | Render dielectric material (no changes) |

**New dependencies added by this feature: 0**

---

## Build Impact

- No new source files to add to `CMakeLists.txt`.
- No new include directories.
- No new link libraries.
- Compile-time impact: `#include <optional>` added to `physics_properties.h`. This header is already transitively included in most translation units via standard library headers. Negligible build time change.
