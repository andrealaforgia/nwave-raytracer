# Experience Map: Scene Physics Animation

**Map ID**: XM-SPA-001
**Journey**: UXJ-SPA-001
**Date**: 2026-02-17
**Status**: Draft

---

## Journey Flow

```
+============+     +==========+     +===========+     +=========+     +=======+
| 1. AUTHOR  |---->| 2. VALID |---->| 3a. PHYS  |---->| 3b. REN |---->| 4. VID|---->[ 5. WATCH ]
|   (YAML)   |     |   ATE    |     |   SIM     |     |   DER   |     |  EO   |
+============+     +==========+     +===========+     +=========+     +=======+
                         |                                                 |
                         |   error                                         |
                         +----> FIX -----+                                 |
                                         |                                 |
                                         +--- back to AUTHOR <------------+
                                                (iterate)
```

---

## Step-by-Step Experience

### Step 1: Author Scene (User's Editor)

```
TRIGGER: User has a physics animation idea

ACTION:  Write YAML scene file

OUTPUT:  scene.yaml on disk
```

**TUI**: None (external text editor). The YAML format is the interface.

**YAML structure overview**:
```yaml
scene:
  gravity: [x, y, z]            # World-level physics

  materials:                     # Appearance definitions
    - name: ${material_name}
      type: lambertian|metal|dielectric|emissive
      ...material-specific props...

  objects:                       # Geometry + physics
    - name: ${object_name}       # Optional, used in diagnostics
      type: sphere|box|plane|cylinder|triangle
      ...shape-specific geometry...
      material: ${material_name} # Reference by name
      physics:                   # Optional block
        body_type: static|dynamic|kinematic
        mass: ${float}
        initial_velocity: [x, y, z]
        friction: ${float}
        restitution: ${float}

  lights:                        # Illumination
    - type: point|directional
      ...light-specific props...

  camera:                        # Viewpoint
    lookfrom: [x, y, z]
    lookat: [x, y, z]
    vup: [0, 1, 0]
    vfov: ${degrees}

  animation:                     # Physics animation config
    duration: ${seconds}
    physics_timestep: ${seconds}
    render_fps: ${integer}
    output_directory: ${path}
```

**Design rationale**:
- `physics:` block is optional per object. Omitting it means static (safe default).
- `name:` on objects is optional but recommended -- used in validation errors and physics summary.
- `animation:` section is required only when `--physics-animate` is used.
- Gravity is at scene level, not per-object. Per-object gravity override via `gravity_enabled: false`.

---

### Step 2: Validate

```
$ nwave validate scene.yaml
```

**Happy path TUI mockup**:
```
+----------------------------------------------------------------------+
| Validating scene.yaml...                                             |
|                                                                      |
|   Scene structure     [OK]                                           |
|   Materials (3)       [OK]                                           |
|   Objects (26)        [OK]  (1 static, 25 dynamic)                   |
|   Material references [OK]                                           |
|   Physics properties  [OK]                                           |
|   Animation config    [OK]  (5.0s, 30 fps = 150 frames)             |
|   Lights (1)          [OK]                                           |
|   Camera              [OK]                                           |
|                                                                      |
| Scene is valid. Ready to render.                                     |
+----------------------------------------------------------------------+
```

**Error path TUI mockup**:
```
+----------------------------------------------------------------------+
| Validating scene.yaml...                                             |
|                                                                      |
|   Scene structure     [OK]                                           |
|   Materials (3)       [OK]                                           |
|   Objects (26)        [FAIL]                                         |
|     - "ball" (line 22): physics.mass must be positive, got -2.0      |
|     - "w_block_0" (line 30): unknown material "geen_glass"           |
|       Did you mean: "green_glass"?                                   |
|   Material references [FAIL]                                         |
|                                                                      |
| 2 errors found. Fix and re-validate.                                 |
+----------------------------------------------------------------------+
```

**Validation rules for physics**:
- `body_type` must be one of: `static`, `dynamic`, `kinematic`
- `mass` must be > 0 for dynamic bodies (ignored for static)
- `friction` must be in [0, 1]
- `restitution` must be in [0, 1]
- `initial_velocity` must be a 3-element array
- If `--physics-animate` is used, `animation:` section must be present
- `animation.duration` must be > 0
- `animation.physics_timestep` must be > 0 and <= duration
- `animation.render_fps` must be > 0
- `animation.output_directory` must be a valid path

---

### Step 3a: Physics Simulation

```
$ nwave render scene.yaml --physics-animate
```

**TUI mockup (physics phase)**:
```
+----------------------------------------------------------------------+
| Loading scene.yaml...                                                |
|   26 objects, 1 light, 25 dynamic bodies                             |
|                                                                      |
| Simulating physics (5.0s at 60Hz)...                                 |
|   [========================================] 300/300 steps (0.4s)    |
|                                                                      |
| Physics summary:                                                     |
|   Active bodies at end: 18/25                                        |
|   Total collisions: 47                                               |
|   Bodies at rest: 7                                                  |
+----------------------------------------------------------------------+
```

**Data flow**:
```
scene.yaml --(SceneLoader)--> Scene + PhysicsProperties[]
                                  |
                                  v
                         PhysicsSimulator.initialize(scene)
                                  |
                         For step = 0 to 300:
                           PhysicsSimulator.step(dt=0.01667)
                           Store transforms[step]
                                  |
                                  v
                         transforms[300][25] -- per-step, per-body
```

**Key variables**:
- `${total_steps}` = `animation.duration / animation.physics_timestep` = 5.0 / 0.01667 = ~300
- `${dynamic_body_count}` = count of objects with `physics.body_type: dynamic` = 25
- `${steps_per_frame}` = `(1.0 / animation.render_fps) / animation.physics_timestep` = (1/30) / 0.01667 = ~2

---

### Step 3b: Frame Rendering

**TUI mockup (render phase)**:
```
+----------------------------------------------------------------------+
| Rendering 150 frames (800x450, 16 SPP)...                           |
|   Frame   1/150 [==                                      ]   1%     |
|   ETA: 12m 30s                                                       |
+----------------------------------------------------------------------+
```

Mid-render:
```
+----------------------------------------------------------------------+
| Rendering 150 frames (800x450, 16 SPP)...                           |
|   Frame  75/150 [====================                    ]  50%     |
|   ETA: 6m 15s   Elapsed: 6m 33s                                     |
+----------------------------------------------------------------------+
```

Completion:
```
+----------------------------------------------------------------------+
| Rendering 150 frames (800x450, 16 SPP)...                           |
|   Frame 150/150 [========================================] 100%     |
|   Done (12m 48s)                                                     |
|                                                                      |
| Frames saved to frames/                                              |
|   frames/frame_0000.ppm ... frames/frame_0149.ppm                   |
|                                                                      |
| To create video:                                                     |
|   ffmpeg -framerate 30 -i frames/frame_%04d.ppm \                   |
|     -c:v libx264 -pix_fmt yuv420p output.mp4                        |
+----------------------------------------------------------------------+
```

**Data flow per frame**:
```
transforms[frame * steps_per_frame] --(apply to scene)--> updated Scene
                                                              |
                                                              v
                                                     Renderer.render(camera, scene, settings)
                                                              |
                                                              v
                                                     pixel_buffer (800 x 450 x Color3)
                                                              |
                                                              v
                                                     ImageWriter.write("frames/frame_NNNN.ppm")
```

**Key variables**:
- `${total_frames}` = `animation.duration * animation.render_fps` = 5.0 * 30 = 150
- `${image_width}` = from CLI `--width` or default 800
- `${image_height}` = derived from aspect ratio
- `${spp}` = from CLI `--spp` or default 16
- `${output_dir}` = `animation.output_directory`
- `${frame_pattern}` = `frame_%04d.ppm`

---

### Step 4: Video Encoding

```
$ ffmpeg -framerate 30 -i frames/frame_%04d.ppm \
    -c:v libx264 -pix_fmt yuv420p output.mp4
```

**TUI**: ffmpeg's own output (not controlled by nwave).

**Key shared data**: The `framerate` value (30) must match `animation.render_fps` from the YAML. The frame filename pattern must match what the renderer wrote. Both are printed by the CLI in Step 3b, ensuring consistency.

---

### Step 5: Watch Result

User opens `output.mp4` in their video player. If the result needs adjustment, they return to Step 1 (edit YAML).

**Iteration shortcuts**:
```
# Quick physics iteration (low quality, fast render)
$ nwave render scene.yaml --physics-animate --width 400 --spp 4

# Adjust only physics (same scene, different physics params)
# Edit scene.yaml, change mass/velocity/friction, re-run
$ nwave render scene.yaml --physics-animate
```

---

## Complete CLI Interface Map

### Commands

| Command | Description |
|---|---|
| `nwave validate <scene.yaml>` | Validate scene file without rendering |
| `nwave render <scene.yaml>` | Render single frame (existing behavior) |
| `nwave render <scene.yaml> --physics-animate` | Physics simulation + multi-frame render |

### Flags for `--physics-animate`

| Flag | Default | Description |
|---|---|---|
| `--width <pixels>` | 800 | Output image width (height from aspect ratio) |
| `--spp <int>` | 16 | Samples per pixel |
| `--max-depth <int>` | 10 | Max ray recursion depth |
| `--output-dir <path>` | (from YAML) | Override YAML output_directory |
| `--fps <int>` | (from YAML) | Override YAML render_fps |

**Flag precedence**: CLI flags override YAML values. This allows quick iteration without editing the YAML file.

---

## Variable Traceability

Every `${variable}` in the TUI mockups traced to its source:

| Variable | Source | Derivation |
|---|---|---|
| `${object_count}` | scene.yaml `objects:` array | `len(objects)` |
| `${light_count}` | scene.yaml `lights:` array | `len(lights)` |
| `${dynamic_body_count}` | scene.yaml objects with `physics.body_type: dynamic` | count filter |
| `${static_body_count}` | scene.yaml objects with `physics.body_type: static` or no physics block | count filter |
| `${material_count}` | scene.yaml `materials:` array | `len(materials)` |
| `${duration}` | scene.yaml `animation.duration` | direct |
| `${physics_timestep}` | scene.yaml `animation.physics_timestep` | direct |
| `${render_fps}` | scene.yaml `animation.render_fps` or `--fps` flag | CLI override > YAML |
| `${total_steps}` | computed | `ceil(duration / physics_timestep)` |
| `${total_frames}` | computed | `ceil(duration * render_fps)` |
| `${steps_per_frame}` | computed | `round((1.0 / render_fps) / physics_timestep)` |
| `${image_width}` | CLI `--width` or default 800 | CLI override > default |
| `${image_height}` | computed | `image_width / aspect_ratio` |
| `${spp}` | CLI `--spp` or default 16 | CLI override > default |
| `${output_dir}` | scene.yaml `animation.output_directory` or `--output-dir` | CLI override > YAML |
| `${frame_pattern}` | hardcoded | `frame_%04d.ppm` |
| `${physics_sim_time}` | measured | wall-clock time for physics phase |
| `${render_elapsed}` | measured | wall-clock time for render phase |
| `${render_eta}` | computed | `(elapsed / frames_done) * frames_remaining` |
| `${active_bodies_at_end}` | PhysicsSimulator query | count of non-sleeping bodies after last step |
| `${total_collisions}` | PhysicsSimulator accumulated counter | collision callback counter |
| `${bodies_at_rest}` | PhysicsSimulator query | count of sleeping bodies after last step |

---

## Integration Risk Map

| Risk | Severity | Where It Breaks | Mitigation |
|---|---|---|---|
| Physics shape not supported for a geometry type | High | Step 3a: PhysicsSimulator.add_body() | Validate at scene load: if object has `physics: dynamic` and type is `triangle_mesh`, warn that concave dynamic meshes are not supported |
| Frame numbering mismatch between renderer and ffmpeg command | Medium | Step 4: ffmpeg fails to find frames | Single source: frame pattern generated once, used for both file writing and CLI output |
| Rotation not applied (only translation) | High | Step 3b: objects slide without rotating | Requires Matrix4x4 + TransformedShape implementation; physics transforms include quaternion rotation |
| Physics timestep does not divide evenly into render frame time | Low | Step 3b: slight temporal jitter | Use accumulator pattern with interpolation between last two physics states |
| Output directory does not exist | Medium | Step 3b: first frame write fails | Create directory automatically (like current `--animate` does with `std::filesystem::create_directories`) |
| Very large scenes exhaust memory during physics pre-simulation | Low | Step 3a: OOM | For MVP, document scene size limits; future: stream transforms to disk |
| NaN in physics transform propagates to renderer | High | Step 3b: black or corrupted frames | NaN guard after extracting each transform; skip frame or halt with diagnostic |
| User forgets `--physics-animate` flag | Low | Step 3: renders single static frame | If scene has `animation:` section but no `--physics-animate`, print hint: "Scene has animation config. Did you mean --physics-animate?" |

---

## Emotional Journey Annotations

```
Step 1 (Author)      Emotion: Curious, slightly uncertain
                     Design response: Safe defaults (static bodies),
                     clear YAML structure, optional physics block

Step 2 (Validate)    Emotion: Seeking confirmation
                     Design response: Checklist format with [OK]/[FAIL],
                     specific line numbers, "did you mean?" suggestions

Step 3a (Simulate)   Emotion: Anticipation ("did my idea work?")
                     Design response: Fast execution (<1s), collision count
                     as early feedback, active body summary

Step 3b (Render)     Emotion: Patient waiting
                     Design response: Progress bar with ETA, frame counter,
                     elapsed time -- removes uncertainty about wait duration

Step 3b (Complete)   Emotion: Accomplishment
                     Design response: Total time reported, file locations
                     listed, next step (ffmpeg) provided as copy-paste

Step 4 (Video)       Emotion: Routine confidence
                     Design response: Command is pre-built, just paste

Step 5 (Watch)       Emotion: Satisfaction or iteration instinct
                     Design response: Quick iteration path documented
                     (low-res preview with --width 400 --spp 4)
```

---

## Horizontal Coherence Checks

| Check | Status | Notes |
|---|---|---|
| CLI vocabulary consistent across all output | PASS | "objects", "bodies", "frames", "steps" used consistently per glossary |
| Emotional arc has no jarring transitions | PASS | Curiosity -> confirmation -> anticipation -> patience -> satisfaction |
| Shared artifacts have single source of truth | PASS | All traced in registry; no ambiguous sources |
| Error messages include actionable fix guidance | PASS | Line numbers, suggestions, clear next step |
| Progress feedback present at every waiting point | PASS | Physics: progress bar + summary; Render: progress bar + ETA |
| Output of each step feeds cleanly into next step | PASS | YAML -> validate -> simulate -> render -> ffmpeg -> video |
| CLI flags override YAML values (no conflicts) | PASS | Clear precedence: CLI > YAML > defaults |
