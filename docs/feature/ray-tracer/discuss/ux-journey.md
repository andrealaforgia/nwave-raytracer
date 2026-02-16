# UX Journey: Scene Definition to Rendered Image

**Journey**: A developer defines a 3D scene, renders it, iterates, and exports a final image.
**Persona**: Developer/technical user comfortable with config files and CLI tools.
**Epic**: ray-tracer

---

## Journey Overview

```
[1. Define Scene] --> [2. Validate Scene] --> [3. Render] --> [4. View Result] --> [5. Iterate] --> [6. Export]
      |                      |                    |                |                   |               |
   Author a             Catch errors          Watch progress    See the image     Tweak and        Save final
   scene file           before burning        and feel in       and judge it      re-render        artifact
                        render time           control                             quickly
```

---

## Step 1: Define the Scene

**What the user does**: Creates or edits a scene description file (YAML or custom DSL) that declares objects, materials, lights, and a camera.

**Input**: Text editor of choice + scene file format documentation.
**Output**: A scene file on disk (e.g., `scenes/cornell-box.yaml`).

**Example scene file (target format)**:

```yaml
# scenes/cornell-box.yaml
image:
  width: 800
  height: 600
  samples_per_pixel: 100
  max_depth: 50

camera:
  position: [278, 278, -800]
  look_at: [278, 278, 0]
  up: [0, 1, 0]
  fov: 40
  aperture: 0.0        # pinhole (no depth-of-field)
  focus_distance: 10.0

materials:
  red:
    type: lambertian
    albedo: [0.65, 0.05, 0.05]
  white:
    type: lambertian
    albedo: [0.73, 0.73, 0.73]
  green:
    type: lambertian
    albedo: [0.12, 0.45, 0.15]
  light:
    type: emissive
    color: [15, 15, 15]
  glass:
    type: dielectric
    ior: 1.5
  mirror:
    type: metal
    albedo: [0.8, 0.8, 0.9]
    fuzziness: 0.0

objects:
  - type: plane
    name: left-wall
    point: [555, 0, 0]
    normal: [-1, 0, 0]
    material: green

  - type: plane
    name: right-wall
    point: [0, 0, 0]
    normal: [1, 0, 0]
    material: red

  - type: sphere
    name: glass-orb
    center: [190, 90, 190]
    radius: 90
    material: glass

lights:
  - type: area
    shape: rect
    position: [213, 554, 227]
    size: [130, 0, 105]
    material: light
```

**Shared artifacts produced**:
- `scene_file_path` -- the path to the scene file on disk
- `image.width`, `image.height` -- used by renderer and output
- `samples_per_pixel`, `max_depth` -- render quality parameters
- `materials.*` -- referenced by name from objects

**Design decisions**:
- YAML chosen over custom DSL for approachability -- developers already know it.
- Materials are declared separately and referenced by name to avoid duplication.
- Render settings (`image`, `camera`) live in the same file so each scene is self-contained.
- Named objects (`name: glass-orb`) for clear error messages and log output.

---

## Step 2: Validate the Scene

**What the user does**: Runs a validation command before committing to a render.

**Input**: Scene file path.
**Output**: Validation report (pass/fail with specifics).

**CLI interaction**:

```
$ nwave validate scenes/cornell-box.yaml

Validating scenes/cornell-box.yaml...

  Scene structure     OK
  Camera              OK   (pinhole, fov=40)
  Materials (6)       OK
  Objects (3)         OK
  Lights (1)          OK
  References          OK   (all material refs resolve)

Scene valid. Ready to render.
  Objects: 3  |  Materials: 6  |  Lights: 1
  Output: 800x600  |  Samples: 100  |  Max depth: 50
```

**Error case -- broken reference**:

```
$ nwave validate scenes/broken.yaml

Validating scenes/broken.yaml...

  Scene structure     OK
  Camera              OK
  Materials (4)       OK
  Objects (5)         FAIL
    - Object "tall-box" references material "chrome" which is not defined.
      Defined materials: red, white, green, light
  Lights (1)          OK

Scene invalid. 1 error found.
```

**Error case -- structural problem**:

```
$ nwave validate scenes/bad-camera.yaml

Validating scenes/bad-camera.yaml...

  Scene structure     OK
  Camera              FAIL
    - "fov" must be between 1 and 179 degrees (got: 200)
  ...

Scene invalid. 1 error found.
```

**Shared artifacts consumed**: `scene_file_path`
**Shared artifacts produced**: validated scene (in-memory structure ready for render)

**Design decisions**:
- Validation is a separate explicit step so the user catches errors cheaply (zero render time wasted).
- Summary line at the end gives a quick scene overview before committing to render.
- Error messages name the specific object and suggest fixes (list available materials).
- Checklist format (OK/FAIL per category) gives scannable confidence.

---

## Step 3: Render the Scene

**What the user does**: Runs the render command and watches progress.

**Input**: Scene file path (or validated scene).
**Output**: Rendered image file + render statistics.

**CLI interaction -- happy path**:

```
$ nwave render scenes/cornell-box.yaml

nwave ray tracer v0.1.0
Scene: scenes/cornell-box.yaml
Output: renders/cornell-box.ppm (800x600, 100 spp, depth 50)

Building BVH... done (3 primitives, 0.002s)

Rendering [=================>              ] 58%  row 348/600  elapsed 12.4s  eta 8.9s
```

After completion:

```
Rendering [================================] 100%  row 600/600  elapsed 21.3s

Render complete.
  Output:     renders/cornell-box.ppm
  Resolution: 800 x 600
  Samples:    100 per pixel
  Time:       21.3s (28,176 rays/ms)
  BVH:        5 nodes, 3 primitives
```

**CLI interaction -- override flags**:

```
$ nwave render scenes/cornell-box.yaml --output renders/test.png --samples 10 --width 400 --height 300

nwave ray tracer v0.1.0
Scene: scenes/cornell-box.yaml
Output: renders/test.png (400x300, 10 spp, depth 50)
Note: --samples and --width/--height override scene file values.

Building BVH... done (3 primitives, 0.001s)

Rendering [================================] 100%  row 300/300  elapsed 0.8s

Render complete.
  Output:     renders/test.png
  Resolution: 400 x 300
  Samples:    10 per pixel
  Time:       0.8s (15,000 rays/ms)
```

**Error case -- render failure**:

```
$ nwave render scenes/broken.yaml

nwave ray tracer v0.1.0
Error: Scene validation failed.
  - Object "tall-box" references material "chrome" which is not defined.

Run `nwave validate scenes/broken.yaml` for details.
```

**Shared artifacts consumed**: `scene_file_path`, all scene parameters
**Shared artifacts produced**: `output_image_path`, render statistics

**Design decisions**:
- Implicit validation before render -- never burn time on a broken scene.
- Progress bar with row count, elapsed time, and ETA -- the user always knows where they stand.
- Output path defaults to `renders/{scene-name}.ppm` (predictable convention).
- CLI flags override scene file values for quick iteration (low-sample preview renders).
- Statistics at the end build confidence ("my render is working correctly") and help tune.
- PPM as default format (trivially simple, zero dependencies); PNG when requested via `--output *.png`.
- BVH build step shown separately so the user understands the two phases.

---

## Step 4: View the Result

**What the user does**: Opens the rendered image in their preferred viewer.

**Input**: Path to rendered image (printed by render command).
**Output**: Visual assessment of the image.

**CLI interaction**:

```
$ nwave render scenes/cornell-box.yaml
...
Render complete.
  Output: renders/cornell-box.ppm
```

The user then opens the image with their OS viewer, or:

```
$ open renders/cornell-box.ppm          # macOS
$ xdg-open renders/cornell-box.ppm     # Linux
```

**Optional convenience** (not in initial version):

```
$ nwave render scenes/cornell-box.yaml --open
# renders, then auto-opens the result
```

**Design decisions**:
- The ray tracer is a CLI tool -- it produces a file, not a GUI window.
- The output path is always printed clearly so the user can copy-paste it.
- `--open` flag is a future convenience, not a priority. The tool does one thing well: produce an image file.
- No built-in viewer. That is not what this tool does.

---

## Step 5: Iterate

**What the user does**: Edits the scene file, re-renders (often with lower samples for speed), compares results, repeats.

**Iteration workflow**:

```
# 1. Quick preview render (low samples, small resolution)
$ nwave render scenes/cornell-box.yaml --samples 4 --width 200 --height 150
Render complete. Output: renders/cornell-box.ppm  Time: 0.1s

# 2. Edit the scene file (move the sphere, change material)
$ vim scenes/cornell-box.yaml

# 3. Re-render preview
$ nwave render scenes/cornell-box.yaml --samples 4 --width 200 --height 150
Render complete. Output: renders/cornell-box.ppm  Time: 0.1s

# 4. Satisfied -- full quality render
$ nwave render scenes/cornell-box.yaml --samples 500
Render complete. Output: renders/cornell-box.ppm  Time: 108.7s
```

**Design decisions**:
- CLI flag overrides make iteration fast -- no need to edit the scene file to change sample count.
- Same output path by default means the viewer can be left open and refreshed (for viewers that support it).
- The iteration loop is: edit scene file -> render at low quality -> view -> repeat -> render at full quality. The tool supports this loop with minimal friction.
- No "watch mode" or auto-reload in initial version. Explicit re-render keeps the user in control.

---

## Step 6: Export the Final Image

**What the user does**: Renders at full quality and optionally converts to a different format.

**Input**: Scene file + desired output format.
**Output**: Final image file in target format.

**CLI interaction**:

```
$ nwave render scenes/cornell-box.yaml --samples 1000 --output renders/cornell-box-final.png

nwave ray tracer v0.1.0
Scene: scenes/cornell-box.yaml
Output: renders/cornell-box-final.png (800x600, 1000 spp, depth 50)

Building BVH... done (3 primitives, 0.002s)

Rendering [================================] 100%  row 600/600  elapsed 213.5s

Render complete.
  Output:     renders/cornell-box-final.png
  Resolution: 800 x 600
  Samples:    1000 per pixel
  Time:       213.5s (22,491 rays/ms)
```

**Supported output formats** (determined by file extension):
- `.ppm` -- PPM (default, zero-dependency)
- `.png` -- PNG (requires image writing library)

**Design decisions**:
- Format determined by output file extension -- no separate `--format` flag needed.
- PPM is always available (no dependencies). PNG requires a library but is the practical default for sharing.
- No separate "export" command. Rendering to a named file IS the export. One command, one output.

---

## Shared Artifact Registry

| Artifact | Source (Step) | Consumed By (Steps) | Single Source of Truth |
|---|---|---|---|
| `scene_file_path` | Step 1 (user creates file) | Steps 2, 3, 5 | Filesystem path, passed as CLI argument |
| `image.width` / `image.height` | Step 1 (scene file) | Step 3 (renderer), Step 6 (output) | Scene file; overridable via `--width`/`--height` |
| `samples_per_pixel` | Step 1 (scene file) | Step 3 (renderer) | Scene file; overridable via `--samples` |
| `max_depth` | Step 1 (scene file) | Step 3 (renderer) | Scene file; overridable via `--depth` |
| `materials` map | Step 1 (scene file) | Step 2 (validation), Step 3 (renderer) | Scene file, referenced by name |
| `output_image_path` | Step 3 (renderer, default or `--output`) | Step 4 (user views), Step 6 (final export) | Derived from scene name or explicit `--output` flag |
| `render_statistics` | Step 3 (renderer) | Step 4 (user reads terminal output) | Computed during render, printed to stdout |
| `validated_scene` (in-memory) | Step 2 (validator) | Step 3 (renderer, implicit re-validation) | Parsed + validated scene structure |

---

## CLI Command Summary

| Command | Purpose | Key Flags |
|---|---|---|
| `nwave validate <scene>` | Validate scene file without rendering | (none) |
| `nwave render <scene>` | Render scene to image | `--output`, `--samples`, `--width`, `--height`, `--depth`, `--open` |

---

## Error Paths Summary

| Error | When | User Sees | Recovery |
|---|---|---|---|
| Scene file not found | validate, render | `Error: File not found: scenes/foo.yaml` | Fix the path |
| YAML parse error | validate, render | `Error: Parse error at line 12: expected mapping` | Fix syntax in editor |
| Undefined material ref | validate, render | `Object "X" references material "Y" which is not defined. Defined materials: ...` | Add the material or fix the typo |
| Invalid parameter range | validate, render | `"fov" must be between 1 and 179 (got: 200)` | Fix the value |
| No objects in scene | validate, render | `Scene has no objects. Nothing to render.` | Add objects |
| No lights in scene | validate (warning), render | `Warning: Scene has no lights. Render will be dark unless objects emit light.` | Add lights or emissive materials |
| Output directory missing | render | `Error: Output directory "renders/" does not exist. Create it?` | Create the directory |
| Render interrupted (Ctrl+C) | render | `Render interrupted at row 348/600. Partial output saved to renders/cornell-box.ppm` | Re-run render or use partial result |
