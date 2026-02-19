# Experience Map: GPU Compute Rendering (Metal)

**Map ID**: XM-GCR-001
**Journey**: UXJ-GCR-001
**Date**: 2026-02-17
**Status**: Draft

---

## Journey Flow

```
+============+     +==========+     +============+     +=============+     +===========+
| 1. CONFIG  |---->| 2. BUILD |---->| 3. RENDER  |---->| 4. VALIDATE |---->| 5. DAILY  |
|   (CMake)  |     |  (make)  |     |  (--backend|     |  (CPU vs    |     |   USE     |
|            |     |          |     |    gpu)     |     |    GPU)     |     |           |
+============+     +==========+     +============+     +=============+     +===========+
      |                 |                 |                                       |
      | no Metal        | shader err      | runtime err                          |
      v                 v                 v                                      |
   [WARN:           [FIX shader,      [Error msg                                |
    CPU-only         rebuild]          + fallback                                |
    build OK]                           to CPU]                                  |
                                                                                |
                                          +--- Animation pipeline (+--physics-animate --backend gpu)
                                          |
                                          +--- back to Step 1 (if rebuild needed)
```

---

## Step-by-Step Experience

### Step 1: Configure Build (CMake)

```
TRIGGER: Developer wants GPU-accelerated rendering
ACTION:  cmake -B build -DNWAVE_ENABLE_GPU=ON
OUTPUT:  Configured build with Metal detection result
```

**TUI mockup (Metal found)**:
```
+----------------------------------------------------------------------+
| $ cmake -B build -DNWAVE_ENABLE_GPU=ON                              |
|                                                                      |
| -- nwave-raytracer build configuration                               |
| -- C++ Standard: 17                                                  |
| -- Metal GPU support: ENABLED                                        |
| --   Metal framework: /System/Library/Frameworks/Metal.framework     |
| --   Metal shader compiler: /usr/bin/metal                           |
| --   Foundation framework: found                                     |
| -- Configuring done                                                  |
| -- Generating done                                                   |
+----------------------------------------------------------------------+
```

**TUI mockup (Metal not found)**:
```
+----------------------------------------------------------------------+
| $ cmake -B build -DNWAVE_ENABLE_GPU=ON                              |
|                                                                      |
| -- nwave-raytracer build configuration                               |
| -- C++ Standard: 17                                                  |
| -- Metal GPU support: NOT AVAILABLE (Metal framework not found)      |
| CMake Warning:                                                       |
|   NWAVE_ENABLE_GPU=ON was requested but Metal is not available.      |
|   The build will succeed. CPU rendering is always available.         |
| -- Configuring done                                                  |
+----------------------------------------------------------------------+
```

| Dimension | Detail |
|---|---|
| **Action** | Run CMake with GPU option |
| **Touchpoint** | CMake configure output |
| **Thinking** | "Will Metal be detected? Do I need extra setup?" |
| **Feeling** | Cautious uncertainty, then relief when Metal is found |
| **Pain points** | Missing Xcode CLI tools; unclear whether GPU option silently degrades |
| **Opportunity** | Explicit detection output; warning-not-error when Metal is missing; clear framework paths shown |

---

### Step 2: Build

```
TRIGGER: CMake configured successfully
ACTION:  cmake --build build
OUTPUT:  Binary with Metal support, compiled shaders
```

**TUI mockup (happy path)**:
```
+----------------------------------------------------------------------+
| $ cmake --build build -j12                                           |
|                                                                      |
| [  1%] Building CXX nwave_core...                                    |
| [ 30%] Building CXX nwave_domain...                                  |
| [ 45%] Compiling Metal shaders...                                    |
|        shaders/ray_trace.metal -> ray_trace.air                      |
|        Linking Metal library -> nwave_shaders.metallib               |
| [ 46%] Building OBJCXX metal_backend.mm                              |
| [ 80%] Building CXX nwave_infrastructure...                          |
| [100%] Built target nwave                                            |
+----------------------------------------------------------------------+
```

**TUI mockup (shader error)**:
```
+----------------------------------------------------------------------+
| [ 45%] Compiling Metal shaders...                                    |
| shaders/ray_trace.metal:87:12: error: use of undeclared identifier   |
|     'scattered_ray'                                                  |
|     return scattered_ray;                                            |
|            ^~~~~~~~~~~~~                                             |
| 1 error generated.                                                   |
| make[2]: *** [nwave_shaders.metallib] Error 1                        |
+----------------------------------------------------------------------+
```

| Dimension | Detail |
|---|---|
| **Action** | Run cmake --build |
| **Touchpoint** | Build output with shader compilation phase |
| **Thinking** | "Will the shaders compile? Will the ObjC++ bridging work?" |
| **Feeling** | Moderate confidence; relief when shaders compile; familiarity with clang-style errors if they fail |
| **Pain points** | New build phase (shader compilation) is unfamiliar; ObjC++ errors can be cryptic |
| **Opportunity** | Shader compilation as a visible, named build step; familiar error format; shaders compiled at build time, not runtime |

---

### Step 3: Render with GPU Backend

```
TRIGGER: Build succeeded, developer wants to test GPU rendering
ACTION:  nwave render scene.yaml --backend gpu
OUTPUT:  Rendered image with GPU performance statistics
```

**TUI mockup (GPU render)**:
```
+----------------------------------------------------------------------+
| $ nwave render scenes/cornell-box.yaml --backend gpu                 |
|                                                                      |
| nwave ray tracer v0.2.0                                              |
| Scene: scenes/cornell-box.yaml                                       |
| Backend: Metal GPU (Apple M2 Max, 30 cores, 32 GB)                  |
| Output: output.ppm (800x600, 16 SPP, depth 10)                      |
|                                                                      |
| Uploading scene to GPU... done (3 shapes, 6 materials, 0.001s)      |
| Building GPU BVH... done (5 nodes, 0.001s)                          |
|                                                                      |
| Rendering [================================] 100%  elapsed 0.34s     |
|                                                                      |
| Render complete.                                                     |
|   Output:     output.ppm                                             |
|   Resolution: 800 x 600                                              |
|   Samples:    16 per pixel                                           |
|   Time:       0.34s (22,588,235 rays/s)                              |
|   Backend:    Metal GPU (Apple M2 Max)                               |
|   Speedup:    ~62x vs estimated CPU time                             |
+----------------------------------------------------------------------+
```

**TUI mockup (CPU render, unchanged from current)**:
```
+----------------------------------------------------------------------+
| $ nwave render scenes/cornell-box.yaml                               |
|                                                                      |
| nwave ray tracer v0.2.0                                              |
| Scene: scenes/cornell-box.yaml                                       |
| Backend: CPU (12 threads)                                            |
| Output: output.ppm (800x600, 16 SPP, depth 10)                      |
|                                                                      |
| Building BVH... done (3 primitives, 0.002s)                         |
|                                                                      |
| Rendering [================================] 100%  row 600/600       |
|   elapsed 21.3s                                                      |
|                                                                      |
| Render complete.                                                     |
|   Output:     output.ppm                                             |
|   Resolution: 800 x 600                                              |
|   Samples:    16 per pixel                                           |
|   Time:       21.3s (28,176 rays/ms)                                 |
|   Backend:    CPU (12 threads)                                       |
+----------------------------------------------------------------------+
```

**TUI mockup (GPU not available error)**:
```
+----------------------------------------------------------------------+
| $ nwave render scenes/cornell-box.yaml --backend gpu                 |
|                                                                      |
| Error: GPU backend requested but not available.                      |
|   This binary was built without Metal support.                       |
|   Rebuild with: cmake -B build -DNWAVE_ENABLE_GPU=ON                |
|   Or use: nwave render scenes/cornell-box.yaml --backend cpu         |
+----------------------------------------------------------------------+
```

| Dimension | Detail |
|---|---|
| **Action** | Run render with --backend gpu |
| **Touchpoint** | CLI render output with GPU-specific phases and statistics |
| **Thinking** | "How fast is it? Did it work correctly? What GPU is it using?" |
| **Feeling** | Anticipation, then thrill at the speedup; satisfaction seeing device info and throughput |
| **Pain points** | New "upload to GPU" phase might confuse; unclear if output is correct without comparison |
| **Opportunity** | GPU device info in header; upload phase explicitly shown; speedup estimate quantifies the win; same output format as CPU for familiarity |

---

### Step 4: Validate Correctness

```
TRIGGER: First successful GPU render
ACTION:  Render same scene with both backends, compare output
OUTPUT:  Confidence that GPU produces correct results
```

**Workflow**:
```
$ nwave render scenes/cornell-box.yaml --backend cpu -o renders/cpu.ppm
$ nwave render scenes/cornell-box.yaml --backend gpu -o renders/gpu.ppm
$ # Visual comparison or external diff tool
```

| Dimension | Detail |
|---|---|
| **Action** | Render with both backends, compare images |
| **Touchpoint** | Two image files on disk |
| **Thinking** | "Do these look the same? Are there subtle differences from float vs double precision?" |
| **Feeling** | Cautious optimism; seeking confirmation that GPU output is trustworthy |
| **Pain points** | Manual comparison is tedious; floating-point differences are expected but hard to quantify visually |
| **Opportunity** | Future `nwave compare` command; documented tolerance expectations; deterministic seeding for reproducibility |

---

### Step 5: Daily Use (GPU as Routine)

```
TRIGGER: GPU rendering validated and trusted
ACTION:  Use --backend gpu (or --backend auto) for all rendering
OUTPUT:  Dramatically faster iteration cycles
```

**Iteration workflow with GPU**:
```
# Quick preview (was 0.8s on CPU, now 0.02s on GPU)
$ nwave render scenes/cornell-box.yaml --backend gpu --spp 4 --width 200

# Full quality (was 213.5s on CPU, now ~3.4s on GPU)
$ nwave render scenes/cornell-box.yaml --backend gpu --spp 1000

# Animation (was 12m48s on CPU, now ~18.6s on GPU)
$ nwave render scene.yaml --physics-animate --backend gpu
```

| Dimension | Detail |
|---|---|
| **Action** | Regular development workflow with GPU backend |
| **Touchpoint** | Same CLI, same flags, just faster |
| **Thinking** | "I can iterate in real-time now. Let me try more complex scenes." |
| **Feeling** | Confident, empowered, creative (the tool is no longer the bottleneck) |
| **Pain points** | Must remember `--backend gpu` flag until auto-detect is default |
| **Opportunity** | `--backend auto` as future default; GPU enables higher SPP and resolution in iteration loops |

---

## GPU-Specific Error Experience

### Error: Runtime GPU Memory Exceeded

```
+----------------------------------------------------------------------+
| $ nwave render huge_scene.yaml --backend gpu                         |
|                                                                      |
| nwave ray tracer v0.2.0                                              |
| Scene: huge_scene.yaml                                               |
| Backend: Metal GPU (Apple M2 Max, 30 cores, 32 GB)                  |
|                                                                      |
| Uploading scene to GPU...                                            |
| Error: Scene requires ~34.2 GB GPU buffer space (32.0 GB available). |
|   Largest buffers:                                                   |
|     BVH nodes:        18.1 GB                                        |
|     Triangle vertices: 14.6 GB                                       |
|     Materials:          1.5 GB                                        |
|   Options:                                                           |
|     - Reduce scene complexity (12,847,293 triangles)                 |
|     - Use --backend cpu (slower, limited by system RAM)              |
+----------------------------------------------------------------------+
```

### Error: GPU Timeout Warning

```
+----------------------------------------------------------------------+
| Rendering [=====>                           ] 18%  elapsed 28.4s     |
|                                                                      |
| Warning: GPU command exceeded 30s timeout.                           |
|   Splitting into smaller dispatches. Render continues.               |
|                                                                      |
| Rendering [================================] 100%  elapsed 142.7s    |
+----------------------------------------------------------------------+
```

### Error: NaN Pixels Detected

```
+----------------------------------------------------------------------+
| Render complete.                                                     |
|   Output:     output.ppm                                             |
|   ...                                                                |
|   Warning: 23 pixels contained NaN values (replaced with magenta).   |
|   This usually indicates a shader bug (division by zero,             |
|   degenerate geometry). Run with --backend cpu to verify.            |
+----------------------------------------------------------------------+
```

---

## Data Flow: CPU vs GPU Pipeline

```
                         CPU PATH (existing)
                         ==================
scene.yaml --> SceneLoader --> Scene + Camera + Materials
                                  |
                                  v
                            Renderer.render()
                                  |
                          std::thread per scanline chunk
                                  |
                          trace_ray() [recursive, virtual dispatch]
                                  |
                                  v
                          pixels[] (Color3 vector)
                                  |
                                  v
                            write_ppm()


                         GPU PATH (new)
                         ==============
scene.yaml --> SceneLoader --> Scene + Camera + Materials
                                  |
                                  v
                          MetalBackend.upload_scene()
                            |-- Flatten BVH (pointer tree -> index array)
                            |-- Pack shapes (virtual -> tagged union structs)
                            |-- Pack materials (virtual -> tagged union structs)
                            |-- Allocate Metal buffers
                            |-- Copy data to GPU
                                  |
                                  v
                          MetalBackend.render()
                            |-- Set compute pipeline state
                            |-- Encode kernel dispatch (threadgroups)
                            |-- Commit command buffer
                            |-- Wait for completion
                                  |
                                  v
                          Read back pixel buffer (GPU -> CPU)
                                  |
                                  v
                          pixels[] (Color3 vector, same as CPU)
                                  |
                                  v
                            write_ppm()  [same as CPU path]
```

**Key insight**: The GPU and CPU paths diverge at the renderer and converge at the pixel output. Everything before (scene loading, validation) and after (image writing) is shared. The `MetalBackend` is an infrastructure concern (Ring 4) that replaces `std::thread` parallelism with GPU compute parallelism.

---

## Variable Traceability

Every `${variable}` in the TUI mockups traced to its source:

| Variable | Source | Derivation |
|---|---|---|
| `${backend_name}` | `--backend` CLI flag | `"Metal GPU"` if gpu, `"CPU"` if cpu |
| `${device_name}` | `MTLDevice.name` property | Queried at runtime when GPU selected |
| `${gpu_core_count}` | Metal device properties | Queried at runtime |
| `${gpu_memory_gb}` | `MTLDevice.recommendedMaxWorkingSetSize` | Bytes / (1024^3), rounded |
| `${shape_count}` | Scene object count after loading | `scene.shapes().size()` |
| `${material_count}` | Scene material count after loading | `scene.materials().size()` |
| `${upload_time_s}` | Wall-clock time for GPU buffer upload | Measured: start to end of `upload_scene()` |
| `${bvh_node_count}` | Flattened BVH node array size | `flattened_bvh.size()` |
| `${bvh_build_time_s}` | Wall-clock time for BVH flatten + upload | Measured |
| `${render_time_s}` | Wall-clock time for GPU render dispatch | Measured: commit to completion |
| `${rays_per_sec}` | Computed throughput | `(width * height * spp) / render_time_s` |
| `${speedup_estimate}` | Computed ratio | `estimated_cpu_time / gpu_time` (optional, requires baseline) |
| `${image_width}` | CLI `--width` or scene file | CLI override > scene > default |
| `${image_height}` | Computed from width and aspect ratio | `width / aspect_ratio` |
| `${spp}` | CLI `--spp` or default | CLI override > default (16) |
| `${max_depth}` | Hardcoded default | 10 (same as CPU) |
| `${buffer_size_gb}` | Computed per buffer | Each Metal buffer allocation size |
| `${available_memory_gb}` | `MTLDevice.recommendedMaxWorkingSetSize` | Bytes / (1024^3) |
| `${nan_pixel_count}` | Post-render scan | Count of pixels with NaN components |
| `${thread_count}` | CPU: `std::thread::hardware_concurrency()` | Queried at runtime |

---

## CLI Interface Changes

### New Flag: `--backend`

| Value | Behavior |
|---|---|
| `cpu` | Use multi-threaded CPU renderer (current behavior, default) |
| `gpu` | Use Metal GPU compute renderer (requires `NWAVE_HAS_METAL`) |
| `auto` | Use GPU if available, fall back to CPU (future default) |

### Updated `--help` Output

```
Usage: nwave <command> [options]

Commands:
  render <file.yaml>   Load a YAML scene and render to PPM
    --width <N>        Override image width
    --spp <N>          Override samples per pixel
    -o <file>          Output filename (default: output.ppm)
    --backend <type>   Rendering backend: cpu (default), gpu, auto
    --fps <N>          Override frames per second
    --output-dir <dir> Override output directory
    --physics-animate  Run physics-driven animation
  validate <file.yaml> Validate a YAML scene (no render)

Flags:
  --help               Show this help message
```

### Flag Interactions

| Combination | Behavior |
|---|---|
| `--backend gpu --physics-animate` | Physics on CPU, rendering on GPU. Scene uploaded once, transforms updated per frame. |
| `--backend gpu --spp 4 --width 200` | GPU preview render (fast iteration). |
| `--backend auto` (no Metal) | Silently falls back to CPU. Prints `Backend: CPU (12 threads) [auto-selected, no GPU available]`. |
| `--backend gpu` (no Metal build) | Error with rebuild instructions. |

---

## Emotional Journey Annotations

```
Step 1 (Configure)   Emotion: Cautious uncertainty
                     Design response: Automatic detection, explicit output,
                     warning-not-error when Metal missing, CPU always works

Step 2 (Build)       Emotion: Moderate confidence, watching for shader errors
                     Design response: Shader compilation as named build phase,
                     familiar clang error format, build-time not runtime errors

Step 3 (Render)      Emotion: Anticipation -> thrill at first speedup
                     Design response: GPU device info in header, explicit
                     upload phase, speedup estimate quantifies the win,
                     same output format as CPU for continuity

Step 4 (Validate)    Emotion: Cautious optimism, seeking correctness proof
                     Design response: Both backends produce same format,
                     easy side-by-side comparison, tolerance documented

Step 5 (Daily)       Emotion: Confident empowerment
                     Design response: Identical CLI (just add --backend gpu),
                     same flags work, animation pipeline unchanged

Errors               Emotion: Brief confusion or frustration
                     Design response: Every error names the problem,
                     states what was expected vs found, suggests recovery.
                     GPU errors always mention --backend cpu as escape hatch.
```

---

## Horizontal Coherence Checks

| Check | Status | Notes |
|---|---|---|
| CLI vocabulary consistent across all output | PASS | "backend", "Metal GPU", "CPU", "upload", "buffer" used consistently per glossary |
| Emotional arc has no jarring transitions | PASS | Cautious -> relief -> anticipation -> thrill -> confidence. Each step's feedback sustains momentum. |
| Shared artifacts have single source of truth | PASS | All traced in registry. GPU device info comes from Metal API. Build config from CMake cache. |
| Error messages include actionable fix guidance | PASS | Every error includes: what went wrong, why, and what to do next. GPU errors always offer CPU fallback. |
| Progress feedback present at every waiting point | PASS | Build: percentage; upload: "done" with timing; render: progress bar; shader compile: per-file status. |
| Output of each step feeds cleanly into next step | PASS | CMake -> build -> render. Same pixel output format regardless of backend. |
| CPU path completely unchanged | PASS | Default backend is CPU. No new flags required for existing workflow. |
| GPU backend isolated to infrastructure layer | PASS | MetalBackend is Ring 4. Core/domain/application layers remain pure C++17. |
| New CLI flag follows existing conventions | PASS | `--backend` follows same pattern as `--width`, `--spp`, `--physics-animate`. |

---

## Integration Risk Map

| Risk | Severity | Where It Breaks | Mitigation |
|---|---|---|---|
| Shader function signatures diverge from C++ dispatch code | High | Step 3: kernel dispatch crashes or produces garbage | Build-time validation: static_assert on buffer struct sizes matching between C++ and Metal |
| Float (GPU) vs double (CPU) precision differences | Medium | Step 4: visible artifacts in GPU renders | Document expected tolerance; use float consistently in GPU path; compare with reduced-precision CPU path |
| BVH flattening produces incorrect indices | High | Step 3: wrong ray-scene intersections | Unit test: flatten BVH, traverse with known rays, compare hit results to pointer-based BVH |
| Material tagged-union packing misses a type | High | Step 3: objects render as wrong material or black | Exhaustive switch in packing code; compiler warning on unhandled enum cases; test all material types |
| GPU memory estimate underestimates actual usage | Medium | Step 5b: allocation fails at runtime despite passing check | Add safety margin (10-20%) to estimate; handle allocation failure gracefully |
| Metal watchdog kills long-running shader | Medium | Step 5d: render fails or hangs | Split large dispatches into chunks; set `MTLCommandBuffer` timeout; detect and retry with smaller dispatches |
| `.metallib` not found at runtime (deployment) | Low | Step 5c: immediate error on GPU render | Embed metallib as binary resource OR place alongside executable with known relative path |
| Animation pipeline re-uploads entire scene per frame | Medium | Step 6: animation with GPU slower than expected | Upload scene once; update only transform buffer per frame |
| New `--backend` flag conflicts with future flags | Low | Future CLI changes | Flag is self-contained; no interaction with scene content flags |

---

## Scope Boundaries

**In scope for this journey**:
- CMake build system integration for Metal detection and shader compilation
- CLI `--backend` flag for backend selection (cpu, gpu, auto)
- GPU render pipeline: scene upload, compute dispatch, pixel readback
- Error handling for all GPU-specific failure modes
- GPU rendering integrated with existing animation pipeline
- Performance statistics (rays/s, speedup estimate)

**Out of scope (future journeys)**:
- `nwave compare` subcommand for CPU vs GPU image diffing
- Real-time interactive preview via Metal rendering to a window
- Multi-GPU rendering
- Hybrid CPU+GPU rendering (complex rays on CPU, simple on GPU)
- GPU-accelerated BVH construction
- Texture memory management for GPU
- `--backend auto` as default (requires stability validation first)
- Verbose/debug output modes for GPU internals (buffer sizes, dispatch params)
