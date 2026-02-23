# Glass Sweeper Prism

**Date**: 2026-02-20
**Feature ID**: glass-sweeper-prism
**Status**: Complete

---

## 1. Feature Summary

Added a blue glass rectangular prism to the nWave bowling demo that sweeps across the chessboard, pushing all dynamic objects (W blocks, bowling ball, e blocks, jelly cube) off the board. The sweeper is a kinematic physics body that wakes at frame 60 (after the bowling impact plays out) and moves at constant velocity in the -Z direction, clearing the board by animation end.

This feature required two targeted infrastructure changes (kinematic velocity application, per-body wake activation), one domain extension (per-body `wake_frame` field), one application change (per-body wake check in the render loop), one YAML parser extension (wake_frame parsing), and YAML content additions (blue glass material + sweeper prism object). No new components or dependencies were introduced.

---

## 2. User Stories Delivered

| Story ID | Title | Status |
|----------|-------|--------|
| US-001 | Blue Glass Material in Scene YAML | Delivered |
| US-002 | Sweeper Prism Box Appears in Scene | Delivered |
| US-003 | Kinematic Velocity for Sweeper Motion | Delivered |
| US-004 | Delayed Sweep Start via Per-Body Wake Frame | Delivered |
| US-005 | Sweeper Pushes All Dynamic Objects Off the Board | Delivered |

**5 of 5 user stories delivered. 0 deferred.**

### Story Details

- **US-001 / US-002** (YAML-only): Added `blue_glass` dielectric material (IOR 1.5, tint [0.4, 0.4, 0.95]) and `sweeper_prism` box (min [-4, 0.01, 4.0], max [4, 1.5, 4.3]) to `nwave_bowling.yaml`. Zero code changes -- leveraged existing YAML parser and dielectric material system.

- **US-003** (one-line fix): Widened the `initial_velocity` condition in `jolt_physics_simulator.cpp` to include `BodyType::KINEMATIC` alongside `DYNAMIC`. Kinematic bodies now receive their configured velocity at creation time.

- **US-004** (cross-layer feature): Added `std::optional<int> wake_frame` to `PhysicsProperties`, YAML parsing for the field, `wake_body(int body_id)` method on the `PhysicsSimulator` port with Jolt implementation, and per-body wake check in the `AnimationRenderer` render loop.

- **US-005** (integration verification): Validated end-to-end that the sweeper clears all dynamic objects past z=-4 by animation end. Static objects (chessboard, ground, n/a/v letters) remain in place.

---

## 3. Architecture Decisions

### Decision 1: Sweeper as Kinematic Body (not animated transform)

Jolt's kinematic body type participates in collision detection and pushes dynamic bodies, but is not affected by forces or collisions itself. This provides physics-driven sweeping with constant velocity, no gravity, and automatic collision response.

**Rejected alternatives**: (1) Pure transform animation -- no collision detection, objects would not be pushed. (2) Dynamic body with very high mass -- affected by gravity, could be deflected by heavy bowling ball, velocity would not remain constant.

### Decision 2: Per-Body `wake_frame` in PhysicsProperties (not AnimationConfig)

Added `std::optional<int> wake_frame` to `PhysicsProperties` for per-object wake timing. The animation renderer checks each body's wake_frame during the render loop, alongside the existing global wake check.

**Rejected alternatives**: (1) Multiple global wake frames (list in AnimationConfig) -- cannot target individual bodies. (2) Named wake groups -- over-engineered for a single delayed-wake object.

### Decision 3: Extend `initial_velocity` to Kinematic Bodies (not new API)

Widened the existing velocity condition to include `BodyType::KINEMATIC`. One-line change, no new API method needed.

**Rejected alternatives**: (1) New `set_linear_velocity(body_id, vec3)` method -- unnecessary complexity for constant velocity known at creation time. (2) Post-creation velocity via `BodyInterface::SetLinearVelocity` -- same effect but requires body_id mapping, more complex for identical result.

---

## 4. Implementation Summary

### Phases Completed

| Phase | Name | Steps | Description |
|-------|------|-------|-------------|
| 01 | Domain and Port Foundation | 01-01, 01-02 | wake_frame field in PhysicsProperties, wake_body method on PhysicsSimulator port |
| 02 | Infrastructure | 02-01, 02-02, 02-03 | Kinematic velocity fix, wake_body Jolt implementation, YAML wake_frame parsing |
| 03 | Application | 03-01 | Per-body wake_frame check in animation render loop |
| 04 | Scene Content and Integration | 04-01, 04-02 | Blue glass material, sweeper prism YAML, end-to-end verification |

**Total steps executed**: 7 (across 4 phases)
**All steps passed**: Yes

### Step Execution Log

| Step | Title | Status | Tests | Summary |
|------|-------|--------|-------|---------|
| 01-01 | Per-body wake_frame field | COMMIT | PASS | Added `std::optional<int> wake_frame` to PhysicsProperties with default nullopt. 2 unit tests. |
| 01-02 | wake_body method on port | COMMIT | PASS | Pure virtual `wake_body(int)` on PhysicsSimulator. Throwing stub in Jolt, recording fake in tests. |
| 02-01 | Kinematic velocity fix | COMMIT | PASS | Widened velocity condition to include KINEMATIC. 2 integration tests. |
| 02-02 | wake_body Jolt implementation | COMMIT | PASS | Real ActivateBody implementation with body_id validation. 3 integration tests. |
| 02-03 | YAML wake_frame parsing | COMMIT | PASS | Extended parse_physics() for wake_frame. Parametrized test with typical and zero edge case. |
| 03-01 | Per-body wake in render loop | COMMIT | PASS | Per-body wake check after global wake_all. 1 acceptance + 2 unit tests. |
| 04-01 | Scene YAML content | COMMIT | PASS | blue_glass material and sweeper_prism box in nwave_bowling.yaml. |

### Production Files Modified

| File | Layer | Change |
|------|-------|--------|
| `src/domain/physics_properties.h` | Domain | Added `std::optional<int> wake_frame` field |
| `src/application/physics_simulator.h` | Application | Added `virtual void wake_body(int body_id) = 0` |
| `src/application/animation_renderer.cpp` | Application | Per-body wake_frame check in render loop |
| `src/infrastructure/jolt_physics_simulator.h` | Infrastructure | wake_body declaration |
| `src/infrastructure/jolt_physics_simulator.cpp` | Infrastructure | Kinematic velocity condition + wake_body implementation |
| `src/infrastructure/yaml_scene_loader.cpp` | Infrastructure | wake_frame parsing in parse_physics() |
| `scenes/nwave_bowling.yaml` | Content | blue_glass material + sweeper_prism object |

---

## 5. Quality Gates

### Test Results

All 343 tests pass after feature completion.

### Mutation Testing

| Metric | Value |
|--------|-------|
| Total mutations | 10 |
| Killed | 10 |
| Survived | 0 |
| **Kill rate** | **100%** |

**Quality gate: PASSED** (100% >= 80% threshold)

Key observations from mutation testing:

- **Defense in depth**: Default value mutation (M1) caught by 4 independent tests across 3 layers (domain, infrastructure, application).
- **Boundary precision**: Kinematic velocity guard mutations (M2, M3) caught by targeted integration tests.
- **Behavioral coverage**: ActivateBody removal (M4) and validation removal (M5) caught by separate happy-path and error-path tests.
- **Parametrized value**: YAML parsing mutations (M6, M7) caught by parametrized tests with typical and zero edge case variants.
- **Redundant kill signals**: Render loop mutations (M8, M9, M10) each caught by 2-3 tests including both acceptance and unit levels.

### Refactoring

L1-L4 continuous refactoring performed across all steps during the REFACTOR_CONTINUOUS phase of the TDD cycle.

---

## 6. Metrics

| Metric | Value |
|--------|-------|
| Tests before feature | 332 |
| Tests after feature | 343 |
| New tests added | 11 |
| Production files modified | 7 |
| Roadmap steps | 7 |
| Steps passed | 7 (100%) |
| Mutation kill rate | 100% (10/10) |
| User stories delivered | 5/5 |
| New components introduced | 0 |
| New dependencies introduced | 0 |

---

## 7. Completion Status

| Field | Value |
|-------|-------|
| Completion date | 2026-02-20 |
| Total roadmap steps | 7 |
| All quality gates passed | Yes |
| Mutation testing passed | Yes (100% kill rate) |
| All user stories delivered | Yes (5/5) |
| Test regressions | 0 |

---

## 8. Reference Documents

All feature documents are preserved in `docs/feature/glass-sweeper-prism/` for reference:

- `roadmap.yaml` -- 7-step execution roadmap across 4 phases
- `execution-log.yaml` -- TDD cycle log for all 7 steps
- `mutation/mutation-test-results.md` -- Mutation testing results (100% kill rate, 10/10)
- `discuss/user-stories.md` -- 5 user stories (US-001 through US-005)
- `design/architecture-design.md` -- Architecture decisions, component interaction, risk analysis
