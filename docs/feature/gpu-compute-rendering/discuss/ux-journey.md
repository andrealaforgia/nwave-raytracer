# UX Journey: GPU Compute Rendering (Metal)

**Journey ID**: UXJ-GCR-001
**Feature**: gpu-compute-rendering
**Persona**: Solo developer building this ray tracer, interacting via CLI and CMake build system
**Date**: 2026-02-17
**Status**: Draft

---

## Journey Summary

The developer refactors the nwave ray tracer to support Metal GPU compute shaders for rendering. The "user experience" is developer experience: building with Metal support, selecting GPU vs CPU backend at render time, interpreting GPU-specific output and diagnostics, and handling failures gracefully (no Metal device, shader compilation errors, GPU memory limits). The existing CPU path must remain fully functional as a fallback.

**Trigger**: Render times are too slow for iteration (minutes per frame, hours for animation sequences). The developer wants 50-200x speedup via GPU compute.

**Success Criteria**: A single `--backend gpu` flag produces identical (within floating-point tolerance) output to CPU rendering, with dramatic speedup visible in the render statistics. CPU rendering remains the default and works unchanged on any system.

---

## Emotional Arc

```
Confidence
    ^
    |                                                        * Render + Compare
    |                                                   *****
    |                                          * First GPU
    |                                     *****  render works!
    |                          * Build OK *
    |                     *****
    |           * CMake finds
    |      **** Metal
    |  ***
    | *
    * Configure build
    |
    +-------------------------------------------------------------------> Time

    Cautious/       Relief          Growing         Thrill of       Confident
    uncertain       "it found       confidence      first GPU       daily
    "will this      Metal"          "it compiles"   speedup         workflow
     work on
     my machine?"
```

The arc moves from build-system uncertainty (will Metal even be detected?) through compilation relief, to the high-reward moment of the first GPU render completing in seconds instead of minutes. After that, GPU rendering becomes routine and the emotional baseline shifts to confident daily use.

---

## Step 1: Configure the Build System

**What the developer does**: Runs CMake to configure the project. The build system must detect Metal availability automatically.

**What they feel**: Cautious uncertainty. "Will CMake find the Metal framework? Do I need to install anything extra? Will this break my existing CPU build?"

**What they type**:
```
$ cmake -B build -DNWAVE_ENABLE_GPU=ON
```

**What they see (happy path -- macOS with Metal)**:
```
-- nwave-raytracer build configuration
-- C++ Standard: 17
-- Metal GPU support: ENABLED
--   Metal framework: /System/Library/Frameworks/Metal.framework
--   Metal shader compiler: /usr/bin/metal
--   Foundation framework: found
-- Configuring done
-- Generating done
```

**What they see (no Metal -- Linux or old macOS)**:
```
-- nwave-raytracer build configuration
-- C++ Standard: 17
-- Metal GPU support: NOT AVAILABLE (Metal framework not found)
--   GPU rendering will not be available. CPU backend will be used.
-- Configuring done
-- Generating done
```

**What they see (GPU requested but not available)**:
```
$ cmake -B build -DNWAVE_ENABLE_GPU=ON
-- nwave-raytracer build configuration
-- C++ Standard: 17
-- Metal GPU support: NOT AVAILABLE (Metal framework not found)
CMake Warning:
  NWAVE_ENABLE_GPU=ON was requested but Metal is not available on this platform.
  The build will succeed but GPU backend will not be compiled.
  CPU rendering is always available.
-- Configuring done
```

**Shared artifacts produced**:
- `NWAVE_ENABLE_GPU` CMake option (ON/OFF)
- `NWAVE_HAS_METAL` compile definition (set when Metal is actually found)
- Compiled `.metallib` shader library (embedded or alongside binary)

**Design decisions**:
- `NWAVE_ENABLE_GPU` defaults to OFF. The developer must opt in. This ensures the existing build path is completely unchanged for anyone who does not want GPU support.
- Metal detection is automatic via `find_library(Metal)` and `find_program(metal)`. No manual path configuration required.
- If Metal is requested but not found, CMake warns but does not error. The build still produces a working CPU-only binary. This is a warning, not a failure -- the developer is never blocked.
- Metal shader files (`.metal`) are compiled to `.metallib` at build time. Shader compilation errors surface during `cmake --build`, not at runtime.

**Error paths**:
- Metal shader syntax error: build fails with clang-style error message pointing to `.metal` file and line number. Developer fixes shader code and rebuilds.
- Missing Xcode Command Line Tools: CMake cannot find `metal` compiler. Error message suggests `xcode-select --install`.

---

## Step 2: Build the Project

**What the developer does**: Compiles the project with Metal support.

**What they feel**: Moderate confidence. CMake found everything, but will the shader code actually compile? Will the Objective-C++ bridging work?

**What they type**:
```
$ cmake --build build -j$(sysctl -n hw.ncpu)
```

**What they see (happy path)**:
```
[  1%] Building CXX object src/CMakeFiles/nwave_core.dir/core/aabb.cpp.o
...
[ 45%] Compiling Metal shaders...
       shaders/ray_trace.metal -> ray_trace.air
       Linking Metal library -> nwave_shaders.metallib
[ 46%] Building OBJCXX object src/CMakeFiles/nwave_infrastructure.dir/infrastructure/metal_backend.mm.o
...
[100%] Built target nwave
```

**What they see (shader compilation error)**:
```
[ 45%] Compiling Metal shaders...
shaders/ray_trace.metal:87:12: error: use of undeclared identifier 'scattered_ray'
    return scattered_ray;
           ^
1 error generated.
make[2]: *** [src/CMakeFiles/nwave_shaders.dir/build.make:76: nwave_shaders.metallib] Error 1
```

**What they feel**: Relief when shaders compile successfully. The familiar clang-style error format for shader failures means no new tooling to learn -- they can read these errors the same way they read C++ errors.

**Shared artifacts produced**:
- `nwave` binary with Metal support compiled in (guarded by `NWAVE_HAS_METAL`)
- `nwave_shaders.metallib` (Metal shader library, bundled with binary or in known relative path)

**Design decisions**:
- Metal shaders are compiled at build time, not runtime. This catches shader bugs during development, not during a user's render.
- The `.metallib` is placed alongside the binary or embedded as a resource. The binary knows where to find it relative to itself.
- Objective-C++ files (`.mm`) are used only in the infrastructure layer (Ring 4). Core, domain, and application layers remain pure C++17.

---

## Step 3: Select Backend and Render

**What the developer does**: Runs a render with the GPU backend selected via CLI flag.

**What they feel**: Anticipation. This is the payoff moment -- will it actually be faster?

**What they type**:
```
$ nwave render scenes/cornell-box.yaml --backend gpu
```

**What they see (happy path)**:
```
nwave ray tracer v0.2.0
Scene: scenes/cornell-box.yaml
Backend: Metal GPU (Apple M2 Max, 30 GPU cores, 32 GB unified memory)
Output: output.ppm (800x600, 16 SPP, depth 10)

Uploading scene to GPU... done (3 shapes, 6 materials, 0.001s)
Building GPU BVH... done (5 nodes, 0.001s)

Rendering [================================] 100%  elapsed 0.34s

Render complete.
  Output:     output.ppm
  Resolution: 800 x 600
  Samples:    16 per pixel
  Time:       0.34s (22,588,235 rays/s)
  Backend:    Metal GPU (Apple M2 Max)
  Speedup:    ~62x vs estimated CPU time
```

**What they see (CPU backend, for comparison)**:
```
$ nwave render scenes/cornell-box.yaml --backend cpu
```
or simply (CPU is the default):
```
$ nwave render scenes/cornell-box.yaml
```
```
nwave ray tracer v0.2.0
Scene: scenes/cornell-box.yaml
Backend: CPU (12 threads)
Output: output.ppm (800x600, 16 SPP, depth 10)

Building BVH... done (3 primitives, 0.002s)

Rendering [================================] 100%  row 600/600  elapsed 21.3s

Render complete.
  Output:     output.ppm
  Resolution: 800 x 600
  Samples:    16 per pixel
  Time:       21.3s (28,176 rays/ms)
  Backend:    CPU (12 threads)
```

**What they see (--backend gpu on a CPU-only build)**:
```
$ nwave render scenes/cornell-box.yaml --backend gpu

Error: GPU backend requested but not available.
  This binary was built without Metal support (NWAVE_ENABLE_GPU was OFF or Metal was not found).
  Rebuild with: cmake -B build -DNWAVE_ENABLE_GPU=ON
  Or use the CPU backend: nwave render scenes/cornell-box.yaml --backend cpu
```

**What they see (--backend auto, the eventual default)**:
```
$ nwave render scenes/cornell-box.yaml --backend auto

nwave ray tracer v0.2.0
Scene: scenes/cornell-box.yaml
Backend: Metal GPU (Apple M2 Max) [auto-selected]
...
```

**Shared artifacts consumed**: Scene file, CLI flags (`--backend`, `--spp`, `--width`)
**Shared artifacts produced**: `output_image_path`, render statistics, backend info

**Design decisions**:
- `--backend cpu` is the default in v0.2.0. This ensures zero behavioral change for existing users. Once GPU rendering is proven stable, `--backend auto` becomes the default.
- `--backend gpu` explicitly requests Metal. `--backend auto` selects GPU if available, falls back to CPU.
- GPU device info is printed in the header (chip name, core count, memory) so the developer knows exactly what hardware is being used.
- The "Uploading scene to GPU" step is shown explicitly. This is a new phase that does not exist in CPU rendering. Showing it prevents confusion about what the renderer is doing before pixels appear.
- Speedup estimate is printed as a convenience. It uses a rough CPU baseline (prior renders or an estimated rays/s) to give the developer the satisfaction of seeing the improvement quantified.

---

## Step 4: Validate Correctness (GPU vs CPU)

**What the developer does**: Compares GPU and CPU output to verify the GPU path produces correct results.

**What they feel**: Cautious optimism. Speed is great, but only if the output is correct. Even small differences (wrong normals, missing shadows, color shifts) would undermine confidence.

**What they type**:
```
$ nwave render scenes/cornell-box.yaml --backend cpu -o renders/cpu.ppm
$ nwave render scenes/cornell-box.yaml --backend gpu -o renders/gpu.ppm
```

Then visually compares, or (future convenience):
```
$ nwave compare renders/cpu.ppm renders/gpu.ppm
```

**What they see (compare, if implemented)**:
```
Comparing renders/cpu.ppm vs renders/gpu.ppm...

  Resolution: 800x600 (match)
  Max pixel difference: 0.0039 (1/255)
  Mean pixel difference: 0.0002
  Pixels with diff > 0.01: 0 of 480,000 (0.00%)

  Result: PASS (within floating-point tolerance)
```

**Design decisions**:
- A `compare` subcommand is a future convenience, not required for the initial journey. The developer can visually diff images or use external tools (ImageMagick `compare`).
- Floating-point differences between CPU (double precision) and GPU (float precision, potentially different instruction ordering) are expected. The tolerance threshold is documented.
- The developer should be able to render the same scene with both backends using identical random seeds. This requires deterministic seeding, which is a design constraint on the GPU kernel.

**Error paths**:
- Visible differences: likely a bug in the shader code (wrong normal transformation, missing material branch). The developer debugs by reducing the scene to the simplest failing case.

---

## Step 5: GPU-Specific Error Handling

**What the developer experiences**: Various GPU-specific failure modes during development and usage.

### 5a: No Metal Device at Runtime

```
$ nwave render scene.yaml --backend gpu

Error: No Metal GPU device found.
  Metal framework is linked but no compatible GPU is available.
  This can happen in VMs or remote sessions without GPU access.
  Use --backend cpu to render with the CPU.
```

**Emotion**: Mild confusion, quickly resolved. The error names the problem and provides the fix.

### 5b: Scene Too Large for GPU Memory

```
$ nwave render huge_scene.yaml --backend gpu

nwave ray tracer v0.2.0
Scene: huge_scene.yaml
Backend: Metal GPU (Apple M2 Max, 30 GPU cores, 32 GB unified memory)

Uploading scene to GPU...
Error: Scene requires ~34.2 GB GPU buffer space but only 32.0 GB is available.
  Largest buffers: BVH nodes (18.1 GB), triangle vertices (14.6 GB), materials (1.5 GB)
  Options:
    - Reduce scene complexity (currently 12,847,293 triangles)
    - Use --backend cpu (slower but no memory limit beyond system RAM)
```

**Emotion**: Frustration, but the error is informative. It tells the developer exactly which buffers are large and what their options are. No guessing.

### 5c: Shader Library Not Found

```
$ nwave render scene.yaml --backend gpu

Error: Metal shader library not found.
  Expected at: /usr/local/bin/nwave_shaders.metallib
  The shader library should be alongside the nwave binary.
  Try rebuilding: cmake --build build
```

**Emotion**: Confusion (deployment issue), quickly resolved by the suggested fix.

### 5d: GPU Timeout (Long Render Dispatch)

```
$ nwave render massive_scene.yaml --backend gpu --spp 1000

nwave ray tracer v0.2.0
...
Rendering [=====>                           ] 18%  elapsed 28.4s

Warning: GPU command buffer execution exceeded 30s.
  Metal may terminate long-running shaders. Splitting work into smaller dispatches.
  Render will continue but may be slightly slower than optimal.

Rendering [================================] 100%  elapsed 142.7s
```

**Emotion**: Brief alarm at the warning, then relief that the renderer handles it automatically. The developer learns that very heavy renders may trigger Metal's watchdog timer and the system adapts.

---

## Step 6: GPU Rendering in Animation Pipeline

**What the developer does**: Uses GPU backend for physics animation rendering (the existing `--physics-animate` workflow).

**What they type**:
```
$ nwave render scene.yaml --physics-animate --backend gpu
```

**What they see**:
```
nwave ray tracer v0.2.0
Scene: scene.yaml
Backend: Metal GPU (Apple M2 Max, 30 GPU cores, 32 GB unified memory)

Loading scene.yaml...
  26 objects, 1 light, 25 dynamic bodies

Simulating physics (5.0s at 60Hz)...
  [========================================] 300/300 steps (0.4s)

Physics summary:
  Active bodies at end: 18/25
  Total collisions: 47
  Bodies at rest: 7

Uploading scene to GPU... done (26 shapes, 3 materials, 0.002s)

Rendering 150 frames (800x450, 16 SPP, Metal GPU)...
  Frame 150/150 [========================================] 100%
  Done (18.6s, avg 0.12s/frame)

Frames saved to frames/
  frames/frame_0000.ppm ... frames/frame_0149.ppm

To create video:
  ffmpeg -framerate 30 -i frames/frame_%04d.ppm \
    -c:v libx264 -pix_fmt yuv420p output.mp4
```

**What they feel**: Deep satisfaction. The animation pipeline that previously took 12+ minutes now completes in under 20 seconds. The workflow is identical -- only the `--backend gpu` flag was added.

**Design decisions**:
- Scene is uploaded to GPU once at the start. Per-frame, only the transform buffer is updated (cheap operation). This avoids re-uploading the entire scene for each frame.
- The "Uploading scene to GPU" step appears once, before the frame rendering loop. This communicates the optimization to the developer.
- Physics simulation remains on CPU (Jolt Physics). Only rendering moves to GPU. This is explicit in the output: physics phase has no "Metal GPU" annotation.

---

## Shared Artifact Registry

| Artifact | Source (Step) | Consumed By (Steps) | Format | Single Source of Truth |
|---|---|---|---|---|
| `NWAVE_ENABLE_GPU` | CMake option (Step 1) | Build system (Step 2), compile definitions | CMake cache variable | CMakeLists.txt |
| `NWAVE_HAS_METAL` | CMake detection (Step 1) | Preprocessor guards in C++ code | Compile definition | CMake find_library result |
| `nwave_shaders.metallib` | Metal shader compilation (Step 2) | GPU backend at runtime (Step 3) | Metal shader library | Build output, alongside binary |
| `--backend` flag | CLI argument (Step 3) | Renderer backend selection | String: `cpu`, `gpu`, `auto` | CLI parser |
| `metal_device_name` | Metal API query (Step 3) | CLI output header, error messages | String | `MTLDevice.name` at runtime |
| `gpu_core_count` | Metal API query (Step 3) | CLI output header | Integer | `MTLDevice` properties |
| `gpu_memory_size` | Metal API query (Step 3) | Memory check (Step 5b), CLI output | Bytes | `MTLDevice.recommendedMaxWorkingSetSize` |
| `scene_gpu_buffers` | Scene upload (Step 3) | GPU kernel dispatch | Metal buffers (shapes, materials, BVH, pixels) | MetalBackend class |
| `render_statistics` | Render completion (Step 3) | CLI output, speedup estimate | Struct | Computed during render |
| `scene_file_path` | CLI argument | Scene loading, all steps | String | CLI argv |
| `output_image_path` | CLI `-o` flag or default | Image writing | String | CLI parser |

---

## Integration Checkpoints

| Checkpoint | From | To | What to Verify |
|---|---|---|---|
| CMake Metal detection | `find_library(Metal)` | `NWAVE_HAS_METAL` definition | Framework path resolves, `metal` compiler found |
| Shader compilation | `.metal` source files | `.metallib` output | No compile errors; shader function signatures match C++ dispatch code |
| Scene data serialization | Domain objects (Ring 2) | Metal GPU buffers | All shape types serialized correctly; material properties packed to GPU struct layout; BVH flattened from pointer-based to index-based |
| GPU buffer allocation | Scene size calculation | `MTLDevice.newBuffer()` | Buffer sizes do not exceed `recommendedMaxWorkingSetSize`; allocation succeeds |
| Kernel dispatch | Render settings + GPU buffers | Compute kernel execution | Thread group sizes valid for device; output buffer dimensions match image size |
| Pixel readback | GPU output buffer | Host pixel array | GPU float4 pixels converted correctly to Color3; gamma correction applied (same as CPU path) |
| Backend selection | `--backend` CLI flag | Renderer instantiation | `gpu` flag with no Metal support produces clear error; `auto` falls back gracefully |
| Animation per-frame update | Physics transforms (CPU) | GPU transform buffer | Only transform buffer updated per frame, not entire scene; buffer contents match CPU-side transforms |

---

## CLI Vocabulary

Consistent terminology across all user-facing output:

| Term | Meaning | Used In |
|---|---|---|
| `backend` | Rendering execution target (CPU or GPU) | CLI flag, output header, error messages |
| `Metal GPU` | Apple Metal compute shader backend | Output header, error messages |
| `CPU` | Multi-threaded CPU backend (std::thread) | Output header, default |
| `upload` / `uploading` | Transferring scene data to GPU memory | GPU render phase output |
| `dispatch` | Sending work to GPU compute kernel | Internal; not shown to user unless in verbose/debug |
| `buffer` | GPU memory allocation for scene data | Error messages (memory limit), verbose output |
| `shader` | Metal compute kernel code | Build output, error messages |
| `metallib` | Compiled Metal shader library | Build output, error messages |
| `rays/s` | Throughput metric | Render statistics |
| `speedup` | Performance ratio vs CPU baseline | Render statistics (optional) |
| `thread group` | GPU execution unit (Metal terminology) | Debug output only |

---

## Error Paths Summary

| Error | When | User Sees | Recovery |
|---|---|---|---|
| Metal framework not found | CMake configure (Step 1) | Warning: Metal not available, build continues without GPU | Install on macOS or accept CPU-only build |
| `metal` compiler not found | CMake configure (Step 1) | Error suggesting `xcode-select --install` | Install Xcode Command Line Tools |
| Shader compilation error | Build (Step 2) | Clang-style error with file, line, column | Fix shader source code, rebuild |
| `--backend gpu` on CPU-only build | Render (Step 3) | Error with rebuild instructions and CPU fallback suggestion | Rebuild with `NWAVE_ENABLE_GPU=ON` or use `--backend cpu` |
| No Metal device at runtime | Render (Step 3) | Error explaining no compatible GPU found | Use `--backend cpu` |
| GPU memory exceeded | Render (Step 3) | Error showing buffer sizes vs available memory | Reduce scene complexity or use CPU backend |
| Shader library not found | Render (Step 3) | Error showing expected path, suggesting rebuild | Rebuild the project |
| GPU timeout / watchdog | Render (Step 3) | Warning about automatic work splitting | No action needed; system self-heals |
| GPU produces NaN pixels | Render (Step 3) | Warning: N pixels contained NaN, replaced with magenta (debug) or black | Debug shader code for division by zero or invalid geometry |

---

## Future Considerations (Out of Scope)

These emerged during journey design but are explicitly deferred:

1. **`--backend auto` as default**: Once GPU rendering is proven stable and correct, `auto` should become the default backend. Deferred until correctness is validated across multiple scene types.
2. **`nwave compare` subcommand**: CPU vs GPU image comparison tool. Useful for validation but not required -- external tools (ImageMagick) suffice initially.
3. **Real-time preview window**: Metal supports rendering to a CAMetalLayer for interactive preview. Completely different journey; deferred.
4. **Multi-GPU support**: Splitting work across multiple Metal devices. Unnecessary for single-developer macOS workstation.
5. **Hybrid CPU+GPU rendering**: Using CPU for recursive/complex rays that GPU handles poorly. Optimization for later.
6. **GPU-accelerated BVH construction**: Building the BVH on GPU. CPU BVH build is already fast (<10ms for typical scenes).
7. **Texture memory management**: Streaming large textures to GPU. Depends on texture support being added first.
