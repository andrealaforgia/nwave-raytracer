# User Stories: Scene Physics Animation

**Document ID**: US-SPA-001
**Feature**: scene-physics-animation
**Date**: 2026-02-17
**Status**: Ready for DESIGN wave
**Slicing strategy**: Elephant Carpaccio -- thin vertical slices, each independently testable

---

## Story Map Overview

```
Walking Skeleton    Scene Loading        Physics Foundation    Animation Pipeline     Polish
US-SPA-000          US-SPA-010           US-SPA-020            US-SPA-030             US-SPA-050
(YAML->Render)      US-SPA-011           US-SPA-021            US-SPA-031             US-SPA-051
                    US-SPA-012           US-SPA-022            US-SPA-032
                    US-SPA-013           US-SPA-023
                                         US-SPA-024
```

---

## US-SPA-000: Walking Skeleton -- YAML Scene to Rendered Frame [P0]

### Problem (The Pain)
Andrea is a C++ developer maintaining a ray tracer where every scene is hardcoded in `main.cpp`. He must edit C++ source, recompile, and relink to see any scene change -- even moving a sphere by one unit. This 30-60 second recompile cycle for every tweak kills creative iteration.

### Who (The User)
- C++ developer working on a personal ray tracer project
- Works from the command line on macOS
- Wants fast iteration: change a text file, re-render, see result

### Solution (What We Build)
Parse a minimal YAML scene file containing one sphere, one plane, one point light, and one camera. Load these into existing Scene/Camera objects. Render a single frame to PPM. No physics, no animation -- just YAML to pixels.

### Domain Examples

#### Example 1: Minimal scene renders correctly
Andrea creates `simple.yaml` with a red Lambertian sphere (center [0, 0.5, 0], radius 0.5), a white metallic ground plane (y=0), a point light at [0, 10, 5], and a camera at [0, 2, 5] looking at the origin. He runs `nwave render simple.yaml`. The output `output.ppm` shows a red sphere sitting on a reflective floor, lit from above -- visually identical to the same scene hardcoded in C++.

#### Example 2: Material reference resolution
Andrea's YAML defines materials `red_rubber` (Lambertian, albedo [0.85, 0.15, 0.15]) and `floor_metal` (Metal, albedo [0.9, 0.9, 0.9], fuzz 0.05). The sphere references `material: red_rubber` and the plane references `material: floor_metal`. The system constructs the correct Material subclass for each.

#### Example 3: Missing material reference caught
Andrea's YAML references `material: "gren_glass"` on a sphere, but no material with that name exists. The system prints: `Error: Object "ball" references unknown material "gren_glass". Available materials: red_rubber, floor_metal.` The process exits with code 1, no image written.

### UAT Scenarios (BDD)

#### Scenario: Minimal YAML scene renders to PPM
Given Andrea has a file `simple.yaml` with 1 sphere (center [0, 0.5, 0], radius 0.5, material "red_rubber"), 1 plane (point [0,0,0], normal [0,1,0], material "floor_metal"), 1 point light (position [0,10,5], color [1,1,1], intensity 0.8), camera (lookfrom [0,2,5], lookat [0,0.5,0], vfov 40), and materials "red_rubber" (lambertian) and "floor_metal" (metal)
When Andrea runs `nwave render simple.yaml`
Then a file `output.ppm` is created with valid PPM format and non-zero pixel data

#### Scenario: Named materials resolve to correct types
Given Andrea's YAML defines material "green_glass" as `type: dielectric, ior: 1.5` and a sphere references `material: green_glass`
When the scene is loaded
Then the sphere's material is a Dielectric instance with index of refraction 1.5

#### Scenario: Unknown material reference fails with helpful error
Given Andrea's YAML has a sphere referencing `material: "geen_glass"` but only "green_glass" is defined
When Andrea runs `nwave render scene.yaml`
Then the system prints an error mentioning `"geen_glass"` and listing available materials
And the process exits with non-zero status
And no output image is written

#### Scenario: Camera parameters from YAML match rendering
Given Andrea's YAML sets camera lookfrom [2, 3, 6], lookat [0, 0.3, 0.5], vfov 38, and image width 800
When the scene loads and renders
Then the Camera object has image_width 800 and the rendered image dimensions match

#### Scenario: CLI width flag overrides YAML
Given Andrea's YAML specifies no explicit width (defaults to 800)
When Andrea runs `nwave render simple.yaml --width 400`
Then the rendered image is 400 pixels wide

### Acceptance Criteria
- [ ] YAML file with materials, objects, lights, and camera sections parses into Scene, Camera, Material, Shape, and Light domain objects
- [ ] Material references by name resolve to the correct Material subclass instance
- [ ] Unknown material references produce an error message naming the unresolved reference and listing available materials
- [ ] Rendered PPM output from YAML-loaded scene is visually consistent with the same scene constructed in C++
- [ ] CLI `--width` and `--spp` flags override YAML/default values

### Technical Notes
- Requires adding yaml-cpp as a CMake FetchContent dependency
- SceneLoader goes in Ring 4 (Infrastructure), constructs Ring 2 (Domain) objects
- Walking skeleton does not require physics, Matrix4x4, TransformedShape, or Jolt Physics
- Supported shape types for skeleton: Sphere, Plane, Box (minimum); others can follow
- Supported material types for skeleton: Lambertian, Metal, Dielectric (minimum)

---

## US-SPA-010: Full Shape Type Coverage in YAML Loader [P0]

### Problem (The Pain)
Andrea can load spheres and planes from YAML (via the walking skeleton), but his existing ray tracer supports six shape types (Sphere, Plane, Box, Cylinder, Triangle, TriangleMesh). Scenes that use boxes for the "nWave" letter blocks or cylinders for pillars cannot be loaded from YAML.

### Who (The User)
- C++ developer who has multiple scene ideas using boxes, cylinders, and triangles
- Needs all existing shape types available in YAML without adding new C++ code per scene

### Solution (What We Build)
Extend the YAML scene loader to parse all six existing shape types: Sphere, Plane, Box, Cylinder, Triangle, TriangleMesh. Each shape type maps its YAML properties to the existing constructor parameters.

### Domain Examples

#### Example 1: Box loading for nWave letters
Andrea defines 25 green glass boxes in YAML composing the letter "W": `type: box, min: [0.0, 0.0, 0.5], max: [0.12, 0.12, 0.62], material: green_glass`. Each box loads as a Box(min, max, material) domain object.

#### Example 2: Cylinder for a pillar
Andrea defines a cylinder: `type: cylinder, center: [3, 0, 0], radius: 0.3, height: 2.0, material: marble_white`. The system constructs a Cylinder with matching parameters.

#### Example 3: Unsupported shape type error
Andrea writes `type: cone` (not yet supported). The system reports: `Error: Object "decorative_cone" (line 45): unknown shape type "cone". Supported types: sphere, plane, box, cylinder, triangle, triangle_mesh.`

### UAT Scenarios (BDD)

#### Scenario: Box shape loads from YAML
Given a YAML file defines an object with `type: box`, `min: [0, 0, 0]`, `max: [1, 1, 1]`, `material: red_rubber`
When the scene is loaded
Then the scene contains a Box shape with min corner [0,0,0] and max corner [1,1,1]

#### Scenario: Cylinder shape loads from YAML
Given a YAML file defines an object with `type: cylinder`, `center: [0, 0, 0]`, `radius: 0.5`, `height: 2.0`, `material: floor_metal`
When the scene is loaded
Then the scene contains a Cylinder shape with matching center, radius, and height

#### Scenario: Triangle shape loads from YAML
Given a YAML file defines an object with `type: triangle`, `v0: [0,0,0]`, `v1: [1,0,0]`, `v2: [0.5,1,0]`, `material: red_rubber`
When the scene is loaded
Then the scene contains a Triangle shape with the three specified vertices

#### Scenario: Unknown shape type fails with supported type list
Given a YAML file defines an object with `type: cone`
When Andrea runs `nwave render scene.yaml`
Then the error message lists all supported shape types

### Acceptance Criteria
- [ ] Box, Cylinder, Triangle, and TriangleMesh shape types parse from YAML into correct domain objects
- [ ] All six shape types (Sphere, Plane, Box, Cylinder, Triangle, TriangleMesh) load and render correctly
- [ ] Unknown shape types produce an error listing all supported types

### Technical Notes
- Extends SceneLoader (Ring 4) with additional parsing branches per shape type
- No domain (Ring 2) changes needed -- all shape types already exist

---

## US-SPA-011: Scene Validation with Actionable Error Messages [P0]

### Problem (The Pain)
Andrea has authored a complex YAML scene with 26 objects and 3 materials. He misspelled one material name and set a negative mass on a sphere. Without validation, these errors surface as crashes or wrong renders deep in the pipeline, wasting minutes of his time tracing the cause.

### Who (The User)
- Developer authoring YAML scenes with dozens of objects
- Needs to catch errors before committing to a multi-minute render

### Solution (What We Build)
A validation pass that checks the loaded scene for: material reference integrity, parameter ranges (mass > 0, friction in [0,1], restitution in [0,1], vfov in [1,179]), structural completeness (objects, lights, camera present), and reports all errors with object name, field, value, and fix suggestion.

### Domain Examples

#### Example 1: Checklist validation pass
Andrea runs `nwave validate nwave_bowling.yaml`. Output shows a checklist: Scene structure [OK], Materials (3) [OK], Objects (26) [OK] (1 static, 25 dynamic), Material references [OK], Physics properties [OK], Animation config [OK] (5.0s, 30 fps = 150 frames), Lights (1) [OK], Camera [OK]. Final line: `Scene is valid. Ready to render.`

#### Example 2: Multiple errors reported together
Andrea's scene has `physics.mass: -2.0` on the ball and material reference `"geen_glass"` on a block. Validation reports both errors at once (not stopping at the first), each with object name and line number.

#### Example 3: Suggested fix for misspelled material
The error for `"geen_glass"` includes: `Did you mean: "green_glass"?` using edit distance matching.

### UAT Scenarios (BDD)

#### Scenario: Valid scene passes all checks
Given Andrea has a YAML scene with 3 valid materials, 26 objects with correct material references and valid physics properties, 1 light, and a camera
When Andrea runs `nwave validate scene.yaml`
Then every check shows [OK] and the output ends with `Scene is valid. Ready to render.`

#### Scenario: Negative mass reported with field and value
Given a YAML scene has object "ball" with `physics.mass: -2.0`
When Andrea runs `nwave validate scene.yaml`
Then the output includes `physics.mass must be positive, got -2.0` referencing object "ball"

#### Scenario: Multiple errors reported in single pass
Given a scene has both a negative mass on "ball" and an unresolved material on "w_block_0"
When Andrea runs `nwave validate scene.yaml`
Then both errors appear in the output and the total error count is 2

#### Scenario: Similar material name suggested
Given materials "green_glass" and "floor_metal" exist, and object "w_block_0" references "geen_glass"
When validation runs
Then the error suggests `Did you mean: "green_glass"?`

#### Scenario: Missing camera fails validation
Given a YAML scene has materials, objects, and lights but no camera section
When Andrea runs `nwave validate scene.yaml`
Then the output includes a Camera [FAIL] line explaining the camera section is required

### Acceptance Criteria
- [ ] Validation checks: structure, materials, objects, material references, physics properties, animation config, lights, camera
- [ ] All errors collected and reported in a single pass (no fail-fast on first error)
- [ ] Each error includes object name, field name, invalid value, and fix guidance
- [ ] Misspelled material names suggest the closest match
- [ ] `nwave validate <file>` command works as a standalone CLI subcommand
- [ ] Exit code 0 on valid scene, non-zero on errors

### Technical Notes
- Validator goes in Ring 4 (Infrastructure), inspects Ring 2 (Domain) objects
- Edit distance for name suggestions (Levenshtein or simple substring match)
- Validation runs implicitly before render as well, not only via `validate` subcommand

---

## US-SPA-012: Directional Light and Emissive Material in YAML [P1]

### Problem (The Pain)
Andrea's ray tracer supports directional lights and emissive materials in C++, but the YAML loader (built in the walking skeleton and shape coverage stories) only handles point lights, Lambertian, Metal, and Dielectric. Scenes that need sunlight (directional) or glowing objects (emissive) still require C++ edits.

### Who (The User)
- Developer creating outdoor scenes with sunlight
- Developer creating scenes with neon signs or glowing elements

### Solution (What We Build)
Extend the YAML loader to parse DirectionalLight (direction, color, intensity) and Emissive material (color, intensity).

### Domain Examples

#### Example 1: Directional light for sunlight
Andrea defines a light: `type: directional, direction: [0.5, -1, 0.3], color: [1.0, 0.95, 0.8], intensity: 0.6`. The system constructs a DirectionalLight casting parallel rays from that direction.

#### Example 2: Emissive material for a glowing panel
Andrea defines material `neon_green: { type: emissive, color: [0.2, 1.0, 0.2], intensity: 3.0 }` and assigns it to a thin box. The box glows green in the render.

#### Example 3: Mixed light types in one scene
Andrea's scene has both a point light (overhead) and a directional light (sunlight from the side). Both contribute to illumination and shadow casting.

### UAT Scenarios (BDD)

#### Scenario: Directional light loads from YAML
Given a YAML light has `type: directional`, `direction: [0, -1, 0]`, `color: [1,1,1]`, `intensity: 0.8`
When the scene is loaded
Then the scene contains a DirectionalLight with the specified direction and intensity

#### Scenario: Emissive material loads from YAML
Given a YAML material has `type: emissive`, `color: [1, 0.5, 0]`, `intensity: 2.0`
When the scene is loaded
Then the material is an Emissive instance with the specified color and intensity

#### Scenario: Multiple light types coexist
Given a scene has both a point light and a directional light
When the scene renders
Then both lights contribute to illumination in the rendered image

### Acceptance Criteria
- [ ] DirectionalLight type parses from YAML with direction, color, and intensity
- [ ] Emissive material type parses from YAML with color and intensity
- [ ] Both work correctly in rendered output

### Technical Notes
- Extends SceneLoader only (Ring 4); domain types already exist
- No new domain classes needed

---

## US-SPA-013: CLI Subcommand Structure (validate / render) [P0]

### Problem (The Pain)
Andrea's current executable has a single code path: build a hardcoded scene and render. The `--animate` flag is a simple boolean check. As YAML loading and physics animation are added, the CLI needs clear subcommands (`validate`, `render`) with composable flags (`--physics-animate`, `--width`, `--spp`, `--output-dir`).

### Who (The User)
- Developer who wants to validate scenes before rendering
- Developer who wants to control render parameters from the command line without editing YAML

### Solution (What We Build)
A CLI dispatcher that routes `nwave validate <scene.yaml>` to the validation path and `nwave render <scene.yaml> [flags]` to the rendering path. Flags: `--width`, `--spp`, `--max-depth`, `--output`, `--physics-animate`, `--output-dir`, `--fps`.

### Domain Examples

#### Example 1: Validate subcommand
Andrea runs `nwave validate nwave_bowling.yaml`. The system loads and validates the scene, prints the checklist, and exits without rendering.

#### Example 2: Render with overrides
Andrea runs `nwave render nwave_bowling.yaml --width 400 --spp 4`. The system renders at 400px wide with 4 samples per pixel, ignoring any defaults.

#### Example 3: Help text
Andrea runs `nwave --help`. The system prints usage showing `validate` and `render` subcommands with their flags.

### UAT Scenarios (BDD)

#### Scenario: Validate subcommand dispatches to validation
Given Andrea has a valid scene file `scene.yaml`
When Andrea runs `nwave validate scene.yaml`
Then the validation checklist prints and no image file is produced

#### Scenario: Render subcommand produces image
Given Andrea has a valid scene file `scene.yaml`
When Andrea runs `nwave render scene.yaml`
Then an image file is produced

#### Scenario: Width flag overrides default
Given default width is 800
When Andrea runs `nwave render scene.yaml --width 1920`
Then the rendered image is 1920 pixels wide

#### Scenario: Unknown subcommand shows help
When Andrea runs `nwave frobnicate scene.yaml`
Then the system prints usage information and exits with non-zero status

### Acceptance Criteria
- [ ] `nwave validate <file>` runs validation only, no render
- [ ] `nwave render <file>` loads scene from YAML and renders
- [ ] `--width`, `--spp`, `--max-depth`, `--output` flags accepted and applied
- [ ] `--physics-animate` flag accepted (activates physics path in later stories)
- [ ] `nwave --help` prints usage with subcommands and flags
- [ ] Invalid subcommand prints help and exits non-zero

### Technical Notes
- CLI parsing in Ring 4 (Infrastructure)
- Can use a simple hand-rolled parser or lightweight library (no heavy framework needed for ~10 flags)
- The `--physics-animate` flag is parsed here but its code path is wired up in US-SPA-030

---

## US-SPA-020: Physics Engine Integration -- Sphere Falls onto Plane [P0]

### Problem (The Pain)
Andrea wants objects in his scenes to move under gravity and collide. Currently, every object is fixed at its initial position forever. To simulate a ball falling and bouncing, he would need to hand-compute positions for each frame and hardcode them -- infeasible for any realistic scenario.

### Who (The User)
- Developer who wants to see a ball drop, bounce, and come to rest
- Needs physics to be automatic: define initial conditions, the engine does the rest

### Solution (What We Build)
Integrate Jolt Physics via CMake FetchContent. Create PhysicsSimulator interface (Ring 3) and JoltPhysicsSimulator adapter (Ring 4). Demonstrate with the simplest possible test: a dynamic sphere falls under gravity onto a static plane. Verify positions are physically plausible (sphere falls, bounces, settles).

### Domain Examples

#### Example 1: Sphere falls under gravity
A sphere starts at position [0, 5, 0] with zero initial velocity. After 1 second of simulation at 60 Hz (60 steps) under gravity -9.81 m/s^2, the sphere has fallen approximately 4.9 meters. Its position is near [0, 0.1, 0] (near the ground plane at y=0, accounting for radius 0.5).

#### Example 2: Sphere bounces with restitution
The sphere (restitution 0.6) hits the ground plane. It bounces upward. The bounce height is approximately 0.6 * 5.0 = 3.0 meters. After several bounces, the sphere comes to rest on the plane.

#### Example 3: Static plane does not move
The ground plane is static. After 300 physics steps with a heavy sphere bouncing on it, the plane remains at y=0. Its position is unchanged.

### UAT Scenarios (BDD)

#### Scenario: Sphere falls under gravity
Given a PhysicsSimulator with gravity [0, -9.81, 0]
And a dynamic sphere (mass 1.0, radius 0.5) at position [0, 5, 0]
And a static plane at y=0
When the simulator advances 60 steps at dt=1/60
Then the sphere's y-position is less than 1.0 (it has fallen)

#### Scenario: Sphere bounces off plane
Given a dynamic sphere (restitution 0.6) dropped from y=5.0 onto a static plane
When the simulator runs until the sphere's y-velocity becomes positive (after first ground contact)
Then the sphere's y-position is above the plane (it bounced)

#### Scenario: Static body does not move
Given a static plane at y=0
When 300 physics steps execute with dynamic bodies colliding with it
Then the plane's y-position remains 0

#### Scenario: Physics simulator resets cleanly
Given a simulation has run to completion
When a new simulation is initialized with different bodies
Then no state from the previous simulation affects the new one

### Acceptance Criteria
- [ ] Jolt Physics compiles and links via CMake FetchContent
- [ ] PhysicsSimulator interface defined in Ring 3 with add_body, step, get_transform, set_gravity
- [ ] JoltPhysicsSimulator in Ring 4 implements the interface using Jolt Physics
- [ ] A sphere dropped from height 5 under gravity reaches near-ground level after 1 second of simulation
- [ ] A sphere with restitution 0.6 bounces after hitting a static plane
- [ ] Static bodies do not change position during simulation

### Technical Notes
- Jolt Physics via FetchContent with `JPH_DOUBLE_PRECISION` enabled to match ray tracer's use of `double`
- PhysicsProperties struct added to Ring 2 (Domain): body_type, mass, initial_velocity, friction, restitution
- This story does NOT render frames -- it is a physics-only integration verified by position assertions
- Jolt requires layer/filter callbacks (BroadPhaseLayerInterface, etc.) -- minimal implementations needed

---

## US-SPA-021: Shape-to-Physics-Body Mapping (Sphere, Box, Plane) [P0]

### Problem (The Pain)
The physics engine is integrated (US-SPA-020) but only handles spheres dropped onto planes with hardcoded collision shapes. Andrea's scenes have boxes (the nWave letter blocks), planes (floor), and spheres (the ball). Each ray tracer shape type needs to map to a physics collision shape so the physics engine knows their geometry for collision detection.

### Who (The User)
- Developer creating scenes with mixed shape types that need to interact physically
- Needs boxes to collide with each other and with spheres and planes

### Solution (What We Build)
Implement the mapping from ray tracer Shape subclasses to Jolt Physics collision shapes: Sphere -> SphereShape, Box -> BoxShape (converting min/max to center + half-extents), Plane -> large static box or PlaneShape. This is the adapter logic inside JoltPhysicsSimulator.

### Domain Examples

#### Example 1: Sphere maps to SphereShape
A ray tracer Sphere with center [0, 0.5, 0] and radius 0.5 creates a Jolt SphereShape(0.5) positioned at [0, 0.5, 0].

#### Example 2: Box maps to BoxShape with converted dimensions
A ray tracer Box with min [-1, 0, -0.5] and max [1, 2, 0.5] creates a Jolt BoxShape with half-extents [1, 1, 0.5] positioned at center [0, 1, 0].

#### Example 3: Plane maps to static body
A ray tracer Plane (point [0, 0, 0], normal [0, 1, 0]) maps to a large static box or Jolt PlaneShape representing the ground. It is always static regardless of any physics block.

### UAT Scenarios (BDD)

#### Scenario: Sphere collision shape created correctly
Given a ray tracer Sphere with center [0, 5, 0] and radius 0.5 with dynamic physics properties
When it is added to the PhysicsSimulator
Then the physics body is a sphere with radius 0.5 at position [0, 5, 0]

#### Scenario: Box collision shape created with correct center and half-extents
Given a ray tracer Box with min [2, 0, -0.5] and max [3, 1.5, 0.5] with dynamic physics properties
When it is added to the PhysicsSimulator
Then the physics body is a box with half-extents [0.5, 0.75, 0.5] at center [2.5, 0.75, 0]

#### Scenario: Sphere hits box and both respond
Given a dynamic sphere rolling at velocity [5, 0, 0] and a dynamic box standing in its path
When physics simulates for 1 second
Then the box has moved from its original position (it was hit)
And the sphere's velocity has changed (it transferred energy)

### Acceptance Criteria
- [ ] Sphere shapes map to SphereShape with correct radius and position
- [ ] Box shapes map to BoxShape with correct half-extents and center position
- [ ] Plane shapes map to a static collision body
- [ ] A sphere-box collision produces physically plausible results (both bodies move)

### Technical Notes
- All mapping logic in JoltPhysicsSimulator (Ring 4)
- Cylinder mapping is deferred to US-SPA-024 (not needed for the nWave bowling scenario)
- TriangleMesh mapping requires concave mesh support (static only) -- deferred to US-SPA-024

---

## US-SPA-022: TransformedShape -- Apply Physics Transforms to Render Shapes [P0]

### Problem (The Pain)
The physics engine computes new positions and rotations for dynamic objects each frame, but the ray tracer's Shape classes (Sphere, Box) store their geometry as immutable construction parameters (center, min/max). There is no way to move a shape after construction. Andrea cannot update the visual position of objects to match their physics state.

### Who (The User)
- Developer who needs rendered frames to show objects at their physics-computed positions
- Needs rotation support so tumbling boxes look correct, not axis-aligned ghosts

### Solution (What We Build)
A TransformedShape wrapper (Ring 2) that holds an inner Shape and a Matrix4x4 transform. On `hit()`, it transforms the incoming ray into the inner shape's local space, delegates to the inner shape's `hit()`, then transforms the result back to world space. Also implement Matrix4x4 (Ring 1) with construction from translation + quaternion rotation, inverse, and point/vector transform operations.

### Domain Examples

#### Example 1: Translation-only transform
A sphere originally at [0, 5, 0] has fallen to [0, 0.5, 0]. The TransformedShape wraps the original Sphere and applies a translation of [0, -4.5, 0]. A ray aimed at [0, 0.5, 0] hits the sphere; a ray aimed at [0, 5, 0] misses.

#### Example 2: Rotation transform on a box
A box originally axis-aligned has been knocked to a 30-degree tilt. The TransformedShape applies both translation and rotation. The ray-box intersection correctly reflects the tilted geometry -- a ray that would miss the original axis-aligned box now hits the tilted version.

#### Example 3: Normal transformation preserves correctness
A ray hits a rotated box face. The hit record normal is transformed by the inverse-transpose of the transform matrix, ensuring the normal points outward from the rotated surface, not from the original axis-aligned surface.

### UAT Scenarios (BDD)

#### Scenario: Translated shape hit at new position
Given a Sphere at origin [0, 0, 0] radius 1.0 wrapped in TransformedShape with translation [3, 0, 0]
When a ray from [3, 0, 5] toward [3, 0, 0] is tested
Then the ray hits the sphere (it is now at [3, 0, 0])

#### Scenario: Original position no longer hit after translation
Given a Sphere at origin [0, 0, 0] radius 1.0 wrapped in TransformedShape with translation [3, 0, 0]
When a ray from [0, 0, 5] toward [0, 0, 0] is tested
Then the ray misses (the sphere has moved away from [0, 0, 0])

#### Scenario: Rotated box intersection correct
Given a Box [-0.5, -0.5, -0.5] to [0.5, 0.5, 0.5] wrapped in TransformedShape with 45-degree rotation around Y axis
When a ray is cast that would miss the axis-aligned box but hits the rotated box
Then the intersection is detected with correct hit point and normal

#### Scenario: Normal correctly transformed on rotated surface
Given a rotated TransformedShape box
When a ray hits a face
Then the hit record normal is perpendicular to the rotated face (not to the original axis-aligned face)

### Acceptance Criteria
- [ ] Matrix4x4 supports construction from translation vector + quaternion rotation
- [ ] Matrix4x4 supports inverse() and inverse-transpose for normal transformation
- [ ] TransformedShape wraps any Shape and applies a Matrix4x4 transform
- [ ] Rays are correctly transformed to local space and results back to world space
- [ ] Hit record normals are transformed using inverse-transpose (correct for non-uniform transforms)
- [ ] A translated sphere is hit at its new position and missed at its old position

### Technical Notes
- Matrix4x4 goes in Ring 1 (Core/Math)
- Quaternion type (or quaternion-to-matrix conversion) goes in Ring 1
- TransformedShape goes in Ring 2 (Domain, implements Shape interface)
- This is the most architecturally complex story; it bridges physics output to render input
- Existing shapes remain unmodified (Open/Closed principle)

---

## US-SPA-023: YAML Physics Properties Parsing [P0]

### Problem (The Pain)
The physics engine and shape mapping are working (US-SPA-020, 021), but physics properties (mass, velocity, friction, restitution, body_type) are only settable in C++ test code. Andrea needs to specify physics properties in YAML alongside his scene geometry, so the scene file alone defines the complete simulation.

### Who (The User)
- Developer authoring physics-enabled scenes in YAML
- Wants to iterate on physics parameters (mass, bounciness) by editing text, not recompiling

### Solution (What We Build)
Extend the YAML scene loader to parse the optional `physics:` block on each object and the `animation:` section at scene level. Objects without `physics:` default to static. The parsed PhysicsProperties are associated with their shapes and passed to the PhysicsSimulator during initialization.

### Domain Examples

#### Example 1: Dynamic ball with initial velocity
Andrea writes: `physics: { body_type: dynamic, mass: 2.0, initial_velocity: [8, 0, 0], friction: 0.3, restitution: 0.4 }` on the ball object. The loader creates PhysicsProperties with these values.

#### Example 2: Object without physics block defaults to static
Andrea defines a decorative sphere with no `physics:` block. It defaults to body_type: static, meaning it does not move during simulation.

#### Example 3: Animation section parsed
Andrea writes: `animation: { duration: 5.0, physics_timestep: 0.01667, render_fps: 30, output_directory: frames/ }`. The loader creates an AnimationConfig with these values, computing total_frames = 150.

### UAT Scenarios (BDD)

#### Scenario: Physics properties parsed from YAML
Given a YAML object has `physics: { body_type: dynamic, mass: 2.0, initial_velocity: [8, 0, 0], friction: 0.3, restitution: 0.4 }`
When the scene is loaded
Then the shape has associated PhysicsProperties with body_type DYNAMIC, mass 2.0, initial_velocity [8,0,0], friction 0.3, restitution 0.4

#### Scenario: Missing physics block defaults to static
Given a YAML object has no `physics:` block
When the scene is loaded
Then the shape has PhysicsProperties with body_type STATIC

#### Scenario: Animation section parsed correctly
Given YAML has `animation: { duration: 5.0, physics_timestep: 0.01667, render_fps: 30, output_directory: frames/ }`
When the scene is loaded
Then AnimationConfig has duration 5.0, physics_timestep approximately 1/60, render_fps 30, output_directory "frames/"

#### Scenario: Invalid physics property caught by validation
Given a YAML object has `physics: { body_type: dynamic, mass: -1.0 }`
When validation runs
Then an error reports negative mass with the object name

### Acceptance Criteria
- [ ] `physics:` block parsed per object with body_type, mass, initial_velocity, friction, restitution
- [ ] Missing `physics:` block defaults to body_type: static
- [ ] `animation:` section parsed with duration, physics_timestep, render_fps, output_directory
- [ ] Invalid physics values (negative mass, friction out of [0,1]) caught by validation
- [ ] PhysicsProperties struct associated with loaded shapes, accessible for physics initialization

### Technical Notes
- Extends SceneLoader (Ring 4) and validation checks
- PhysicsProperties struct in Ring 2 (Domain) -- defined in US-SPA-020 but parsed from YAML here
- AnimationConfig can be a simple struct in Ring 2 or Ring 3

---

## US-SPA-024: Cylinder and TriangleMesh Physics Mapping [P2]

### Problem (The Pain)
Andrea's scenes may include cylinders (pillars, columns) and triangle meshes (imported models) that need physics interaction. The sphere/box/plane mapping (US-SPA-021) does not cover these types, so cylinders and meshes pass through physics unaffected.

### Who (The User)
- Developer creating scenes with cylindrical objects or imported mesh geometry that participates in physics

### Solution (What We Build)
Extend the shape-to-physics-body mapping: Cylinder -> Jolt CylinderShape, TriangleMesh -> Jolt MeshShape (static only, with validation error if someone tries dynamic on a concave mesh).

### Domain Examples

#### Example 1: Cylinder as a rolling object
A cylinder (radius 0.3, height 1.0) marked dynamic rolls down an inclined plane and collides with a box.

#### Example 2: TriangleMesh as static environment
A triangle mesh representing a ramp (static) allows spheres to roll down its surface.

#### Example 3: Dynamic TriangleMesh rejected
Andrea marks a concave TriangleMesh as dynamic. Validation reports: `Concave triangle meshes must be static. Object "complex_model" has body_type: dynamic with type: triangle_mesh. Change to static or use convex primitive shapes for dynamic objects.`

### UAT Scenarios (BDD)

#### Scenario: Cylinder physics body created
Given a Cylinder (radius 0.3, height 1.0) with dynamic physics properties
When added to the PhysicsSimulator
Then a CylinderShape collision body is created with matching dimensions

#### Scenario: TriangleMesh creates static physics body
Given a TriangleMesh with body_type: static
When added to the PhysicsSimulator
Then a MeshShape collision body is created as a static body

#### Scenario: Dynamic TriangleMesh rejected at validation
Given a TriangleMesh with body_type: dynamic
When validation runs
Then an error explains concave meshes must be static

### Acceptance Criteria
- [ ] Cylinder shapes map to CylinderShape in physics engine
- [ ] TriangleMesh shapes map to MeshShape as static bodies
- [ ] Dynamic TriangleMesh is caught at validation with clear error message

### Technical Notes
- Extends JoltPhysicsSimulator (Ring 4) with additional shape mapping branches
- Jolt supports CylinderShape natively
- Concave mesh constraint is a fundamental physics engine limitation, not a temporary gap

---

## US-SPA-030: Animation Rendering Loop -- Physics Step + Render Frame [P0]

### Problem (The Pain)
Andrea has all the pieces: YAML scene loading, physics simulation, shape-to-physics mapping, and TransformedShape. But there is no orchestration that connects them: load a YAML scene, run physics, update shapes with transforms, render each frame, write images. He would need to manually wire these steps in main.cpp for each scene.

### Who (The User)
- Developer who wants to run `nwave render scene.yaml --physics-animate` and get frame images out
- Expects a single command to handle the entire pipeline

### Solution (What We Build)
An AnimationRenderer (Ring 3) that orchestrates: (1) load scene from YAML, (2) initialize physics from scene's physics properties, (3) for each frame -- step physics to frame time, extract transforms, apply transforms to TransformedShapes, render frame, write image. Wire this to the `--physics-animate` CLI flag.

### Domain Examples

#### Example 1: Ball bouncing on floor -- 3-second animation
Andrea's YAML has a sphere (dynamic, mass 1.0, restitution 0.7) at y=3.0 above a static plane, duration 3.0s at 30 fps. He runs `nwave render bouncing.yaml --physics-animate`. The system produces 90 frames showing the ball falling, bouncing, and settling.

#### Example 2: Progress feedback during rendering
During the 90-frame render, the CLI shows: `Frame 45/90 [====================] 50% ETA: 2m 15s`. Andrea sees progress and knows how long to wait.

#### Example 3: Physics summary before rendering
Before frames render, the CLI shows: `Simulating physics (3.0s at 60Hz)... 180 steps (0.1s)`. Then `Rendering 90 frames (800x450, 16 SPP)...`.

### UAT Scenarios (BDD)

#### Scenario: Complete animation pipeline produces frame files
Given a YAML scene with a dynamic sphere above a static plane, animation duration 2.0s, render_fps 30, output_directory "frames/"
When Andrea runs `nwave render scene.yaml --physics-animate`
Then 60 frame files (frame_0000.ppm through frame_0059.ppm) are created in the frames/ directory

#### Scenario: Objects move between frames
Given the animation produces 60 frames of a falling sphere
When comparing frame_0000.ppm and frame_0030.ppm
Then the sphere appears at a different vertical position in each frame

#### Scenario: Frame count matches duration times fps
Given animation duration is 5.0 seconds and render_fps is 30
When the animation completes
Then exactly 150 frame files are produced

#### Scenario: Physics summary displayed before render
Given a scene with 25 dynamic bodies
When the animation starts
Then the CLI output includes the number of dynamic bodies and physics step count before frame rendering begins

#### Scenario: Suggested ffmpeg command printed at completion
Given an animation renders to output_directory "frames/" at 30 fps
When all frames are written
Then the CLI prints an ffmpeg command using framerate 30 and the correct frame filename pattern

### Acceptance Criteria
- [ ] `--physics-animate` triggers the full animation pipeline: load, simulate, render loop
- [ ] Frame count equals `ceil(duration * render_fps)`
- [ ] Each frame file is a valid PPM image
- [ ] Object positions change across frames (physics is being applied)
- [ ] Physics summary (body count, step count, time) printed before rendering
- [ ] Frame progress (N/total, percentage, ETA) printed during rendering
- [ ] ffmpeg command printed at completion with correct framerate and path

### Technical Notes
- AnimationRenderer in Ring 3 (Application), orchestrates PhysicsSimulator and Renderer
- Output directory created automatically via `std::filesystem::create_directories`
- For each frame: step physics N times (steps_per_frame = ceil(render_dt / physics_dt)), extract transforms, apply to TransformedShapes, render, write

---

## US-SPA-031: Multiple Physics Steps Per Render Frame [P1]

### Problem (The Pain)
Andrea's animation config has physics at 60 Hz but rendering at 30 fps. The animation loop (US-SPA-030) needs to step physics twice per rendered frame for accurate simulation. If physics only steps once per render frame at 30 Hz, fast-moving objects may tunnel through thin walls or produce inaccurate collisions.

### Who (The User)
- Developer who wants accurate physics at 60 Hz while rendering at 30 fps for faster turnaround
- Needs confidence that reducing render fps does not break physics accuracy

### Solution (What We Build)
The animation loop steps physics `steps_per_frame = round((1/render_fps) / physics_timestep)` times between each rendered frame. If the ratio is not integer, use the accumulator pattern with interpolation between the last two physics states.

### Domain Examples

#### Example 1: 60 Hz physics, 30 fps render = 2 steps per frame
Physics timestep is 1/60, render fps is 30. Each rendered frame advances physics by 2 steps. Frame 0 shows state after step 0. Frame 1 shows state after step 2. Frame 30 shows state after step 60 (1 second).

#### Example 2: 120 Hz physics, 30 fps render = 4 steps per frame
Andrea wants higher physics accuracy for a fast collision. He sets physics_timestep to 1/120 and render_fps to 30. The system steps physics 4 times per rendered frame.

#### Example 3: Non-integer ratio handled with interpolation
Physics timestep is 1/60, render fps is 24. Steps per frame is 2.5. The system uses the accumulator pattern: after 2 full steps, interpolates the remaining 0.5 between the current and next physics state.

### UAT Scenarios (BDD)

#### Scenario: Two physics steps per render frame at 60Hz/30fps
Given physics_timestep is 1/60 and render_fps is 30
When the animation renders 1 frame
Then the physics simulator has advanced by 2 steps

#### Scenario: Four physics steps per render frame at 120Hz/30fps
Given physics_timestep is 1/120 and render_fps is 30
When the animation renders 1 frame
Then the physics simulator has advanced by 4 steps

#### Scenario: Total physics time matches animation duration
Given animation duration is 5.0 seconds
When the animation completes
Then the total physics time elapsed is 5.0 seconds (within one timestep tolerance)

### Acceptance Criteria
- [ ] Physics steps per render frame computed correctly from timestep and fps ratio
- [ ] Multiple physics steps execute between render frames
- [ ] Total simulation time matches animation duration within one timestep tolerance
- [ ] Non-integer ratios handled without accumulating drift

### Technical Notes
- Accumulator pattern from "Fix Your Timestep" article (Gaffer On Games)
- Interpolation between physics states for smooth motion at non-integer ratios
- For MVP, integer ratios (60/30, 120/30) are sufficient; interpolation can be P2

---

## US-SPA-032: CLI Flag Overrides for Animation Parameters [P1]

### Problem (The Pain)
Andrea is iterating on his nWave bowling scene. He wants to quickly preview with low resolution and low samples, then do a final render at high quality. Currently he must edit the YAML file each time to change width, SPP, or fps. He wants command-line overrides.

### Who (The User)
- Developer in the rapid iteration phase of scene development
- Needs to switch between "fast preview" and "final quality" without editing YAML

### Solution (What We Build)
CLI flags `--width`, `--spp`, `--fps`, `--output-dir` override the corresponding YAML values when present. Flag precedence: CLI flag > YAML value > built-in default.

### Domain Examples

#### Example 1: Fast preview
Andrea runs `nwave render nwave_bowling.yaml --physics-animate --width 400 --spp 4`. Renders at 400px wide with 4 samples per pixel. Takes 2 minutes instead of 12.

#### Example 2: High-quality final render
Andrea runs `nwave render nwave_bowling.yaml --physics-animate --width 1920 --spp 48`. Full HD at 48 SPP.

#### Example 3: Override output directory
Andrea runs `nwave render scene.yaml --physics-animate --output-dir /tmp/preview`. Frames go to /tmp/preview/ instead of the YAML-specified directory.

### UAT Scenarios (BDD)

#### Scenario: Width flag overrides YAML default
Given YAML does not specify width (default 800) and Andrea passes `--width 400`
When the animation renders
Then each frame image is 400 pixels wide

#### Scenario: FPS flag overrides YAML value
Given YAML specifies render_fps 30 and Andrea passes `--fps 60`
When the animation completes
Then the frame count is duration * 60 and the ffmpeg command uses framerate 60

#### Scenario: Output directory override
Given YAML specifies output_directory "frames/" and Andrea passes `--output-dir /tmp/test`
When the animation renders
Then frames are written to /tmp/test/

### Acceptance Criteria
- [ ] `--width` overrides image width
- [ ] `--spp` overrides samples per pixel
- [ ] `--fps` overrides render FPS (and frame count)
- [ ] `--output-dir` overrides frame output directory
- [ ] Precedence is CLI flag > YAML value > built-in default

### Technical Notes
- Extends CLI parsing from US-SPA-013
- AnimationConfig merge logic: CLI values overwrite YAML values where specified

---

## US-SPA-050: NWave Bowling Demo Scene in YAML [P1]

### Problem (The Pain)
Andrea has all the infrastructure for physics-animated scenes but no showcase scene that demonstrates the full capability. He wants to recreate the "ball rolling into nWave letter W" scenario from the feature description as a YAML file that produces a compelling demo video.

### Who (The User)
- Developer who wants a visually impressive demo of the physics animation feature
- Wants a reference scene that exercises all capabilities: multiple materials, many dynamic bodies, collisions

### Solution (What We Build)
A YAML scene file (`scenes/nwave_bowling.yaml`) containing: the nWave letter blocks (boxes) on a chessboard floor, a red rubber ball rolling toward the W, physics properties on all blocks and the ball, camera positioned for a good view, and animation config for a 5-second video at 30 fps.

### Domain Examples

#### Example 1: Full nWave bowling scene
25 green glass blocks compose the letter W. Each is a small box (0.12 unit side) with mass 0.3 kg, friction 0.5, restitution 0.2. A red ball (radius 0.5, mass 2.0, initial velocity [8, 0, 0]) rolls from the left. The chessboard floor is static. Camera at [2, 3, 6] looking at [0, 0.3, 0.5].

#### Example 2: Rendered video shows physics
The ball reaches the W at approximately frame 20 (0.67 seconds). Blocks scatter. By frame 90 (3 seconds), most blocks have settled on the floor. The remaining letters (n, a, v, e) are static and unaffected.

#### Example 3: Scene loads and validates cleanly
Running `nwave validate scenes/nwave_bowling.yaml` passes all checks with no errors.

### UAT Scenarios (BDD)

#### Scenario: Demo scene file loads without errors
Given the file `scenes/nwave_bowling.yaml` exists
When Andrea runs `nwave validate scenes/nwave_bowling.yaml`
Then validation passes with all checks [OK]

#### Scenario: Demo scene renders animation frames
Given the demo scene file
When Andrea runs `nwave render scenes/nwave_bowling.yaml --physics-animate`
Then 150 frames are produced (5 seconds at 30 fps)

#### Scenario: Ball and W blocks are dynamic, other letters are static
Given the demo scene's physics properties
When the scene is loaded
Then the ball and W blocks have body_type dynamic, and the n, a, v, e letter blocks have no physics (static default)

### Acceptance Criteria
- [ ] `scenes/nwave_bowling.yaml` exists and validates without errors
- [ ] Scene contains all 5 letters of "nWave" as block compositions
- [ ] Only the ball and W letter blocks are dynamic; other letters are static
- [ ] Animation produces 150 frames at 30 fps over 5 seconds
- [ ] The resulting video shows the ball hitting the W and blocks scattering

### Technical Notes
- This is a content/data story, not a code story -- the YAML file is the deliverable
- The block positions for each letter can be derived from the existing `LETTER_*` bitmap definitions in main.cpp
- Serves as the integration test and demo for the entire feature

---

## US-SPA-051: Render Progress Bar with ETA [P2]

### Problem (The Pain)
Andrea starts a 150-frame animation render and sees no output for 12 minutes. He does not know if the system is working, how far along it is, or how much longer to wait. He has considered killing the process and restarting, unsure if it was stuck.

### Who (The User)
- Developer waiting for a long multi-frame render
- Needs confidence the system is working and an estimate of remaining time

### Solution (What We Build)
A progress reporter that displays during multi-frame renders: frame counter (N/total), percentage, progress bar, elapsed time, and ETA. Updates after each frame completes.

### Domain Examples

#### Example 1: Early in render
After frame 5 of 150 completes: `Frame 5/150 [==                                      ] 3% ETA: 12m 10s`

#### Example 2: Mid-render
After frame 75 of 150: `Frame 75/150 [====================                    ] 50% ETA: 6m 05s`

#### Example 3: Completion
`Frame 150/150 [========================================] 100% Done (12m 48s)`

### UAT Scenarios (BDD)

#### Scenario: Progress shows frame count and percentage
Given a 150-frame animation render is in progress
When frame 30 completes
Then the progress output shows "Frame 30/150" and "20%"

#### Scenario: ETA decreases as render progresses
Given a render has completed 10 frames taking 60 seconds
When the ETA is computed
Then it shows approximately 840 seconds remaining (14 * 60)

#### Scenario: Completion shows total elapsed time
Given all 150 frames have rendered
When the final progress line is printed
Then it shows "Done" and the total elapsed time

### Acceptance Criteria
- [ ] Frame counter (N/total) displayed and updated after each frame
- [ ] Percentage calculated and displayed
- [ ] Visual progress bar (text-based, e.g., `[====...]`)
- [ ] ETA computed from average frame time and remaining frame count
- [ ] Total elapsed time displayed at completion

### Technical Notes
- Extends ProgressReporter (Ring 4) or creates a new AnimationProgressReporter
- Uses `\r` carriage return for in-place updates on terminal
- ETA based on running average of frame render times

---

## Story Dependency Graph

```
US-SPA-000 (Walking Skeleton: YAML -> Render)
    |
    +-- US-SPA-010 (Full Shape Coverage)
    +-- US-SPA-011 (Validation)
    +-- US-SPA-012 (DirLight + Emissive in YAML)
    +-- US-SPA-013 (CLI Subcommands)
    |
US-SPA-020 (Physics Engine: Sphere Falls)  [parallel with 010-013]
    |
    +-- US-SPA-021 (Shape-to-Physics Mapping)
    |       |
    |       +-- US-SPA-024 (Cylinder + Mesh Mapping)
    |
    +-- US-SPA-022 (TransformedShape + Matrix4x4)
    |
    +-- US-SPA-023 (Physics Props in YAML)  [depends on 000, 020]
            |
            +-- US-SPA-030 (Animation Loop)  [depends on 021, 022, 023]
                    |
                    +-- US-SPA-031 (Multiple Steps/Frame)
                    +-- US-SPA-032 (CLI Overrides)
                    +-- US-SPA-050 (nWave Demo Scene)
                    +-- US-SPA-051 (Progress Bar)
```

---

## Priority Summary

| Priority | Stories | Rationale |
|---|---|---|
| P0 (Must Have) | 000, 010, 011, 013, 020, 021, 022, 023, 030 | Minimum viable feature: YAML scene -> physics animation -> frame output |
| P1 (Should Have) | 012, 031, 032, 050 | Quality of life and completeness; demo scene proves the feature |
| P2 (Nice to Have) | 024, 051 | Extended shape support and UX polish |
