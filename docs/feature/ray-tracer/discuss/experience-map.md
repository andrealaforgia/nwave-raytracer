# Experience Map: Ray Tracer Developer Journey

**Journey**: Scene definition to final rendered image.
**Persona**: Developer/technical user.
**Epic**: ray-tracer

---

## Emotional Arc

```
Confidence
    ^
    |                                                              * Export
    |                                                         ****
    |                                           * View   ****
    |                                      *****    *****
    |                          * Render ***     Iterate
    |                     *****
    |           * Validate
    |      *****
    |  ****
    | *
    * Define
    |
    +-------------------------------------------------------------------> Time

    Curious/       Growing         In control      Satisfied       Confident
    Exploratory    confidence      + patient        + critical      + done
```

---

## Touchpoint Map

### 1. Define Scene

| Dimension | Detail |
|---|---|
| **Action** | Create/edit a YAML scene file in a text editor |
| **Touchpoint** | Scene file format (the "API" of the ray tracer) |
| **Thinking** | "What objects do I want? Where should the camera go? What materials look right?" |
| **Feeling** | Curious, exploratory, slightly uncertain about coordinates/parameters |
| **Pain points** | Guessing 3D coordinates without visual feedback; not knowing valid material types; forgetting required fields |
| **Opportunity** | Example scenes shipped with the tool; clear format documentation; schema validation in editors (YAML schema) |

### 2. Validate Scene

| Dimension | Detail |
|---|---|
| **Action** | Run `nwave validate scene.yaml` |
| **Touchpoint** | CLI validation output |
| **Thinking** | "Did I get the format right? Are my references correct?" |
| **Feeling** | Brief anticipation, then relief (pass) or mild frustration (fail) |
| **Pain points** | Cryptic error messages; errors that require re-reading the docs to understand |
| **Opportunity** | Actionable errors that name the problem AND suggest a fix; checklist-style output that shows what passed (not just what failed) |

### 3. Render

| Dimension | Detail |
|---|---|
| **Action** | Run `nwave render scene.yaml`, watch progress |
| **Touchpoint** | CLI progress bar, elapsed/ETA timers |
| **Thinking** | "How long will this take? Is it working?" |
| **Feeling** | Patient but alert; in control because progress is visible; growing anticipation |
| **Pain points** | Long renders with no progress indication; not knowing if the render is stuck or just slow; unable to cancel gracefully |
| **Opportunity** | Clear progress bar with ETA; graceful Ctrl+C with partial output saved; low-sample preview renders for fast feedback |

### 4. View Result

| Dimension | Detail |
|---|---|
| **Action** | Open the rendered image in an image viewer |
| **Touchpoint** | The rendered image itself |
| **Thinking** | "Does this look right? Is the lighting what I expected? Are the materials correct?" |
| **Feeling** | Critical assessment; satisfaction if it looks good, mild disappointment if it does not |
| **Pain points** | PPM format not supported by all viewers; image too dark (gamma issue); not sure if artifacts are bugs or just low sample count |
| **Opportunity** | PNG output for universal viewing; correct gamma handling by default; render stats printed alongside (so user knows "this was only 10 samples, noise is expected") |

### 5. Iterate

| Dimension | Detail |
|---|---|
| **Action** | Edit the scene file, re-render at low quality, compare, repeat |
| **Touchpoint** | Edit-render-view loop (multiple CLI invocations) |
| **Thinking** | "Let me move this sphere. Let me try a different material. What if the light is brighter?" |
| **Feeling** | Engaged, experimental, increasingly confident with each iteration |
| **Pain points** | Slow iteration if every render takes minutes; having to edit the scene file to change sample count; losing track of which render corresponds to which change |
| **Opportunity** | CLI flags (`--samples 4 --width 200`) for instant preview renders; predictable output naming; fast BVH rebuild |

### 6. Export Final Image

| Dimension | Detail |
|---|---|
| **Action** | Run a full-quality render with `--output final.png --samples 1000` |
| **Touchpoint** | Final CLI render output + image file on disk |
| **Thinking** | "This is the final version. Let me render at high quality." |
| **Feeling** | Confidence in the scene (validated through iteration); patience for a long render; satisfaction when done |
| **Pain points** | Very long render times for high sample counts; uncertainty about whether higher samples will meaningfully improve quality |
| **Opportunity** | Print estimated render time before starting; show quality statistics (noise level) in output |

---

## Key Design Principles (derived from emotional arc)

### 1. Never waste render time on a broken scene
Validation is cheap. Rendering is expensive. The tool should fail fast on invalid input and never begin ray tracing an invalid scene. This prevents the most frustrating experience: waiting minutes for a render only to get a crash or garbage output.

### 2. Progress visibility at all times
During rendering, the user must always know: how far along, how long elapsed, how long remaining. A render with no progress output creates anxiety. A render with clear progress creates patience.

### 3. Fast iteration over perfect first attempts
The most common workflow is not "define scene, render once, done." It is "define, preview, tweak, preview, tweak, preview, ..., final render." Every design decision should optimize for the iteration loop: CLI flag overrides for quick changes, low-sample previews in under a second, predictable output paths.

### 4. Errors that teach
Every error message should: (a) name what went wrong, (b) name where it went wrong, and (c) suggest how to fix it. "Material 'chrome' not defined" is bad. "Object 'tall-box' references material 'chrome' which is not defined. Defined materials: red, white, green, light" is good.

### 5. The scene file is the single source of truth
All scene data lives in one file. The CLI provides overrides for iteration convenience (`--samples`, `--width`) but does not introduce state that lives outside the scene file. When the user shares a scene file, the recipient can reproduce the exact same render.

---

## Integration Checkpoints

These are the points where data flows between steps and where integration failures are most likely.

| Checkpoint | From | To | What to verify |
|---|---|---|---|
| Scene file parse | Step 1 | Step 2 | YAML parses correctly; all required fields present |
| Material references | Step 1 | Step 2 | Every object's `material` field resolves to a defined material name |
| Render parameters | Step 1 + CLI flags | Step 3 | CLI overrides merge correctly with scene file defaults; no conflicting values |
| Output path | Step 3 | Step 4, 6 | Output directory exists; format matches extension; path printed to stdout |
| Scene re-load | Step 5 | Step 3 | Edited scene file re-parses correctly; validation runs again before render |

---

## Scope Boundaries

**In scope for this journey**:
- Scene definition via YAML file
- CLI validation of scene files
- CLI rendering with progress feedback
- Image output (PPM, PNG)
- CLI flag overrides for iteration speed

**Out of scope (future journeys)**:
- Interactive/GUI scene editor
- Real-time preview rendering
- Animation and keyframe support
- Network rendering / distributed compute
- Scene file version control integration
- Texture image loading (UV-mapped image textures)
