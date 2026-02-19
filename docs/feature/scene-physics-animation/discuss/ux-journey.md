# UX Journey: Scene Physics Animation

**Journey ID**: UXJ-SPA-001
**Feature**: scene-physics-animation
**Persona**: C++ developer who wants physics-driven animated videos from YAML scenes
**Date**: 2026-02-17
**Status**: Draft

---

## Journey Summary

The user writes a YAML scene file that defines geometry, materials, lights, camera, and physics properties. They run a single CLI command. The system loads the scene, validates it, simulates physics, renders each frame, and produces a video. The user watches the video to see their physics scenario play out.

**Trigger**: The user has an idea for a physics-based animation (e.g., a ball hitting letters that collapse under gravity).

**Success Criteria**: A playable MP4 video showing the physics animation, produced from a single CLI invocation with no intermediate manual steps.

---

## Emotional Arc

```
Curiosity     Confidence     Anticipation     Patience       Satisfaction
 "Can I          "The           "Physics          "Frames        "That
  describe       scene is       looks right,      rendering,     looks
  this in        valid!"        now render"       progress       exactly
  YAML?"                                          is clear"      right!"
    |               |               |               |               |
    v               v               v               v               v
  [Author]------>[Validate]---->[Simulate]------>[Render]------>[Watch]
   Step 1          Step 2         Step 3          Step 4         Step 5
```

The arc builds from exploratory curiosity (authoring) through confirmation (validation), anticipation (simulation preview), patient confidence (rendering progress), to payoff (watching the result). There are no jarring transitions -- each step provides enough feedback to sustain momentum.

---

## Step 1: Author the Scene (YAML)

**What the user does**: Opens a text editor, writes a YAML file defining geometry with physics properties.

**What they feel**: Curiosity mixed with slight uncertainty. "Am I getting the physics properties right? What does restitution 0.7 actually mean?"

**Key design decisions**:
- Physics properties are an optional block on each object. Objects without a `physics:` block default to `static` (safe default -- things stay put unless you say otherwise).
- The `animation:` section at the top level declares duration, timestep, and output fps.
- Material and physics are separate concerns on the same object. Material controls appearance, physics controls motion.

**Shared artifacts produced**: `scene.yaml` file path (consumed by all subsequent steps).

**Example YAML (the "nWave bowling" scenario)**:

```yaml
scene:
  gravity: [0, -9.81, 0]

  materials:
    - name: red_rubber
      type: lambertian
      albedo: [0.85, 0.15, 0.15]

    - name: green_glass
      type: dielectric
      ior: 1.5
      tint: [0.4, 0.95, 0.4]

    - name: floor_metal
      type: metal
      albedo: [0.9, 0.9, 0.9]
      fuzz: 0.05

  objects:
    # Ground
    - name: floor
      type: plane
      point: [0, 0, 0]
      normal: [0, 1, 0]
      material: floor_metal
      physics:
        body_type: static

    # Rolling ball
    - name: ball
      type: sphere
      center: [-5, 0.5, 0]
      radius: 0.5
      material: red_rubber
      physics:
        body_type: dynamic
        mass: 2.0
        initial_velocity: [8, 0, 0]
        friction: 0.3
        restitution: 0.4

    # W letter blocks (each block is dynamic, glass)
    - name: w_block_0
      type: box
      min: [0.0, 0.0, 0.5]
      max: [0.12, 0.12, 0.62]
      material: green_glass
      physics:
        body_type: dynamic
        mass: 0.3
        friction: 0.5
        restitution: 0.2
    # ... (more blocks composing the W)

  lights:
    - type: point
      position: [-4, 10, 2]
      color: [1.0, 0.97, 0.9]
      intensity: 0.7

  camera:
    lookfrom: [2, 3, 6]
    lookat: [0, 0.3, 0.5]
    vup: [0, 1, 0]
    vfov: 38

  animation:
    duration: 5.0
    physics_timestep: 0.01667
    render_fps: 30
    output_directory: frames/
```

**Error paths**:
- Typo in material name: caught at validation (Step 2).
- Missing `animation:` section when using `--physics-animate`: caught at validation with clear message.
- Invalid physics property (negative mass): caught at validation.

---

## Step 2: Validate the Scene

**What the user types**:
```
nwave validate scene.yaml
```

**What they see (happy path)**:
```
Validating scene.yaml...

  Scene structure     [OK]
  Materials (3)       [OK]
  Objects (26)        [OK]  (1 static, 25 dynamic)
  Material references [OK]
  Physics properties  [OK]
  Animation config    [OK]  (5.0s, 30 fps = 150 frames)
  Lights (1)          [OK]
  Camera              [OK]

Scene is valid. Ready to render.
```

**What they feel**: Confidence. The checklist format gives immediate reassurance that the scene is well-formed. The summary "(1 static, 25 dynamic)" confirms the physics setup matches their mental model. The frame count "150 frames" grounds the duration in concrete output.

**Shared artifacts consumed**: `scene.yaml` file path.
**Shared artifacts produced**: Validated scene structure (in-memory, passed to next step).

**Error path example**:
```
Validating scene.yaml...

  Scene structure     [OK]
  Materials (3)       [OK]
  Objects (26)        [FAIL]
    - Object "ball" (line 22): physics.mass must be positive, got -2.0
    - Object "w_block_0" (line 30): unknown material "geen_glass"
      Did you mean: "green_glass"?
  Material references [FAIL]

2 errors found. Fix and re-validate.
```

**Error path emotion**: Brief frustration, quickly resolved. The error messages are specific (line number, field name, suggested fix). The user knows exactly what to change.

---

## Step 3: Run Physics + Render

**What the user types**:
```
nwave render scene.yaml --physics-animate
```

**What they see (Phase 1: Physics simulation)**:
```
Loading scene.yaml...
  26 objects, 1 light, 25 dynamic bodies

Simulating physics (5.0s at 60Hz)...
  [========================================] 300/300 steps (0.4s)

Physics summary:
  Active bodies at end: 18/25
  Total collisions: 47
  Bodies at rest: 7
```

**What they feel**: Anticipation building. The physics simulation is fast (sub-second for typical scenes) so the wait is brief. The summary confirms things happened -- 47 collisions means the ball hit the W and blocks scattered. "Active bodies at end: 18/25" tells them some blocks have settled. This is the moment of "did my scene idea work?" and the summary gives them an early signal before the long render.

**Shared artifacts consumed**: Validated scene, `--physics-animate` flag.
**Shared artifacts produced**: Per-frame transforms (internal), frame count, output directory.

**What they see (Phase 2: Frame rendering)**:
```
Rendering 150 frames (800x450, 16 SPP)...
  Frame 1/150   [====                                    ]   1%  ETA: 12m 30s
```

Then, as rendering progresses:
```
  Frame 75/150  [====================                    ]  50%  ETA: 6m 15s
```

And at completion:
```
  Frame 150/150 [========================================] 100%  Done (12m 48s)

Frames saved to frames/
  frames/frame_0000.ppm ... frames/frame_0149.ppm

To create video:
  ffmpeg -framerate 30 -i frames/frame_%04d.ppm -c:v libx264 -pix_fmt yuv420p output.mp4
```

**What they feel**: Patient confidence during rendering. The progress bar with ETA removes uncertainty -- they know how long to wait. The frame count ticking up provides a steady pulse of progress. At completion, satisfaction: the work is done, files are on disk, and the next step (ffmpeg) is handed to them as a copy-paste command.

**Error paths**:
- Output directory not writable: fail fast before rendering starts with clear message.
- Disk full mid-render: report which frame failed, frames already written are preserved.
- Scene too large for available memory: report memory estimate vs available at load time.

---

## Step 4: Produce Video

**What the user types** (copy-paste from CLI output):
```
ffmpeg -framerate 30 -i frames/frame_%04d.ppm -c:v libx264 -pix_fmt yuv420p output.mp4
```

**What they see**: ffmpeg's standard output (frame encoding progress).

**What they feel**: Routine confidence. This is a well-known tool doing a well-known job. The copy-paste command removes friction.

**Shared artifacts consumed**: Frame images in `frames/` directory, framerate from animation config.
**Shared artifacts produced**: `output.mp4` video file.

**Design note**: The ffmpeg command is not built into nwave. This is deliberate: (a) ffmpeg is a large dependency the ray tracer should not own, (b) users may want different codecs/formats, (c) providing the command as a suggestion respects user autonomy. A future enhancement could add `--video` flag that invokes ffmpeg if available, but that is out of scope for this journey.

---

## Step 5: Watch the Result

**What the user does**: Opens `output.mp4` in any video player.

**What they feel**: Satisfaction (if the physics looks right) or creative iteration instinct ("the ball should be faster", "the blocks should be heavier"). Either way, they know what to tweak in the YAML and can re-run the pipeline.

**Iteration loop**: The user returns to Step 1 (edit YAML) or Step 3 (re-run with adjusted parameters). The fast physics simulation phase (sub-second) means iteration on physics properties is quick -- the bottleneck is rendering, which can be mitigated by reducing resolution/SPP during iteration.

**Quick iteration tip** (surfaced by CLI `--help`):
```
nwave render scene.yaml --physics-animate --width 400 --spp 4
```
Low-quality preview for physics iteration: ~1-2 minutes instead of 12.

---

## Shared Artifact Registry

| Artifact | Source (Step) | Consumed By (Steps) | Format | Single Source of Truth |
|---|---|---|---|---|
| `scene.yaml` | Step 1 (Author) | Steps 2, 3 | YAML file on disk | User's text editor |
| Scene file path | CLI argument | Steps 2, 3, 4 (output dir) | String | CLI argv |
| Validated scene | Step 2 (Validate) | Step 3 (implicitly, via load+validate in render) | In-memory Scene object | SceneLoader + Validator |
| Physics transforms | Step 3 (Simulate) | Step 3 (Render phase) | In-memory per-frame transform array | PhysicsSimulator |
| Frame images | Step 3 (Render) | Step 4 (ffmpeg) | PPM files in output directory | ImageWriter |
| ffmpeg command | Step 3 (CLI output) | Step 4 (user copy-paste) | String printed to stdout | Hardcoded template in CLI |
| Output video | Step 4 (ffmpeg) | Step 5 (Watch) | MP4 file | ffmpeg |
| `animation.duration` | YAML `animation:` section | Physics sim (total time), Render (frame count) | float seconds | scene.yaml |
| `animation.render_fps` | YAML `animation:` section | Render (frame count), ffmpeg command (framerate) | integer | scene.yaml |
| `animation.physics_timestep` | YAML `animation:` section | Physics sim (step size) | float seconds | scene.yaml |
| `animation.output_directory` | YAML `animation:` section | Render (file output), ffmpeg command (input path) | directory path | scene.yaml |

---

## Integration Checkpoints

These are points where data crosses boundaries. Each is a potential integration failure.

| Checkpoint | From | To | Validation |
|---|---|---|---|
| YAML parse to domain objects | SceneLoader (Ring 4) | Scene (Ring 2) | All material refs resolve; all physics props valid; animation section present |
| Domain objects to physics bodies | Scene (Ring 2) | PhysicsSimulator (Ring 3/4) | Every dynamic/kinematic object maps to a supported collision shape; mass > 0 for dynamic |
| Physics transforms back to scene | PhysicsSimulator (Ring 4) | Scene shapes (Ring 2) | Transform count matches dynamic body count; no NaN in positions/rotations |
| Scene per frame to renderer | Scene (Ring 2) | Renderer (Ring 3) | Shape positions updated before render call; camera unchanged across frames |
| Rendered pixels to disk | Renderer (Ring 3) | ImageWriter (Ring 4) | Frame filename sequential; output directory exists and writable |
| Frame naming to ffmpeg | CLI output (Ring 4) | External ffmpeg | Frame numbering pattern matches printf format in suggested command; fps matches |

---

## CLI Vocabulary

Consistent terminology across all user-facing output:

| Term | Meaning | Used In |
|---|---|---|
| `object` | A geometric shape in the scene | Validation output, scene summary |
| `body` | A physics-simulated object (subset of objects) | Physics summary |
| `dynamic` | A body affected by physics forces | Validation, physics summary |
| `static` | A body that never moves | Validation |
| `frame` | A single rendered image | Render progress, file output |
| `step` | A single physics timestep advance | Physics progress |
| `SPP` | Samples per pixel (anti-aliasing quality) | Render settings output |
| `ETA` | Estimated time to completion | Render progress bar |

---

## Future Considerations (Out of Scope)

These emerged during journey design but are explicitly deferred:

1. **Built-in video encoding** (`--video` flag): Requires ffmpeg as dependency or optional runtime detection. Defer to a separate journey.
2. **Physics preview mode** (text-based summary of object trajectories before rendering): Could help users iterate faster. Separate feature.
3. **Kinematic animation paths** (scripted camera or object movement via keyframes): Extends the YAML format significantly. Separate feature.
4. **Scene includes** (`!include letter_w.yaml`): Composing large scenes from parts. Separate feature.
5. **Parallel frame rendering** (render multiple frames simultaneously): Performance optimization. Separate feature.
