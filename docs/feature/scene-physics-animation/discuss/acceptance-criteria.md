# Acceptance Criteria: Scene Physics Animation

**Document ID**: AC-SPA-001
**Feature**: scene-physics-animation
**Date**: 2026-02-17
**Status**: Ready for DESIGN wave
**Format**: Given/When/Then BDD scenarios per story

---

## US-SPA-000: Walking Skeleton -- YAML Scene to Rendered Frame

### AC-000-1: Minimal YAML scene renders to PPM
```gherkin
Given Andrea has a file "simple.yaml" containing:
  | Section   | Content                                                        |
  | materials | "red_rubber": lambertian, albedo [0.85, 0.15, 0.15]           |
  | materials | "floor_metal": metal, albedo [0.9, 0.9, 0.9], fuzz 0.05      |
  | objects   | sphere: center [0, 0.5, 0], radius 0.5, material "red_rubber" |
  | objects   | plane: point [0,0,0], normal [0,1,0], material "floor_metal"  |
  | lights    | point: position [0, 10, 5], color [1,1,1], intensity 0.8      |
  | camera    | lookfrom [0, 2, 5], lookat [0, 0.5, 0], vfov 40              |
When Andrea runs "nwave render simple.yaml"
Then the file "output.ppm" is created
And the file starts with "P3" or "P6" (valid PPM header)
And the image dimensions match the camera's image_width and computed image_height
And at least one pixel has a non-black color value (scene rendered, not empty)
```

### AC-000-2: Named materials resolve to correct types
```gherkin
Given Andrea's YAML defines material "green_glass" as type "dielectric" with ior 1.5 and tint [0.4, 0.95, 0.4]
And a sphere references material "green_glass"
When the scene is loaded
Then the sphere's material is a Dielectric instance
And the Dielectric has index of refraction 1.5
```

### AC-000-3: Unknown material reference fails with helpful error
```gherkin
Given Andrea's YAML defines materials "red_rubber" and "green_glass"
And a sphere references material "geen_glass" (misspelled)
When Andrea runs "nwave render scene.yaml"
Then stderr contains the text "geen_glass"
And stderr contains at least one of the available material names
And the process exits with non-zero status
And no file "output.ppm" exists
```

### AC-000-4: Camera parameters from YAML control rendering
```gherkin
Given Andrea's YAML sets camera with lookfrom [2, 3, 6], lookat [0, 0.3, 0.5], vfov 38
And the YAML does not specify image width (default applies)
When the scene loads and renders
Then the rendered image width is 800 (default)
And the image height preserves the 16:9 aspect ratio (450)
```

### AC-000-5: CLI width flag overrides default
```gherkin
Given Andrea's YAML scene is valid with default width
When Andrea runs "nwave render simple.yaml --width 400"
Then the rendered image is 400 pixels wide
```

---

## US-SPA-010: Full Shape Type Coverage in YAML Loader

### AC-010-1: Box shape loads from YAML
```gherkin
Given a YAML file defines an object:
  type: box
  min: [0, 0, 0]
  max: [1, 1, 1]
  material: red_rubber
When the scene is loaded
Then the scene contains a Box shape
And the Box has min corner [0, 0, 0] and max corner [1, 1, 1]
```

### AC-010-2: Cylinder shape loads from YAML
```gherkin
Given a YAML file defines an object:
  type: cylinder
  center: [0, 0, 0]
  radius: 0.5
  height: 2.0
  material: floor_metal
When the scene is loaded
Then the scene contains a Cylinder shape with center [0,0,0], radius 0.5, height 2.0
```

### AC-010-3: Triangle shape loads from YAML
```gherkin
Given a YAML file defines an object:
  type: triangle
  v0: [0, 0, 0]
  v1: [1, 0, 0]
  v2: [0.5, 1, 0]
  material: red_rubber
When the scene is loaded
Then the scene contains a Triangle shape with the three specified vertices
```

### AC-010-4: Unknown shape type reports available types
```gherkin
Given a YAML file defines an object with type "cone"
When Andrea runs "nwave render scene.yaml"
Then the error message contains "cone"
And the error message lists supported types including "sphere", "plane", "box", "cylinder", "triangle"
And the process exits with non-zero status
```

---

## US-SPA-011: Scene Validation with Actionable Error Messages

### AC-011-1: Valid scene passes all checks
```gherkin
Given Andrea has a YAML scene with:
  | Component  | Count | Status           |
  | materials  | 3     | all valid        |
  | objects    | 26    | 1 static, 25 dyn |
  | lights     | 1     | valid            |
  | camera     | 1     | valid            |
  | animation  | 1     | valid            |
When Andrea runs "nwave validate scene.yaml"
Then every validation check shows "[OK]"
And the final line contains "Scene is valid"
And the process exits with status 0
```

### AC-011-2: Negative mass reported with context
```gherkin
Given a YAML scene has object named "ball" with physics.mass: -2.0
When Andrea runs "nwave validate scene.yaml"
Then the output contains "ball"
And the output contains "mass"
And the output contains "-2.0"
And the output contains "positive" or "must be > 0"
```

### AC-011-3: Multiple errors reported in single pass
```gherkin
Given a scene has:
  - object "ball" with physics.mass: -2.0
  - object "w_block_0" with material: "geen_glass" (unresolved)
When Andrea runs "nwave validate scene.yaml"
Then the output contains errors for both "ball" and "w_block_0"
And the error count shown is 2 (not 1)
```

### AC-011-4: Similar material name suggested
```gherkin
Given materials "green_glass" and "floor_metal" are defined
And object "w_block_0" references material "geen_glass"
When validation runs
Then the error for "w_block_0" includes the suggestion "green_glass"
```

### AC-011-5: Missing camera fails validation
```gherkin
Given a YAML scene has materials, objects, and lights but no camera section
When Andrea runs "nwave validate scene.yaml"
Then the output includes a failed check for camera
And the message explains that a camera section is required
```

---

## US-SPA-012: Directional Light and Emissive Material in YAML

### AC-012-1: Directional light loads from YAML
```gherkin
Given a YAML light section contains:
  type: directional
  direction: [0.5, -1, 0.3]
  color: [1.0, 0.95, 0.8]
  intensity: 0.6
When the scene is loaded
Then the scene contains a DirectionalLight
And the light's direction is [0.5, -1, 0.3] (or its normalization)
And the light's intensity is 0.6
```

### AC-012-2: Emissive material loads from YAML
```gherkin
Given a YAML material section contains:
  name: neon_green
  type: emissive
  color: [0.2, 1.0, 0.2]
  intensity: 3.0
When the scene is loaded
Then the material "neon_green" is an Emissive instance
And the emission color scaled by intensity produces visible glow in renders
```

### AC-012-3: Mixed light types in one scene
```gherkin
Given a scene has both a point light and a directional light
When the scene renders
Then the rendered image has illumination from both lights
And shadows are cast from both light sources
```

---

## US-SPA-013: CLI Subcommand Structure

### AC-013-1: Validate subcommand dispatches to validation only
```gherkin
Given Andrea has a valid scene file "scene.yaml"
When Andrea runs "nwave validate scene.yaml"
Then validation output is printed to stdout
And no image file is created
```

### AC-013-2: Render subcommand produces image
```gherkin
Given Andrea has a valid scene file "scene.yaml"
When Andrea runs "nwave render scene.yaml"
Then an output image file is created
```

### AC-013-3: Width flag overrides default
```gherkin
Given default image width is 800
When Andrea runs "nwave render scene.yaml --width 1920"
Then the rendered image is 1920 pixels wide
```

### AC-013-4: Unknown subcommand shows help
```gherkin
When Andrea runs "nwave frobnicate scene.yaml"
Then the output contains usage information with "validate" and "render" subcommands
And the process exits with non-zero status
```

### AC-013-5: Help flag shows usage
```gherkin
When Andrea runs "nwave --help"
Then the output lists "validate" and "render" subcommands
And the output lists available flags (--width, --spp, --physics-animate)
```

---

## US-SPA-020: Physics Engine Integration -- Sphere Falls onto Plane

### AC-020-1: Sphere falls under gravity
```gherkin
Given a PhysicsSimulator with gravity [0, -9.81, 0]
And a dynamic sphere (mass 1.0, radius 0.5) at position [0, 5, 0] with zero initial velocity
And a static plane at y = 0
When the simulator advances 60 steps at dt = 1/60
Then the sphere's y-position is less than 1.0
```

### AC-020-2: Sphere bounces off plane
```gherkin
Given a dynamic sphere (mass 1.0, radius 0.5, restitution 0.6) at position [0, 5, 0]
And a static plane at y = 0
When the simulator runs until the sphere contacts the plane and then rebounds
Then the sphere's y-position after rebound is greater than 0.5 (above the plane)
And the sphere's peak rebound height is less than 5.0 (energy lost per restitution)
```

### AC-020-3: Static body does not move
```gherkin
Given a static plane at y = 0
And a dynamic sphere (mass 5.0) dropped onto it from height 10
When 300 physics steps execute at dt = 1/60
Then the plane's position remains at y = 0 (unchanged)
```

### AC-020-4: Physics simulator resets cleanly
```gherkin
Given a simulation has run 300 steps with sphere A at position [0, 5, 0]
When a new PhysicsSimulator is created with sphere B at position [3, 2, 0]
Then sphere B's initial position is [3, 2, 0]
And no trace of sphere A exists in the new simulation
```

---

## US-SPA-021: Shape-to-Physics-Body Mapping

### AC-021-1: Sphere collision shape matches ray tracer sphere
```gherkin
Given a ray tracer Sphere with center [0, 5, 0] and radius 0.5
And PhysicsProperties: body_type DYNAMIC, mass 1.0
When the sphere is added to the PhysicsSimulator
Then the physics body has a spherical collision shape with radius 0.5
And the physics body's initial position is [0, 5, 0]
```

### AC-021-2: Box collision shape converts min/max to center/half-extents
```gherkin
Given a ray tracer Box with min [2, 0, -0.5] and max [3, 1.5, 0.5]
And PhysicsProperties: body_type DYNAMIC, mass 2.0
When the box is added to the PhysicsSimulator
Then the physics body has a box collision shape
And the initial position is [2.5, 0.75, 0] (center of min/max)
And the half-extents are [0.5, 0.75, 0.5]
```

### AC-021-3: Sphere-box collision produces plausible result
```gherkin
Given a dynamic sphere (mass 1.0, initial velocity [5, 0, 0]) at [-3, 0.5, 0]
And a dynamic box (mass 2.0) at [0, 0.75, 0]
And a static ground plane at y = 0
When the simulator runs for 120 steps at dt = 1/60
Then the box's x-position is greater than 0 (it was pushed by the sphere)
And the sphere's x-velocity has decreased (energy transferred to box)
```

---

## US-SPA-022: TransformedShape

### AC-022-1: Translated shape hit at new position
```gherkin
Given a Sphere at origin [0, 0, 0] with radius 1.0
And the sphere is wrapped in TransformedShape with translation [3, 0, 0]
When a ray from [3, 0, 5] toward direction [0, 0, -1] is tested for hit
Then the ray hits the TransformedShape
And the hit point is approximately [3, 0, 1]
```

### AC-022-2: Original position no longer hit after translation
```gherkin
Given a Sphere at origin [0, 0, 0] with radius 1.0
And the sphere is wrapped in TransformedShape with translation [3, 0, 0]
When a ray from [0, 0, 5] toward direction [0, 0, -1] is tested for hit
Then the ray misses (returns false)
```

### AC-022-3: Rotated box intersection detected
```gherkin
Given a Box from [-0.5, -0.5, -0.5] to [0.5, 0.5, 0.5]
And the box is wrapped in TransformedShape with 45-degree rotation around the Y axis
When a ray is cast that would miss the axis-aligned box but intersects the 45-degree rotated box
Then the intersection is detected (hit returns true)
```

### AC-022-4: Normal correctly transformed on rotated surface
```gherkin
Given a Box wrapped in TransformedShape with 45-degree Y-axis rotation
When a ray hits what was originally the +X face of the box
Then the hit record normal is rotated 45 degrees from the original [1, 0, 0]
And the normal is a unit vector (length approximately 1.0)
```

### AC-022-5: Matrix4x4 inverse correctness
```gherkin
Given a Matrix4x4 M constructed from translation [1, 2, 3] and 90-degree Y rotation
When M * M.inverse() is computed
Then the result is approximately the identity matrix (within floating point tolerance)
```

---

## US-SPA-023: YAML Physics Properties Parsing

### AC-023-1: Physics properties parsed from YAML
```gherkin
Given a YAML object has:
  physics:
    body_type: dynamic
    mass: 2.0
    initial_velocity: [8, 0, 0]
    friction: 0.3
    restitution: 0.4
When the scene is loaded
Then the shape's PhysicsProperties has:
  | Field            | Value         |
  | body_type        | DYNAMIC       |
  | mass             | 2.0           |
  | initial_velocity | [8, 0, 0]    |
  | friction         | 0.3           |
  | restitution      | 0.4           |
```

### AC-023-2: Missing physics block defaults to static
```gherkin
Given a YAML object has type "sphere" with center and radius but no "physics:" block
When the scene is loaded
Then the shape's PhysicsProperties has body_type STATIC
And mass is the default value (1.0)
```

### AC-023-3: Animation section parsed correctly
```gherkin
Given YAML contains:
  animation:
    duration: 5.0
    physics_timestep: 0.01667
    render_fps: 30
    output_directory: frames/
When the scene is loaded
Then AnimationConfig.duration is 5.0
And AnimationConfig.physics_timestep is approximately 0.01667
And AnimationConfig.render_fps is 30
And AnimationConfig.output_directory is "frames/"
```

### AC-023-4: Invalid physics values caught by validation
```gherkin
Given a YAML object "heavy_ball" has physics.mass: -1.0
And another object "slippery_box" has physics.friction: 1.5
When validation runs
Then errors are reported for both "heavy_ball" (negative mass) and "slippery_box" (friction > 1.0)
```

---

## US-SPA-024: Cylinder and TriangleMesh Physics Mapping

### AC-024-1: Cylinder creates correct physics body
```gherkin
Given a Cylinder (center [0, 1, 0], radius 0.3, height 2.0) with body_type DYNAMIC, mass 1.5
When added to the PhysicsSimulator
Then a cylinder collision shape is created with radius 0.3 and half-height 1.0
```

### AC-024-2: Static TriangleMesh creates mesh collision body
```gherkin
Given a TriangleMesh with 100 triangles and body_type STATIC
When added to the PhysicsSimulator
Then a mesh collision shape is created as a static body
```

### AC-024-3: Dynamic TriangleMesh rejected
```gherkin
Given a TriangleMesh with body_type DYNAMIC
When validation runs
Then an error states that concave triangle meshes must be static
And the error names the specific object and suggests changing to static
```

---

## US-SPA-030: Animation Rendering Loop

### AC-030-1: Animation pipeline produces correct frame count
```gherkin
Given a YAML scene with animation: { duration: 2.0, render_fps: 30, output_directory: "frames/" }
And the scene has a dynamic sphere above a static plane
When Andrea runs "nwave render scene.yaml --physics-animate"
Then 60 files exist in "frames/" directory (frame_0000.ppm through frame_0059.ppm)
And each file is a valid PPM image
```

### AC-030-2: Objects move between frames
```gherkin
Given a falling sphere animation producing 60 frames
When frame_0000.ppm and frame_0030.ppm are compared
Then the sphere appears at different vertical positions in the two frames
```

### AC-030-3: Frame count equals duration times fps
```gherkin
Given animation duration 5.0 and render_fps 30
When the animation completes
Then exactly 150 frame files exist
```

### AC-030-4: Physics summary displayed before rendering
```gherkin
Given a scene with 25 dynamic bodies and physics_timestep 1/60 over 5.0 seconds
When the animation starts
Then the output includes "25 dynamic bodies" (or equivalent count)
And the output includes "300 steps" (or equivalent step count)
And this summary appears before the first frame renders
```

### AC-030-5: ffmpeg command printed at completion
```gherkin
Given an animation renders to "frames/" at 30 fps
When all frames are written
Then the output includes "ffmpeg"
And the output includes "framerate 30" (or "-framerate 30")
And the output includes "frame_%04d.ppm"
```

---

## US-SPA-031: Multiple Physics Steps Per Render Frame

### AC-031-1: Two physics steps per frame at 60Hz/30fps
```gherkin
Given physics_timestep is 1/60 and render_fps is 30
When the animation renders 1 frame
Then physics has advanced by 2 steps (2 * 1/60 = 1/30 second)
```

### AC-031-2: Total physics time matches animation duration
```gherkin
Given animation duration 5.0 seconds, physics_timestep 1/60, render_fps 30
When the animation completes after 150 frames
Then total physics time elapsed is 5.0 seconds (within 1/60 tolerance)
```

### AC-031-3: Higher physics rate does not change frame count
```gherkin
Given two animation runs with same duration (3.0s) and render_fps (30) but different physics_timestep:
  Run A: physics_timestep 1/60  (2 steps per frame)
  Run B: physics_timestep 1/120 (4 steps per frame)
When both animations complete
Then both produce exactly 90 frames
```

---

## US-SPA-032: CLI Flag Overrides for Animation Parameters

### AC-032-1: Width flag overrides for animation
```gherkin
Given YAML specifies no width (default 800)
When Andrea runs "nwave render scene.yaml --physics-animate --width 400"
Then each rendered frame image is 400 pixels wide
```

### AC-032-2: FPS flag overrides YAML render_fps
```gherkin
Given YAML specifies render_fps: 30 and animation duration: 5.0
When Andrea runs "nwave render scene.yaml --physics-animate --fps 60"
Then 300 frames are produced (5.0 * 60)
And the ffmpeg command uses "framerate 60"
```

### AC-032-3: Output directory override
```gherkin
Given YAML specifies output_directory: "frames/"
When Andrea runs "nwave render scene.yaml --physics-animate --output-dir /tmp/test"
Then frame files are written to "/tmp/test/" directory
And no frames are written to "frames/"
```

---

## US-SPA-050: NWave Bowling Demo Scene

### AC-050-1: Demo scene validates without errors
```gherkin
Given the file "scenes/nwave_bowling.yaml" exists
When Andrea runs "nwave validate scenes/nwave_bowling.yaml"
Then all validation checks show [OK]
And the process exits with status 0
```

### AC-050-2: Demo scene produces correct frame count
```gherkin
Given the demo scene has animation duration 5.0 and render_fps 30
When Andrea runs "nwave render scenes/nwave_bowling.yaml --physics-animate"
Then 150 frames are produced
```

### AC-050-3: Only ball and W blocks are dynamic
```gherkin
Given the demo scene is loaded
When the physics properties are examined
Then the object "ball" has body_type DYNAMIC
And objects matching "w_block_*" have body_type DYNAMIC
And objects for letters n, a, v, e have body_type STATIC (default, no physics block)
```

---

## US-SPA-051: Render Progress Bar with ETA

### AC-051-1: Frame count and percentage displayed
```gherkin
Given a 150-frame animation render
When frame 30 completes
Then the output line contains "30/150" or "30 / 150"
And the output line contains "20%"
```

### AC-051-2: ETA decreases over time
```gherkin
Given a render where each frame takes approximately 5 seconds
When frame 10 of 150 completes
Then the displayed ETA is approximately 700 seconds (140 * 5)
When frame 75 of 150 completes
Then the displayed ETA is approximately 375 seconds (75 * 5)
And the second ETA is less than the first
```

### AC-051-3: Completion shows total time
```gherkin
Given all 150 frames have rendered
When the final output line is printed
Then it contains "Done" or "Complete"
And it contains the total elapsed time in minutes and seconds
```
