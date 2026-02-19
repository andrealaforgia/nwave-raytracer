# GPU Compute Rendering -- User Stories

**Document ID**: US-GPU-COMPUTE-001
**Date**: 2026-02-17
**Status**: Draft -- Pending DoR Validation

---

## Phase 0: Walking Skeleton

### US-GPU-000: Render a Flat-Color Frame via Metal Compute

#### Problem (The Pain)
Elena Marchetti wants to learn about GPU compute rendering, but the nwave ray tracer has no GPU code path at all. Before she can trace rays on the GPU, she needs to confirm that the entire Metal pipeline works: device initialization, shader compilation, compute dispatch, texture readback, and PPM output. She finds it risky to build a complex ray tracing kernel without first proving the infrastructure works end-to-end.

#### Who (The User)
- CG student exploring GPU compute for the first time
- Working on a MacBook Pro with Apple M2 chip (macOS 14)
- Motivated by seeing any GPU-generated image appear as a valid PPM file

#### Solution (What We Build)
A minimal Metal compute pipeline that dispatches a trivial shader (horizontal color gradient based on pixel x-coordinate) to a GPU texture, reads the pixels back to CPU memory, and saves them as a PPM file. Triggered by `nwave render scene.yaml --backend=metal` but ignoring the scene entirely for this story.

#### Domain Examples

##### Example 1: Elena renders a gradient image via Metal
Elena runs `nwave render cornell.yaml --backend=metal --width 800` on her M2 MacBook Pro. Instead of the Cornell Box scene, she sees a horizontal gradient (black on the left, blue on the right) in the output file. The file is a valid PPM (P3, 800x450, all values 0-255). This confirms the Metal pipeline works end-to-end.

##### Example 2: Elena runs the Metal backend on her Linux lab machine
Elena tries `nwave render scene.yaml --backend=metal` on the university's Ubuntu server. The program prints "Error: Metal backend is only available on macOS" and exits with code 1. No crash, no PPM file. The CPU renderer still works normally with `nwave render scene.yaml`.

##### Example 3: Elena compares GPU gradient timing
Elena renders the 3840x2160 gradient via Metal and observes it completes in under 1 second (including Metal device init, shader compile, dispatch, readback, and PPM write). She notes this baseline for future comparison against CPU rendering.

#### UAT Scenarios (BDD)

##### Scenario: Metal compute produces a valid PPM gradient image
```
Given Elena has compiled nwave on macOS with Metal support enabled
  And she has any valid scene file "test.yaml"
When Elena runs "nwave render test.yaml --backend=metal --width 800 -o gradient.ppm"
Then a file "gradient.ppm" is created
  And the file begins with "P3" followed by "800 450" and "255"
  And every RGB value is an integer between 0 and 255
  And the leftmost column pixels (x=0) are approximately black (R < 10, G < 10, B < 10)
  And the rightmost column pixels (x=799) are approximately blue (R < 10, G < 10, B > 200)
```

##### Scenario: Metal backend fails gracefully on non-macOS
```
Given Elena has compiled nwave on Linux (without Metal support)
When Elena runs "nwave render test.yaml --backend=metal"
Then the program prints an error message containing "Metal backend is only available on macOS"
  And the exit code is 1
  And no output file is created
```

##### Scenario: Default backend remains CPU
```
Given Elena has compiled nwave on macOS with Metal support
When Elena runs "nwave render test.yaml --width 400" without --backend flag
Then the render uses the CPU renderer (existing behavior unchanged)
  And the output matches what the CPU renderer would produce
```

##### Scenario: Metal initialization completes within time budget
```
Given Elena runs the Metal backend on a Mac with a supported GPU
When the Metal device, command queue, and shader library are initialized
Then initialization completes in under 500ms
  And the compute shader dispatches without errors
  And total pipeline (init + dispatch + readback + PPM write) completes in under 1 second for 3840x2160
```

#### Acceptance Criteria
- [ ] `--backend=metal` CLI flag is recognized by CliDispatcher and passed through RenderCommand
- [ ] Metal device and command queue are initialized successfully on macOS
- [ ] A .metal compute shader compiles via CMake and dispatches a 2D compute grid
- [ ] GPU texture is allocated at the requested image dimensions
- [ ] Pixel data is read back from GPU to CPU memory as a vector<Color3>
- [ ] Output PPM file is valid and opens in image viewers
- [ ] On non-macOS, `--backend=metal` produces a clear error and exit code 1
- [ ] Default backend (no flag) remains CPU -- no behavioral change to existing commands

#### Technical Notes
- Metal device: `MTLCreateSystemDefaultDevice()` in Objective-C++
- Shader file: `src/infrastructure/shaders/gradient.metal` compiled to .metallib by CMake
- Threadgroup size: 16x16 (256 threads per group) is a good default for Metal compute
- Readback: `MTLTexture.getBytes()` or `MTLBuffer` with shared storage mode
- CMake: `add_custom_command` for `xcrun -sdk macosx metal` and `xcrun -sdk macosx metallib`
- Guard Metal code with `#ifdef __APPLE__` and CMake `if(APPLE)` checks

---

## Phase 1: Render Backend Abstraction

### US-GPU-001: Select Render Backend via CLI

#### Problem (The Pain)
Sofia Reyes wants to switch between CPU and GPU rendering for the same scene without changing her workflow. Currently the Renderer class is a concrete implementation with no abstraction point. She needs a clean interface so that `--backend=cpu` and `--backend=metal` both work through the same entry point, and her animation scripts do not need modification.

#### Who (The User)
- Technical artist rendering product scenes on macOS
- Switches between quick CPU previews (low SPP) and GPU production renders (high SPP)
- Uses AnimationRenderer for physics-driven sequences

#### Solution (What We Build)
A RenderBackend abstract interface in Ring 3 (Application) with `render(camera, scene, settings) -> vector<Color3>`. CpuRenderBackend wraps the existing Renderer. MetalRenderBackend wraps the Metal compute pipeline. Backend selection is driven by RenderSettings or CLI flag.

#### Domain Examples

##### Example 1: Sofia renders the same scene on CPU and GPU
Sofia runs `nwave render product.yaml --spp 48 --backend=cpu -o product_cpu.ppm` and then `nwave render product.yaml --spp 48 --backend=metal -o product_gpu.ppm`. Both commands produce valid PPM files of the same scene. The GPU version finishes dramatically faster.

##### Example 2: Sofia runs physics animation with GPU backend
Sofia runs `nwave render bouncing.yaml --physics-animate --backend=metal`. Each animation frame is rendered via Metal compute. The frames appear in the output directory with the same naming convention (frame_0000.ppm, frame_0001.ppm, ...).

##### Example 3: David uses default backend and gets CPU
David runs `nwave render scene.yaml --spp 8` without specifying a backend. The renderer uses the CPU backend, exactly as before this feature existed. Zero behavioral change.

#### UAT Scenarios (BDD)

##### Scenario: CPU backend produces identical output to existing renderer
```
Given Sofia has a scene file "spheres.yaml" with 3 spheres and 1 light
When Sofia renders with "--backend=cpu --spp 1 --width 400"
  And Sofia renders with the existing renderer (no --backend flag) at the same settings
Then both PPM files are byte-identical
```

##### Scenario: Backend flag is passed through AnimationRenderer
```
Given Sofia has a physics animation scene "bounce.yaml" with animation_config
When Sofia runs "nwave render bounce.yaml --physics-animate --backend=metal"
Then each frame is rendered using the Metal backend
  And frames are saved as frame_0000.ppm, frame_0001.ppm, etc.
  And progress is reported to stderr
```

##### Scenario: Unknown backend produces clear error
```
Given Sofia runs "nwave render scene.yaml --backend=vulkan"
When the CLI dispatcher processes the command
Then the program prints "Error: unknown backend 'vulkan'. Available: cpu, metal"
  And exits with code 1
```

##### Scenario: RenderSettings carries backend selection
```
Given Sofia specifies "--backend=metal" on the command line
When the RenderCommand is parsed and RenderSettings is constructed
Then settings.backend is set to "metal"
  And the render handler uses the MetalRenderBackend
```

#### Acceptance Criteria
- [ ] RenderBackend interface exists in Ring 3 with `render(camera, scene, settings) -> vector<Color3>`
- [ ] CpuRenderBackend wraps existing Renderer and produces identical output
- [ ] MetalRenderBackend is instantiated when `--backend=metal` is specified
- [ ] RenderCommand gains a `backend` string field (default: empty, meaning "cpu")
- [ ] AnimationRenderer's WriteCallback works with either backend
- [ ] Unknown backend values produce a clear error listing available backends
- [ ] Existing commands without `--backend` behave identically to before

#### Technical Notes
- RenderBackend is a pure virtual class in Ring 3 (Application)
- CpuRenderBackend delegates to existing Renderer::render()
- MetalRenderBackend lives in Ring 4 (Infrastructure) alongside other Metal code
- Factory function or simple conditional in main.cpp creates the appropriate backend
- RenderSettings gains: `std::string backend = ""` (empty = cpu default)

---

## Phase 2: Ray Generation and Sky on GPU

### US-GPU-002: Generate Camera Rays and Render Sky Gradient on GPU

#### Problem (The Pain)
Elena Marchetti has the Metal compute pipeline working (US-GPU-000 gradient), but it does not generate rays or use the camera. She wants to see the same sky gradient that the CPU renderer produces when there are no objects in the scene, confirming that camera ray generation on the GPU matches the CPU implementation exactly.

#### Who (The User)
- CG student validating GPU ray generation against known CPU output
- Wants pixel-identical sky gradient between CPU and GPU for an empty scene
- Working on macOS with Metal support

#### Solution (What We Build)
A Metal compute shader that receives camera parameters in a buffer, generates a primary ray per pixel using the same math as Camera::generate_ray(), and computes the sky gradient color (background_top/background_bottom blend based on ray Y direction). No scene intersection.

#### Domain Examples

##### Example 1: Elena compares GPU sky to CPU sky for an empty scene
Elena creates a scene YAML with a camera but no objects and no lights. She renders with `--backend=cpu` and `--backend=metal`. Both images show the identical blue-to-white sky gradient. She overlays them in GIMP and finds zero pixel difference.

##### Example 2: Elena changes the camera FOV and checks GPU output
Elena renders the empty scene with vfov=90 (wide angle) and vfov=20 (narrow). The GPU-rendered sky at vfov=20 shows a narrower band of the gradient (more zoomed in), matching the CPU output at vfov=20.

##### Example 3: Elena renders with a tilted camera
Elena sets vup=(1,0,0) to roll the camera 90 degrees. The GPU sky gradient rotates accordingly -- the gradient runs horizontally instead of vertically -- matching the CPU output.

#### UAT Scenarios (BDD)

##### Scenario: GPU sky gradient matches CPU pixel-for-pixel
```
Given Elena has a scene file with camera at (0, 0, -3) looking at (0, 0, 0), vfov=60, no objects, no lights
  And background_top = (0.5, 0.7, 1.0) and background_bottom = (1.0, 1.0, 1.0)
When Elena renders at 400x225, 1 SPP with "--backend=cpu -o sky_cpu.ppm"
  And Elena renders at 400x225, 1 SPP with "--backend=metal -o sky_gpu.ppm"
Then sky_cpu.ppm and sky_gpu.ppm are pixel-identical (same RGB values at every pixel)
```

##### Scenario: Camera parameters are correctly uploaded to GPU
```
Given Elena has a camera with lookfrom=(2, 3, 6), lookat=(0, 0, 0), vup=(0,1,0), vfov=38
When the camera buffer is packed for Metal upload
Then the GPU shader reconstructs the same orthonormal basis (u, v, w) as the CPU Camera class
  And pixel (0, 0) ray direction matches CPU Camera::generate_ray(0, 0)
  And pixel (399, 224) ray direction matches CPU Camera::generate_ray(399, 224)
```

##### Scenario: GPU sky render completes within time budget
```
Given Elena renders an empty scene at 3840x2160 via Metal
When the compute shader dispatches and completes
Then total render time (excluding Metal init) is under 50ms
```

#### Acceptance Criteria
- [ ] Camera parameters (lookfrom, lookat, vup, vfov, aspect_ratio, image dimensions, pixel deltas, pixel00_loc) are packed into a Metal buffer
- [ ] GPU compute shader generates a primary ray per pixel matching Camera::generate_ray() math
- [ ] Sky gradient computation on GPU matches CPU: `(1-a)*background_bottom + a*background_top` where `a = 0.5*(unit_direction.y + 1.0)`
- [ ] GPU-rendered empty scene is pixel-identical to CPU-rendered empty scene at 1 SPP
- [ ] Camera buffer layout is documented for future shader development

#### Technical Notes
- Camera buffer struct must match between C++ and Metal Shading Language
- Use `float` (not `double`) on GPU -- Metal does not support double precision in compute shaders. Camera math must be converted from double to float for the GPU path.
- GPU ray generation: `pixel_pos = pixel00 + px * delta_u + py * delta_v; direction = pixel_pos - lookfrom`
- Sky gradient formula: `a = 0.5 * (normalize(direction).y + 1.0); color = (1-a)*bottom + a*top`

---

## Phase 3: Scene Data Packing

### US-GPU-003: Flatten Scene into GPU-Uploadable Buffers

#### Problem (The Pain)
Elena Marchetti has GPU ray generation working (sky gradient), but she cannot test intersections because the Scene's shapes and materials use virtual dispatch (Shape*, Material*) and heap-allocated objects that are meaningless to the GPU. She needs a way to transform the CPU scene graph into flat, contiguous arrays of plain data that the GPU can read.

#### Who (The User)
- CG student learning about data-oriented design for GPU
- Wants to understand how virtual polymorphism translates to tagged unions
- Needs the flattener to be testable without a GPU (pure CPU data transformation)

#### Solution (What We Build)
A SceneFlattener that traverses a CPU Scene and produces three packed arrays: GPUShape[] (tagged union with ShapeType + parameters), GPUMaterial[] (tagged union with MaterialType + parameters), and GPULight[] (tagged union with LightType + parameters). Each GPUShape holds a material index into the GPUMaterial array. The flattener is a pure C++ transformation, testable in unit tests without Metal.

#### Domain Examples

##### Example 1: Elena flattens a scene with 3 spheres and 2 materials
Elena has a scene with a red Lambertian sphere, a chrome Metal sphere, and a glass Dielectric sphere, plus a gray Lambertian ground plane and one point light. The flattener produces: 4 GPUShapes (3 spheres + 1 plane), 4 GPUMaterials (red Lambertian, chrome Metal, glass Dielectric, gray Lambertian), and 1 GPULight (point). Each sphere's material_index points to the correct entry in the materials array.

##### Example 2: Elena flattens a scene with a TransformedShape
Elena has a Box wrapped in a TransformedShape (rotated 45 degrees around Y). The flattener stores the Box parameters and the 4x4 transformation matrix in the GPUShape entry. The shape_type is tagged as TransformedBox.

##### Example 3: Elena flattens an empty scene
Elena has a scene with no shapes and no lights (just a camera). The flattener produces zero-length arrays. The GPU renderer handles this gracefully by rendering only the sky gradient.

#### UAT Scenarios (BDD)

##### Scenario: Flattener produces correct shape data for spheres
```
Given Elena has a scene with a Sphere at center (1, 2, 3) with radius 0.5 and Lambertian material (albedo 0.8, 0.2, 0.1)
When the SceneFlattener processes the scene
Then the GPUShape array has 1 entry with shape_type = Sphere
  And the sphere's center is (1, 2, 3) and radius is 0.5
  And the sphere's material_index points to a GPUMaterial with type = Lambertian and albedo (0.8, 0.2, 0.1)
```

##### Scenario: Flattener handles all built-in shape types
```
Given Elena has a scene with one Sphere, one Plane, one Box, one Cylinder, and one Triangle
When the SceneFlattener processes the scene
Then the GPUShape array has 5 entries
  And each entry has the correct shape_type tag
  And each entry's geometric parameters match the CPU shape's parameters
```

##### Scenario: Material indices are deduplicated correctly
```
Given Elena has a scene where 3 spheres share the same Lambertian material instance
When the SceneFlattener processes the scene
Then the GPUMaterial array has 1 entry (not 3)
  And all 3 GPUShapes have the same material_index
```

##### Scenario: Flattener is testable without Metal
```
Given Elena writes a unit test for SceneFlattener
When the test constructs a Scene, runs SceneFlattener, and inspects the output arrays
Then the test compiles and passes on Linux (no Metal dependency)
  And the GPUShape, GPUMaterial, GPULight structs are plain C++ (no Metal types)
```

#### Acceptance Criteria
- [ ] GPUShape tagged union covers: Sphere, Plane, Box, Cylinder, Triangle shape types
- [ ] GPUMaterial tagged union covers: Lambertian, Metal, Dielectric, Emissive material types
- [ ] GPULight tagged union covers: Point, Directional light types
- [ ] Each GPUShape has a material_index into the GPUMaterial array
- [ ] SceneFlattener deduplicates shared material instances
- [ ] TransformedShape stores a transformation matrix alongside the base shape parameters
- [ ] Empty scenes produce zero-length arrays without errors
- [ ] GPUShape, GPUMaterial, GPULight are plain C++ structs with no Metal dependencies (testable on Linux)
- [ ] Structs are packed to 16-byte alignment (Metal buffer alignment requirement)

#### Technical Notes
- ShapeType enum: Sphere=0, Plane=1, Box=2, Cylinder=3, Triangle=4
- MaterialType enum: Lambertian=0, Metal=1, Dielectric=2, Emissive=3
- LightType enum: Point=0, Directional=1
- Material deduplication: use a map<const Material*, uint32_t> during traversal
- TransformedShape: store inverse_transform for ray transformation on GPU
- Metal requires 16-byte aligned buffers; use `alignas(16)` or padding in struct layout
- TriangleMesh is out of scope for this story (requires separate vertex/index strategy)

---

## Phase 4: Single-Bounce GPU Rendering

### US-GPU-004: Render a Recognizable Scene on GPU with Diffuse Shading

#### Problem (The Pain)
Sofia Reyes has been watching the GPU pipeline take shape but has not yet seen a real rendered scene come out of the GPU. She has GPU ray generation (sky) and packed scene data (buffers), but the compute shader does not yet intersect rays with shapes or shade them. She wants to see her three-sphere scene rendered by the GPU -- even if it is just single-bounce diffuse shading -- to confirm the intersection and shading math works on the GPU.

#### Who (The User)
- Technical artist who wants visual confirmation that GPU rendering produces correct geometry and shading
- Comparing GPU output against the known-good CPU output for the same scene
- Using a simple scene (few objects) to make visual comparison easy

#### Solution (What We Build)
A Metal compute shader that performs brute-force ray-scene intersection (linear scan over GPUShape array), computes Lambertian diffuse shading with direct lighting from point lights, casts shadow rays, and writes the resulting color to the output texture. Single bounce only (no recursion/iteration for reflected or refracted rays).

#### Domain Examples

##### Example 1: Sofia renders 3 spheres on a plane via GPU
Sofia renders her classic scene: red Lambertian sphere, blue Lambertian sphere, gray ground plane, one white point light overhead. The GPU image shows recognizable spheres with diffuse shading (bright on the side facing the light, dark on the opposite side), a visible ground plane, and shadows. The overall appearance matches the CPU-rendered 1-SPP image.

##### Example 2: Sofia renders a scene with shadows on GPU
Sofia places a small sphere between the light and a larger sphere. The GPU image shows the small sphere casting a shadow on the large sphere, confirming shadow rays work on the GPU.

##### Example 3: Sofia renders an empty scene on GPU (regression check)
Sofia renders a scene with no objects. The GPU produces the sky gradient, confirming that the intersection miss path still works after adding intersection code.

#### UAT Scenarios (BDD)

##### Scenario: GPU renders spheres with correct diffuse shading
```
Given Sofia has a scene with a red sphere (Lambertian albedo 0.7, 0.1, 0.1) at (0, 0, 0)
  And a gray ground plane (Lambertian albedo 0.5, 0.5, 0.5) at y = -0.5
  And a point light at (-2, 3, -1) with white color and intensity 1.0
  And the camera at (0, 0, -1.5) looking at (0, 0, 0)
When Sofia renders at 400x225, 1 SPP with "--backend=metal"
Then the center pixel region shows a red-dominant color (R > G and R > B)
  And the sphere has shading variation (brighter toward the light, darker away)
  And the ground plane is visible below the sphere as gray pixels
  And pixels above the sphere show the sky gradient
```

##### Scenario: GPU shadow rays block light correctly
```
Given Sofia has a scene with a large sphere and a small sphere between it and the light
When Sofia renders via GPU at 1 SPP
Then the large sphere shows a dark region where the small sphere blocks the light
  And the shadow region is darker than the directly-lit regions of the large sphere
```

##### Scenario: GPU and CPU produce visually comparable single-bounce renders
```
Given Sofia renders the three-sphere scene at 400x225, 1 SPP with "--backend=cpu"
  And renders the same scene at 400x225, 1 SPP with "--backend=metal"
When Sofia compares both images
Then the sphere positions, sizes, and silhouettes match
  And the shading pattern (light/dark sides) matches
  And colors are within +/-10 per channel for most pixels (allowing for float vs double precision differences)
```

#### Acceptance Criteria
- [ ] GPU compute shader intersects rays with Sphere, Plane, and Box shapes
- [ ] Intersection math matches CPU implementations (quadratic for sphere, dot product for plane, slab for box)
- [ ] Lambertian diffuse shading: `color = albedo * max(0, dot(N, L)) * light_intensity`
- [ ] Shadow rays are cast from hit points toward each light with epsilon offset
- [ ] GPU brute-force scan finds the closest intersection (smallest positive t)
- [ ] Miss rays produce sky gradient (regression from US-GPU-002)
- [ ] Ambient term (0.05 * albedo) matches CPU renderer
- [ ] GPU output is visually comparable to CPU output for simple scenes at 1 SPP

#### Technical Notes
- Sphere intersection: quadratic formula `b^2 - 4ac`, same as Sphere::hit()
- Plane intersection: `t = dot(point - origin, normal) / dot(direction, normal)`
- Shadow ray: `origin = hit_point + 0.001 * normal; direction = toward_light`
- GPU uses float precision; CPU uses double. Small per-pixel differences expected.
- GPU PCG random number generator seeded per pixel (pixel_index + frame_seed)
- Brute-force intersection: iterate shapes[0..N-1], track closest t_hit

---

## Phase 5: Multi-Bounce Ray Tracing

### US-GPU-005: Iterative Multi-Bounce Ray Tracing on GPU

#### Problem (The Pain)
David Okonkwo has single-bounce GPU rendering working, but his chrome metal sphere appears flat black (no reflections) and his glass sphere is opaque (no refraction). The CPU renderer uses recursive trace_ray() to follow reflected and refracted rays for up to 10 bounces, producing the characteristic mirror reflections and glass transparency. The GPU cannot use recursion, so David needs an iterative equivalent that accumulates color across bounces.

#### Who (The User)
- Hobbyist 3D artist who wants to see reflections and refractions on GPU-rendered scenes
- Expects GPU output to match CPU output visually for all material types
- Scenes include Metal, Dielectric, and Emissive materials alongside Lambertian

#### Solution (What We Build)
An iterative ray tracing loop in the Metal compute shader. Instead of recursion, the shader uses a loop (up to max_depth iterations) with explicit accumulation variables: `throughput` (color attenuation so far) and `accumulated_color` (emitted + direct light contributions). Each bounce updates the ray origin/direction, applies material scatter/reflect/refract, and reduces throughput by the material's attenuation.

#### Domain Examples

##### Example 1: David sees reflections in a chrome sphere on GPU
David renders a scene with a chrome Metal sphere (albedo 0.9, 0.9, 0.9; fuzziness 0) next to a red Lambertian sphere. The GPU image shows the red sphere reflected in the chrome sphere surface, matching the CPU output. The reflection is sharp and undistorted.

##### Example 2: David sees glass refraction on GPU
David renders a glass sphere (Dielectric IOR 1.5) in front of a striped background. The GPU image shows the background visible through the glass, distorted by refraction, with edge reflections from Schlick's approximation. The effect matches the CPU render.

##### Example 3: David sees emissive glow on GPU
David renders a scene with an emissive sphere (bright white, intensity 3.0) next to a Lambertian wall. The GPU image shows the emissive sphere glowing and the wall showing color bleeding from the emissive light, matching the CPU render.

#### UAT Scenarios (BDD)

##### Scenario: Metal reflection works on GPU
```
Given David has a chrome Metal sphere (albedo 0.9, 0.9, 0.9; fuzziness 0) at (-1, 0, 0)
  And a red Lambertian sphere at (1, 0, 0)
  And a point light at (0, 5, -3)
  And the camera at (0, 1, -3) looking at (0, 0, 0)
When David renders at 400x225, 1 SPP with "--backend=metal"
Then the chrome sphere surface shows a recognizable reflection of the red sphere
  And the reflection color has red-dominant hue (from the reflected Lambertian sphere)
```

##### Scenario: Glass refraction works on GPU
```
Given David has a Dielectric sphere (IOR 1.5) at (0, 0, 0)
  And a colored background visible behind the sphere
  And max_depth = 10
When David renders at 400x225, 16 SPP with "--backend=metal"
Then objects behind the glass sphere are visible but distorted (refracted)
  And the glass sphere edges show stronger reflection than the center (Fresnel)
```

##### Scenario: Iterative depth limit matches CPU recursive depth limit
```
Given David has two Metal spheres facing each other with max_depth = 5
When David renders with "--backend=metal"
Then reflections are visible up to 5 levels
  And the deepest reflection returns black (loop terminated)
When David renders with "--backend=cpu" at the same max_depth
Then the GPU and CPU images show the same number of visible reflection bounces
```

##### Scenario: All four material types work on GPU
```
Given David has a scene with one Lambertian, one Metal, one Dielectric, and one Emissive sphere
When David renders at 400x225, 16 SPP with "--backend=metal"
Then the Lambertian sphere shows matte diffuse shading
  And the Metal sphere shows reflections
  And the Dielectric sphere shows refraction and transparency
  And the Emissive sphere appears bright/glowing
```

#### Acceptance Criteria
- [ ] GPU ray tracing uses an iterative loop (not recursion) with explicit throughput accumulation
- [ ] Metal material: reflect(I, N) with fuzziness perturbation using GPU RNG
- [ ] Dielectric material: Snell's law refraction, Schlick's reflectance, total internal reflection
- [ ] Emissive material: contributes emitted color, does not scatter
- [ ] Lambertian material: random hemisphere scatter with GPU RNG
- [ ] max_depth is respected; loop terminates and returns black when exhausted
- [ ] Multi-light support: each hit point accumulates contributions from all lights
- [ ] GPU-rendered scenes with all material types are visually comparable to CPU output at same SPP

#### Technical Notes
- Iterative loop pattern:
  ```
  color = (0,0,0); throughput = (1,1,1);
  for (bounce = 0; bounce < max_depth; bounce++) {
      if (hit) {
          color += throughput * emitted;
          color += throughput * direct_light_contribution;
          throughput *= attenuation;
          ray = scattered_ray;
      } else {
          color += throughput * sky_color;
          break;
      }
  }
  ```
- GPU RNG: PCG (Permuted Congruential Generator) seeded per pixel per sample
- Schlick's: `F0 = ((n1-n2)/(n1+n2))^2; F = F0 + (1-F0)*(1-cos_theta)^5`
- Refraction: `eta = front_face ? (1.0/ior) : ior; sin2_theta_t = eta*eta*(1 - cos^2); if sin2_theta_t > 1.0 then reflect`

---

## Phase 6: Linear BVH on GPU

### US-GPU-006: GPU-Accelerated BVH Traversal for Complex Scenes

#### Problem (The Pain)
Sofia Reyes's 500-sphere scene takes far too long even on the GPU because the compute shader tests every ray against every shape (brute-force linear scan). With 500 shapes, 3840x2160 pixels, and 48 SPP, that is 500 x 8M x 48 = 192 billion intersection tests. She needs the GPU to skip shapes that are clearly not in the ray's path, just like the CPU's BVH does.

#### Who (The User)
- Technical artist rendering complex production scenes on GPU
- Expects GPU render time to scale sub-linearly with scene complexity
- Needs pixel-identical results between brute-force and BVH GPU rendering

#### Solution (What We Build)
Build the BVH on the CPU (reusing the existing BVH construction code), flatten it into a linear array of BVH nodes (each node stores AABB bounds and either primitive indices for leaves or child node offsets for interior nodes), upload to a Metal buffer, and traverse it in the compute shader using an explicit stack.

#### Domain Examples

##### Example 1: Sofia's 500-sphere scene renders 10x faster with GPU BVH
Sofia renders her 500-sphere scene via GPU without BVH: 15 seconds. She enables GPU BVH and re-renders: under 2 seconds. Both images are visually identical.

##### Example 2: Sofia's 3-sphere scene has minimal BVH overhead
Sofia renders a simple 3-sphere scene with and without GPU BVH. Both take approximately the same time (BVH overhead is negligible for small scenes). Images are identical.

##### Example 3: BVH handles a scene with all shapes at the same position
Sofia creates 100 spheres all at center (0,0,0). The BVH construction completes without error. The GPU render shows the front-most sphere, matching CPU output.

#### UAT Scenarios (BDD)

##### Scenario: GPU BVH produces identical images to GPU brute-force
```
Given Sofia has a scene with 100 random spheres with varied materials and positions
When Sofia renders with GPU brute-force (no BVH)
  And Sofia renders with GPU BVH enabled
Then both images are pixel-identical
```

##### Scenario: GPU BVH provides measurable speedup on large scenes
```
Given Sofia has a scene with 500 spheres
When she renders at 800x450, 1 SPP via GPU without BVH and records the time
  And she renders the same scene with GPU BVH and records the time
Then the GPU BVH render is at least 5x faster than GPU brute-force
```

##### Scenario: Linear BVH node layout is correct
```
Given Sofia has a scene with 10 spheres
When the CPU builds the BVH and flattens it into a linear array
Then interior nodes store AABB bounds and an offset to the second child
  And leaf nodes store AABB bounds, a primitive offset, and a primitive count
  And the flattened array can be traversed top-down using index arithmetic (no pointers)
```

##### Scenario: GPU BVH traversal stack does not overflow
```
Given Sofia has a scene with 10000 shapes producing a deep BVH tree
When the GPU traverses the BVH
Then the fixed-size stack (64 entries) is sufficient for the tree depth
  And no stack overflow occurs
  And the render completes correctly
```

#### Acceptance Criteria
- [ ] CPU builds BVH from Scene shapes using existing construction algorithm
- [ ] BVH is flattened into a contiguous array of LinearBVHNode structs
- [ ] LinearBVHNode contains: AABB (min, max), primitive offset, primitive count (leaf), second child offset (interior)
- [ ] Linear BVH array is uploaded to a Metal buffer
- [ ] GPU compute shader traverses BVH using a fixed-size stack (64 entries)
- [ ] GPU BVH traversal produces identical intersection results to GPU brute-force
- [ ] GPU BVH provides at least 5x speedup for 500+ shape scenes
- [ ] Edge cases handled: empty scene, single shape, degenerate BVH (all shapes at same position)

#### Technical Notes
- LinearBVHNode struct (GPU-compatible):
  ```
  struct LinearBVHNode {
      float3 aabb_min;
      float3 aabb_max;
      uint32_t offset;      // leaf: first primitive index; interior: second child index
      uint32_t count;       // leaf: primitive count (>0); interior: 0
  };
  ```
- Traversal: push root; while stack not empty, pop node, test AABB, if leaf test primitives, if interior push children
- First child is always node_index + 1 (implicit); second child stored in offset field
- AABB test uses the slab method on GPU (same as CPU)
- Shapes referenced by index into the flat GPUShape array

---

## Phase 7: SPP Accumulation

### US-GPU-007: Multi-Sample Anti-Aliasing on GPU

#### Problem (The Pain)
Sofia Reyes's GPU renders look sharp in geometry but noisy and aliased because the compute shader traces only 1 ray per pixel. She needs the same multi-sample anti-aliasing that the CPU provides (random sub-pixel jittering, N samples averaged per pixel) to produce production-quality images on the GPU.

#### Who (The User)
- Technical artist producing final-quality product renders via GPU
- Needs GPU renders at 48 SPP to match the quality of CPU renders at 48 SPP
- Expects render time to scale linearly with SPP on the GPU

#### Solution (What We Build)
Extend the GPU compute shader with a per-pixel sample loop. Each sample uses GPU RNG to jitter the ray origin within the pixel area. Colors are accumulated and averaged. Gamma correction (sqrt) is applied after averaging. NaN/infinity values are clamped.

#### Domain Examples

##### Example 1: Sofia compares GPU at 1 SPP vs. 48 SPP
Sofia renders her product scene via GPU at 1 SPP (noisy, aliased edges) and 48 SPP (smooth, anti-aliased). The 48 SPP image shows dramatically smoother edges, cleaner reflections, and less noise in glass refractions.

##### Example 2: Sofia verifies GPU SPP quality matches CPU SPP quality
Sofia renders the same scene at 48 SPP on CPU and GPU. She overlays the images and finds them visually indistinguishable at a normal viewing distance. Per-pixel differences are within +/-5 per channel (statistical convergence).

##### Example 3: Sofia observes linear GPU time scaling with SPP
Sofia renders at 1, 8, 16, and 48 SPP on GPU and records times. Each doubling of SPP roughly doubles render time, confirming linear scaling.

#### UAT Scenarios (BDD)

##### Scenario: GPU multi-sample reduces aliasing
```
Given Sofia renders a scene with a sphere against a contrasting background
When she renders at 1 SPP via GPU
Then sphere edges show visible staircase/jagged pixels
When she renders at 48 SPP via GPU
Then sphere edges show smooth anti-aliased transitions
```

##### Scenario: GPU gamma correction matches CPU
```
Given Sofia renders a gray sphere (albedo 0.5, 0.5, 0.5) under uniform lighting at 48 SPP
When she renders via GPU and via CPU
Then both images have comparable center-pixel brightness (within +/-5 per channel)
  And gamma correction (sqrt) is applied after sample averaging on both
```

##### Scenario: GPU handles NaN and infinity gracefully
```
Given Sofia renders a scene that produces degenerate rays (e.g., near-zero direction vectors)
When the GPU shader encounters NaN or infinity in a color computation
Then that sample contributes (0, 0, 0) instead of NaN
  And the final pixel color is valid (all channels in [0, 255])
```

##### Scenario: GPU SPP scales linearly with render time
```
Given Sofia renders at 1 SPP and records GPU render time T1
  And renders at 8 SPP and records GPU render time T8
When she compares T8 to T1
Then T8 is approximately 8 * T1 (within 20% tolerance for dispatch overhead)
```

#### Acceptance Criteria
- [ ] GPU compute shader loops N times per pixel (N = settings.samples_per_pixel)
- [ ] Each sample jitters the ray origin within the pixel area using GPU RNG
- [ ] Final pixel color is the arithmetic mean of all sample colors
- [ ] Gamma correction (sqrt(clamp(color, 0, 1))) is applied after averaging
- [ ] NaN and infinity values are clamped to zero before accumulation
- [ ] GPU at N SPP produces image quality comparable to CPU at N SPP
- [ ] Render time scales approximately linearly with SPP

#### Technical Notes
- Per-pixel loop: `for (s = 0; s < spp; s++) { jitter ray; trace; accumulate; } color /= spp;`
- Jitter: `u_offset = pcg_random_float(rng); v_offset = pcg_random_float(rng);`
- PCG state must be unique per pixel per sample: seed = `pixel_index * spp + sample_index + frame_seed`
- Gamma: `output = sqrt(clamp(color, 0.0, 1.0))` per channel, then scale to [0, 255]
- NaN guard: `if (color.x != color.x) color.x = 0;` (NaN != NaN is true)

---

## Phase 8: Animation Integration

### US-GPU-008: Physics Animation Rendering via GPU Backend

#### Problem (The Pain)
Sofia Reyes uses the `--physics-animate` flag to render 300-frame physics simulations. Each frame takes 2+ minutes on CPU at low quality (1 SPP). A full sequence takes 10+ hours. She wants to render animation frames via GPU to reduce per-frame time to seconds, making physics animation practical for iterating on simulation parameters.

#### Who (The User)
- Technical artist rendering physics-driven animation sequences
- Currently waiting 10+ hours for a 300-frame CPU animation at low SPP
- Wants GPU rendering per frame to cut total animation time to minutes

#### Solution (What We Build)
Wire the GPU backend into AnimationRenderer's WriteCallback so that each frame is rendered via Metal compute. The scene is re-flattened and re-uploaded to the GPU each frame (physics changes object positions). Frame output uses the existing PPM naming convention.

#### Domain Examples

##### Example 1: Sofia renders a bouncing-ball animation on GPU
Sofia runs `nwave render bounce.yaml --physics-animate --backend=metal --spp 8`. The animation renders 300 frames, each via GPU. Total time: under 10 minutes (compared to 10+ hours on CPU). Frames appear in the output directory with correct naming.

##### Example 2: Sofia verifies frame 0 matches between CPU and GPU
Sofia renders just the first frame of the animation via CPU and GPU at the same SPP. The images are visually comparable -- same ball position, same lighting, same materials.

##### Example 3: Sofia monitors per-frame progress during GPU animation
Sofia observes progress output during GPU animation: "Frame 1/300 ... Frame 2/300 ..." printed to stderr, same as CPU animation. She can estimate remaining time based on per-frame GPU render time.

#### UAT Scenarios (BDD)

##### Scenario: GPU animation produces correctly named frame files
```
Given Sofia has a physics animation scene "bounce.yaml" with 60 frames
When Sofia runs "nwave render bounce.yaml --physics-animate --backend=metal -o frames/"
Then 60 PPM files are created: frame_0000.ppm through frame_0059.ppm
  And each file is a valid PPM with correct dimensions
  And frame_0000.ppm shows the initial scene state
```

##### Scenario: Scene is re-uploaded each frame for physics updates
```
Given Sofia has a physics animation where a sphere falls from y=5 to y=0 over 30 frames
When Sofia renders via GPU backend
Then frame_0000.ppm shows the sphere at y=5
  And frame_0029.ppm shows the sphere near y=0
  And intermediate frames show the sphere at intermediate positions
  And the GPU buffers are updated each frame to reflect physics changes
```

##### Scenario: Progress reporting works with GPU backend
```
Given Sofia runs a 100-frame GPU animation
When frames are rendering
Then progress is reported to stderr (e.g., "Frame 1/100", "Frame 2/100", ...)
  And progress updates appear at least once per frame
```

##### Scenario: GPU animation matches CPU animation visually
```
Given Sofia renders frame 15 of a physics animation via CPU at 8 SPP
  And renders frame 15 of the same animation via GPU at 8 SPP
When Sofia compares both frame_0015.ppm files
Then the sphere positions match (same physics simulation)
  And the shading and materials are visually comparable
```

#### Acceptance Criteria
- [ ] `--physics-animate --backend=metal` triggers GPU rendering for each animation frame
- [ ] SceneFlattener re-runs each frame to capture physics-updated positions
- [ ] GPU buffers are re-uploaded each frame with updated scene data
- [ ] Frame files follow existing naming convention (frame_NNNN.ppm)
- [ ] Progress reporting to stderr works identically to CPU animation
- [ ] Per-frame GPU render time is significantly faster than per-frame CPU render time
- [ ] First and last frames show correct physics states (initial and final positions)

#### Technical Notes
- WriteCallback signature remains unchanged: `void(filename, scene, camera, width, spp)`
- Inside the GPU WriteCallback: flatten scene -> upload buffers -> dispatch compute -> readback -> write PPM
- Scene re-flattening cost is expected to be small (<10ms for 500 objects) relative to GPU render time
- GPU device and command queue can be reused across frames (initialize once, reuse)
- Consider buffer pooling: allocate GPU buffers once at max scene size, update contents each frame
