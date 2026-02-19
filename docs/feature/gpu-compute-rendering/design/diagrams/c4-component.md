# C4 Component Diagram: GPU Compute Rendering

**Document ID**: DIAG-GPU-001
**Feature**: gpu-compute-rendering
**Date**: 2026-02-18
**Status**: Draft

---

## 1. System Context (C4 Level 1)

```
+-------------------+
|    Developer      |
|  (CLI user)       |
+--------+----------+
         |
         | nwave render scene.yaml --backend=metal
         v
+--------+----------+
|  nwave-raytracer  |
|  (single binary)  |
+--------+----------+
         |
         | reads: scene.yaml
         | writes: output.ppm, frames/*.ppm
         v
+--------+----------+
|  File System      |
|  (YAML in,        |
|   PPM out)        |
+-------------------+
```

---

## 2. Container Diagram (C4 Level 2)

The system is a single executable (monolith). This level shows the logical containers within it.

```
+================================================================+
|                     nwave (single executable)                    |
|                                                                  |
|  +------------------+    +------------------+                    |
|  | CPU Render Path  |    | GPU Render Path  |                    |
|  | (std::thread     |    | (Metal compute   |                    |
|  |  parallelism)    |    |  shader)         |                    |
|  +--------+---------+    +--------+---------+                    |
|           |                       |                              |
|           |   both produce        |                              |
|           +----->  vector<Color3> <------+                       |
|                         |                                        |
|                         v                                        |
|                  +------+------+                                 |
|                  | PPM Writer  |                                 |
|                  +------+------+                                 |
|                         |                                        |
|                    output.ppm                                    |
|                                                                  |
|  +------------------+    +------------------+                    |
|  | Scene Loading    |    | Physics Engine   |                    |
|  | (yaml-cpp)       |    | (Jolt Physics)   |                    |
|  +------------------+    +------------------+                    |
|                                                                  |
|  [macOS only] Metal.framework, Foundation.framework              |
|  [macOS only] nwave_shaders.metallib                             |
+================================================================+
```

---

## 3. Component Diagram (C4 Level 3)

### 3.1 GPU Rendering Components by Ring

```
+================================================================+
|                     Ring 4: Infrastructure                       |
|                                                                  |
|  +-----------------------------------------------------------+  |
|  | Metal Components (macOS only, .mm files)                   |  |
|  |                                                            |  |
|  |  +-------------------+                                     |  |
|  |  | MetalRenderBackend|---implements---> RenderBackend (R3) |  |
|  |  +--------+----------+                                     |  |
|  |           |                                                |  |
|  |           | uses                                           |  |
|  |           v                                                |  |
|  |  +--------+----------+    +-------------------+            |  |
|  |  | MetalDevice       |    | MetalBufferManager|            |  |
|  |  | (device, queue,   |    | (upload, readback)|            |  |
|  |  |  pipeline)        |    |                   |            |  |
|  |  +-------------------+    +-------------------+            |  |
|  |           |                        |                       |  |
|  |           | dispatches             | manages               |  |
|  |           v                        v                       |  |
|  |  +-------------------+    +-------------------+            |  |
|  |  | ray_trace.metal   |    | MTLBuffer objects |            |  |
|  |  | (compute kernel)  |    | (GPU memory)      |            |  |
|  |  +-------------------+    +-------------------+            |  |
|  +-----------------------------------------------------------+  |
|                                                                  |
|  +-----------------------------------------------------------+  |
|  | GPU Data Components (pure C++, all platforms)              |  |
|  |                                                            |  |
|  |  +-------------------+    +-------------------+            |  |
|  |  | SceneFlattener    |    | BVHFlattener      |            |  |
|  |  | (Scene -> GPU     |    | (shapes -> linear |            |  |
|  |  |  structs)         |    |  BVH array)       |            |  |
|  |  +--------+----------+    +--------+----------+            |  |
|  |           |                        |                       |  |
|  |           | produces               | produces              |  |
|  |           v                        v                       |  |
|  |     GPUShape[]               LinearBVHNode[]               |  |
|  |     GPUMaterial[]            (Ring 1 types)                |  |
|  |     GPULight[]                                             |  |
|  +-----------------------------------------------------------+  |
|                                                                  |
|  [EXISTING] CliDispatcher, YamlSceneLoader, Validator, etc.     |
+================================================================+

+================================================================+
|                     Ring 3: Application                          |
|                                                                  |
|  +-------------------+                                          |
|  | RenderBackend     |<---------- abstract interface             |
|  | (pure virtual)    |                                          |
|  +--------+----------+                                          |
|           ^          ^                                          |
|           |          |                                          |
|  +--------+--+  +---+-------------------+                       |
|  | CpuRender |  | MetalRenderBackend    |                       |
|  | Backend   |  | (Ring 4 implements)   |                       |
|  +--------+--+  +-----------------------+                       |
|           |                                                      |
|           | delegates to                                         |
|           v                                                      |
|  +-------------------+    +-------------------+                  |
|  | Renderer          |    | AnimationRenderer |                  |
|  | (existing, no     |    | (existing,        |                  |
|  |  changes)         |    |  WriteCallback    |                  |
|  +-------------------+    |  uses backend)    |                  |
|                           +-------------------+                  |
+================================================================+

+================================================================+
|                     Ring 2: Domain                               |
|                                                                  |
|  [EXISTING, UNCHANGED]                                          |
|  Shape hierarchy: Sphere, Plane, Box, Cylinder, Triangle,       |
|                   TransformedShape                               |
|  Material hierarchy: Lambertian, Metal, Dielectric, Emissive    |
|  Light hierarchy: PointLight, DirectionalLight                   |
|  Scene, Camera, HitRecord                                       |
+================================================================+

+================================================================+
|                     Ring 1: Core / Math                          |
|                                                                  |
|  [EXISTING] Vec3, Point3, Color3, Ray, AABB, Matrix4x4, etc.   |
|  [NEW] gpu_types.h: GPUShape, GPUMaterial, GPULight,            |
|         GPUCamera, LinearBVHNode (plain C structs, no Metal)    |
+================================================================+
```

### 3.2 Data Flow Through Components

```
scene.yaml
    |
    v
YamlSceneLoader (R4) -----> Scene + Camera (R2)
    |                              |
    v                              |
CliDispatcher (R4) -----> --backend=metal
    |                              |
    v                              v
main.cpp: create MetalRenderBackend
    |
    | backend->render(camera, scene, settings)
    v
MetalRenderBackend (R4)
    |
    |--- SceneFlattener.flatten(scene) ---------> FlatScene {
    |                                               GPUShape[],
    |                                               GPUMaterial[],
    |                                               GPULight[] }
    |
    |--- BVHFlattener.build_and_flatten(shapes) -> LinearBVHNode[]
    |
    |--- Pack GPUCamera from Camera params ------> GPUCamera
    |
    |--- MetalBufferManager.upload(all) ---------> MTLBuffer[6]
    |
    |--- MetalDevice.dispatch_compute() ---------> GPU execution
    |                                               (ray_trace.metal)
    |
    |--- MetalBufferManager.readback() ----------> vector<Color3>
    |
    v
PPMWriter.write(filename, pixels) -------> output.ppm
```

---

## 4. Backend Selection Flow

```
CLI: nwave render scene.yaml --backend=X
    |
    v
CliDispatcher.handle_render()
    |
    | RenderCommand { backend: "metal" }
    v
main.cpp: select_backend(command.backend)
    |
    +--- "cpu" or "" -----> CpuRenderBackend (wraps Renderer)
    |
    +--- "metal" ---------->
    |       |
    |       +--- #ifdef NWAVE_HAS_METAL ---> MetalRenderBackend
    |       |
    |       +--- #else ---> Error: "Metal backend not available"
    |
    +--- unknown ----------> Error: "Unknown backend 'X'. Available: cpu, metal"
    |
    v
RenderBackend* backend  (polymorphic dispatch)
    |
    | backend->render(camera, scene, settings)
    v
vector<Color3> pixels
```

---

## 5. Animation Integration Flow

```
main.cpp: run_physics_animate(cmd)
    |
    | create backend based on cmd.backend
    v
WriteCallback = [backend](filename, scene, camera, width, spp) {
    |
    | RenderSettings settings;
    | settings.samples_per_pixel = spp;
    |
    | auto pixels = backend->render(camera, scene, settings);
    | write_ppm(filename, pixels, ...);
};
    |
    v
AnimationRenderer.render()
    |
    | for each frame:
    |   physics.step()
    |   update TransformedShape transforms
    |   write_callback_(filename, scene, camera, width, spp)
    |       |
    |       v
    |   backend->render()  <-- CPU or GPU, transparent
    |       |
    |       v
    |   write_ppm()
    v
frames/frame_0000.ppm ... frame_NNNN.ppm
```

The WriteCallback closure captures the RenderBackend pointer. AnimationRenderer does not know or care which backend is being used. This is the key architectural decision: the integration point is at the WriteCallback level, not inside AnimationRenderer.
