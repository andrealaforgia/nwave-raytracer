# Mutation Testing Results -- glass-sweeper-prism

**Date**: 2026-02-20
**Phase**: 2.5 -- Mutation Testing
**Gate**: >= 80% kill rate

## Summary

| Metric         | Value   |
|----------------|---------|
| Total mutations | 10     |
| Killed          | 10     |
| Survived        | 0      |
| **Kill rate**   | **100%** |

**Quality gate: PASSED** (100% >= 80%)

## Mutation Results

| ID  | File | Mutation | Expected Catcher | Result | Failing Tests |
|-----|------|----------|-------------------|--------|---------------|
| M1  | `src/domain/physics_properties.h` | Change `wake_frame{std::nullopt}` to `wake_frame{0}` | DefaultWakeFrameIsNullopt | KILLED | PhysicsPropertiesTest.DefaultWakeFrameIsNullopt, YamlSceneLoaderWakeFrameParsingTest.WakeFrameIsNulloptWhenOmittedFromPhysicsBlock, AnimationRendererAcceptance.WakesIndividualBodiesAtTheirConfiguredWakeFrame, AnimationRenderer.CallsWakeBodyAtPerBodyWakeFrame |
| M2  | `src/infrastructure/jolt_physics_simulator.cpp` | Remove `\|\| desc.properties.body_type == BodyType::KINEMATIC` from velocity guard | kinematic velocity test | KILLED | JoltPhysicsSimulatorTest.kinematic_body_moves_at_constant_velocity_unaffected_by_gravity |
| M3  | `src/infrastructure/jolt_physics_simulator.cpp` | Change `KINEMATIC` to `STATIC` in velocity guard | kinematic velocity test | KILLED | JoltPhysicsSimulatorTest.kinematic_body_moves_at_constant_velocity_unaffected_by_gravity |
| M4  | `src/infrastructure/jolt_physics_simulator.cpp` | Remove `ActivateBody` call from `wake_body` | wake_body activation test | KILLED | JoltPhysicsSimulatorTest.wake_body_activates_sleeping_kinematic_body |
| M5  | `src/infrastructure/jolt_physics_simulator.cpp` | Remove validation check from `wake_body` | invalid id test | KILLED | JoltPhysicsSimulatorTest.wake_body_with_invalid_id_throws (crash/segfault) |
| M6  | `src/infrastructure/yaml_scene_loader.cpp` | Remove entire `wake_frame` parsing if-block | wake_frame parsing test | KILLED | WakeFrameValues/YamlSceneLoaderWakeFramePresentTest.ParsesWakeFrameFromPhysicsBlock/typical_frame, WakeFrameValues/YamlSceneLoaderWakeFramePresentTest.ParsesWakeFrameFromPhysicsBlock/zero_edge_case |
| M7  | `src/infrastructure/yaml_scene_loader.cpp` | Change key from `"wake_frame"` to `"wake_framex"` | wake_frame parsing test | KILLED | WakeFrameValues/YamlSceneLoaderWakeFramePresentTest.ParsesWakeFrameFromPhysicsBlock/typical_frame, WakeFrameValues/YamlSceneLoaderWakeFramePresentTest.ParsesWakeFrameFromPhysicsBlock/zero_edge_case |
| M8  | `src/application/animation_renderer.cpp` | Remove entire per-body wake loop | per-body wake test | KILLED | AnimationRendererAcceptance.WakesIndividualBodiesAtTheirConfiguredWakeFrame, AnimationRenderer.CallsWakeBodyAtPerBodyWakeFrame |
| M9  | `src/application/animation_renderer.cpp` | Change `==` to `!=` in `should_wake_at_frame` frame comparison | per-body wake test | KILLED | AnimationRendererAcceptance.WakesIndividualBodiesAtTheirConfiguredWakeFrame, AnimationRenderer.CallsWakeBodyAtPerBodyWakeFrame, AnimationRenderer.GlobalWakeFrameStillWorksAlongsidePerBodyWake |
| M10 | `src/application/animation_renderer.cpp` | Remove `physics_->wake_body(body_ids[i])` call from inside loop | per-body wake test | KILLED | AnimationRendererAcceptance.WakesIndividualBodiesAtTheirConfiguredWakeFrame, AnimationRenderer.CallsWakeBodyAtPerBodyWakeFrame, AnimationRenderer.GlobalWakeFrameStillWorksAlongsidePerBodyWake |

## Analysis

All 10 mutations were killed by the existing test suite. Key observations:

- **Defense in depth**: M1 (default value mutation) was caught by 4 independent tests across 3 layers (domain, infrastructure, application), demonstrating strong cross-layer coverage.
- **Boundary precision**: M2 and M3 (condition mutations on the KINEMATIC velocity guard) were both caught by the same kinematic velocity integration test, confirming the test precisely validates the boundary condition.
- **Behavioral coverage**: M4 (removing ActivateBody) and M5 (removing validation) were caught by separate, targeted tests -- one for the happy path behavior and one for the error path.
- **Parametrized test value**: M6 and M7 (YAML parsing mutations) were caught by the parametrized `ParsesWakeFrameFromPhysicsBlock` test with both typical_frame and zero_edge_case variants.
- **Redundant kill signals**: M8, M9, and M10 (animation renderer loop mutations) were each caught by 2-3 tests, including both the acceptance test and unit tests.

## Revert Verification

All mutations were reverted after testing. Final full test suite run: **343 tests passed, 0 failed**.
