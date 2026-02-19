# Mutation Testing Report: soft-body-jelly-physics

**Date**: 2026-02-19
**Status**: SKIPPED

## Reason

No C++ mutation testing tool is available in this environment.

- `mull-runner` (LLVM-based C++ mutation tester): not installed
- `mutate++` (Clang-based C++ mutation tester): not installed
- nWave mutation-test command supports Python (cosmic-ray), Java (PIT), JS/TS/C# (Stryker) only

## Skip Justification

Per nWave mutation-test skip conditions: "no mutation tool for the language" is a documented valid skip reason.

## Test Coverage Summary

The feature has comprehensive test coverage across all layers:

- **Domain types**: 3 tests (SoftBodyDesc defaults, SoftBodyMeshData, BodyType::SOFT)
- **DeformableMesh**: 12 tests (intersection, normals, AABB, vertex update, V-shape deformation)
- **Letter shapes**: 4 tests (box count, dimensions, physics properties)
- **PhysicsSimulator API**: 3 tests (soft body interface methods)
- **AnimationRenderer**: 5 tests (soft body registration, mesh update, frame ordering, backward compat)
- **Jolt soft body spike**: 3 tests (API validation)
- **Jolt soft body creation**: 7 tests (grid, position, parameters, gravity)
- **Jolt mesh extraction**: 7 tests (vertices, world space, face indices, consistency)
- **YAML parsing**: 7 tests (soft_body_cube, defaults, existing scenes)
- **Integration**: 2 tests (end-to-end scene load + animation, physics verification)

**Total feature tests**: 53
**Total project tests**: 387 (all passing)
