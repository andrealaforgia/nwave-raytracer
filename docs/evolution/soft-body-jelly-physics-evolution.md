# Evolution Document: Soft Body Jelly Physics

**Project ID**: soft-body-jelly-physics
**Date**: 2026-02-19
**Status**: Complete

## Goal

Add soft body jelly physics with deformable mesh rendering. A pink jelly cube falls from above, hits the 'e' letter of 'nWave', makes it fall, and bounces back with wobbly soft-body physics.

## Architecture

### Ring Structure (Clean Architecture)

```
Core (Vec3, AABB, Ray)
  ↑
Domain (SoftBodyDesc, SoftBodyMeshData, DeformableMesh, BodyType::SOFT, LetterShapes)
  ↑
Application (PhysicsSimulator soft body API, AnimationRenderer soft body loop)
  ↑
Infrastructure (JoltPhysicsSimulator XPBD, YamlSceneLoader soft_body_cube, main.cpp wiring)
```

### Key Design Decisions

1. **SoftBodyDesc as pure value type** — Carries all creation parameters (grid resolution, pressure, compliance, etc.) without behavior. Lives in Domain ring.

2. **DeformableMesh as Shape subclass** — Per-frame mutable vertices with Moller-Trumbore intersection and area-weighted smooth normals. Linear face scan (no BVH) since soft body meshes are small.

3. **PhysicsSimulator interface extension** — Three new pure virtuals (add_soft_body, is_soft_body, get_soft_body_mesh) keep the Application ring decoupled from Jolt.

4. **Jolt XPBD soft body** — Uses SoftBodySharedSettings with NxNxN vertex grid, axis-aligned edge constraints, tetrahedral volume constraints (6 per cell), and triangulated surface faces on all 6 sides.

5. **AnimationRenderer dual path** — Rigid bodies use TransformedShape + relative delta transforms. Soft bodies use DeformableMesh with per-frame vertex replacement. Both paths coexist in the same render loop.

6. **Letter 'e' as compound boxes** — 5 boxes (spine, top/middle/bottom bars, right-upper segment) with dynamic physics and start_asleep=true. Avoids ttf2mesh/V-HACD dependencies.

## Phases and Steps

| Phase | Step | Title | Status |
|-------|------|-------|--------|
| 01 Domain Foundation | 01-01 | SoftBodyDesc and SoftBodyMeshData domain types | COMMIT |
| 01 Domain Foundation | 01-02 | PhysicsSimulator soft body API extension | COMMIT |
| 01 Domain Foundation | 01-03 | DeformableMesh shape with ray intersection | COMMIT |
| 02 Jolt Integration | 02-00 | Spike: Validate Jolt XPBD API availability | COMMIT |
| 02 Jolt Integration | 02-01 | JoltPhysicsSimulator soft body creation | COMMIT |
| 02 Jolt Integration | 02-02 | JoltPhysicsSimulator soft body mesh extraction | COMMIT |
| 03 Rendering Pipeline | 03-01 | DeformableMesh vertex update with smooth normals | COMMIT |
| 03 Rendering Pipeline | 03-02 | AnimationRenderer soft body loop integration | COMMIT |
| 04 YAML Scene Support | 04-01 | YAML parsing for soft_body_cube object type | COMMIT |
| 05 Demo Scene | 05-01 | Letter 'e' as compound box shape | COMMIT |
| 05 Demo Scene | 05-02 | Demo scene: pink jelly cube hits 'e' letter | COMMIT |

## Files Created

### Production Code
- `src/domain/soft_body_desc.h` — Soft body creation parameters
- `src/domain/soft_body_mesh_data.h` — Per-frame deformed mesh data
- `src/domain/shapes/deformable_mesh.h` / `.cpp` — Shape with mutable vertices
- `src/domain/shapes/letter_shapes.h` / `.cpp` — Compound box letter 'e'

### Test Code
- `tests/domain/soft_body_types_test.cpp`
- `tests/domain/deformable_mesh_test.cpp`
- `tests/domain/deformable_mesh_update_test.cpp`
- `tests/domain/letter_shapes_test.cpp`
- `tests/application/physics_simulator_soft_body_api_test.cpp`
- `tests/application/animation_renderer_soft_body_test.cpp`
- `tests/infrastructure/jolt_soft_body_api_spike_test.cpp`
- `tests/infrastructure/jolt_soft_body_creation_test.cpp`
- `tests/infrastructure/jolt_soft_body_mesh_extraction_test.cpp`
- `tests/infrastructure/yaml_soft_body_parsing_test.cpp`
- `tests/integration/jelly_demo_integration_test.cpp`

### Scene Files
- `scenes/jelly_cube_demo.yaml` — Demo scene with pink jelly cube and 'e' letter

## Files Modified

- `src/domain/physics_properties.h` — Added BodyType::SOFT
- `src/application/physics_simulator.h` — 3 new pure virtual soft body methods
- `src/application/animation_renderer.h` / `.cpp` — Soft body setup and per-frame update
- `src/infrastructure/jolt_physics_simulator.h` / `.cpp` — XPBD soft body creation, mesh extraction
- `src/infrastructure/yaml_scene_loader.h` / `.cpp` — soft_body_cube parsing, SceneLoadResult extension
- `src/main.cpp` — Pass soft_body_descs to AnimationRenderer
- `src/CMakeLists.txt` — New source files
- `tests/CMakeLists.txt` — New test files

## Test Summary

- **53 feature-specific tests** covering all acceptance criteria
- **387 total project tests**, all passing
- Test coverage spans Domain, Application, Infrastructure, and Integration layers

## Quality Gates

- [x] Roadmap created and approved (revision 2)
- [x] All 11 steps executed with COMMIT status
- [x] L1-L4 refactoring complete
- [x] Mutation testing: SKIPPED (no C++ mutation tool available)
- [x] All 387 tests passing

## Usage

Run the jelly cube demo:
```bash
./nwave render --scene scenes/jelly_cube_demo.yaml --physics-animate
```

Generate video:
```bash
ffmpeg -framerate 30 -i frames/jelly_demo/frame_%04d.ppm -c:v libx264 -pix_fmt yuv420p jelly_demo.mp4
```
