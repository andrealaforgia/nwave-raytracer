# Root Cause Analysis: Procedural Equirectangular Texture Seam Artifacts

**Date:** 2026-02-22
**Investigator:** Rex (Root Cause Analysis Specialist)
**Methodology:** Toyota 5 Whys, multi-causal investigation
**Status:** COMPLETE -- root causes identified and validated

---

## 1. Problem Statement

Procedural equirectangular textures applied to spheres in the nwave ray tracer present visible dark/light lines (seam artifacts). The user suspects the algorithm may be mapping only half of the sphere surface. The reference algorithm is from https://boytchev.github.io/texture-generator/.

**Affected components:**
- `src/domain/materials/procedural_texture.cpp` (CPU-side texture generation)
- `src/infrastructure/metal/shaders/ray_trace.metal` (GPU-side texture sampling)
- `src/infrastructure/yaml_scene_loader.cpp` (material pipeline configuration)

**Affected scene:** `scenes/nwave_bowling.yaml` (20 procedural texture spheres)

---

## 2. Investigation Scope

- Procedural texture generation pipeline (CPU)
- Equirectangular-to-sphere coordinate mapping (CPU generation + GPU sampling)
- Shader-side texture sampling code paths for procedural textures on spheres
- Comparison with reference algorithm (boytchev/texture-generator)

---

## 3. Evidence Gathered

### Evidence E1: CPU-side equirectangular mapping (procedural_texture.cpp, lines 518-528)

```cpp
for (int py = 0; py < height; py++) {
    for (int px = 0; px < width; px++) {
        float u = static_cast<float>(px) / static_cast<float>(width);
        float v = static_cast<float>(py) / static_cast<float>(height);

        // Equirectangular -> 3D point on unit sphere
        float theta = v * static_cast<float>(M_PI);
        float phi = u * 2.0f * static_cast<float>(M_PI);
        float sx = std::cos(phi) * std::sin(theta);
        float sy = std::cos(theta);
        float sz = std::sin(phi) * std::sin(theta);

        Color3f color = evaluate_pattern(pattern, sx, sy, sz, seed);
```

**Assessment:** This mapping is CORRECT. It matches the reference algorithm exactly:
- `theta = v * PI` (polar angle, 0 at north pole to PI at south pole)
- `phi = u * 2 * PI` (azimuthal angle, full 0 to 2*PI range)
- Cartesian conversion: `x = cos(phi)*sin(theta)`, `y = cos(theta)`, `z = sin(phi)*sin(theta)`

The user's hypothesis that "the algorithm maps only half the sphere" is NOT confirmed for the generation step. The full sphere is covered.

### Evidence E2: Reference algorithm (boytchev/texture-generator, generator.js)

```javascript
var u = x / width;
var v = y / height;
vector.setFromSphericalCoords( 1, Math.PI*v, 2*Math.PI*u );
pattern( vector.x, vector.y, vector.z, color, options, u, v, x, y );
```

Three.js `setFromSphericalCoords(radius, phi, theta)` convention (from Vector3.js source):
```javascript
// phi = polar angle from positive Y axis [0, PI]
// theta = azimuthal angle [0, 2*PI]
x = sin(phi) * sin(theta) * radius
y = cos(phi) * radius
z = sin(phi) * cos(theta) * radius
```

Called as: `setFromSphericalCoords(1, PI*v, 2*PI*u)`
- phi (polar) = PI * v
- theta (azimuthal) = 2 * PI * u

Resulting coordinates:
- `x = sin(PI*v) * sin(2*PI*u)`
- `y = cos(PI*v)`
- `z = sin(PI*v) * cos(2*PI*u)`

### Evidence E3: nwave coordinate mapping vs. reference -- MISMATCH DETECTED

nwave `procedural_texture.cpp`:
- `x = cos(phi) * sin(theta)` where phi = 2*PI*u
- `y = cos(theta)`
- `z = sin(phi) * sin(theta)` where phi = 2*PI*u

Reference (Three.js):
- `x = sin(phi) * sin(theta)` where phi = PI*v, theta = 2*PI*u
- `y = cos(phi)`
- `z = sin(phi) * cos(theta)`

Expanding with the actual angle values:

**nwave:** phi_nw = 2*PI*u (azimuthal), theta_nw = PI*v (polar)
- `sx = cos(2*PI*u) * sin(PI*v)`
- `sy = cos(PI*v)`
- `sz = sin(2*PI*u) * sin(PI*v)`

**Reference:** phi_ref = PI*v (polar), theta_ref = 2*PI*u (azimuthal)
- `x = sin(PI*v) * sin(2*PI*u)`
- `y = cos(PI*v)`
- `z = sin(PI*v) * cos(2*PI*u)`

Comparing term by term:
- `sy == y`: Both are `cos(PI*v)`. **MATCH.**
- `sx` vs `x`: nwave has `cos(2*PI*u) * sin(PI*v)`, reference has `sin(PI*v) * sin(2*PI*u)`. **DIFFERENT** -- nwave uses `cos(2*PI*u)` for x, reference uses `sin(2*PI*u)` for x.
- `sz` vs `z`: nwave has `sin(2*PI*u) * sin(PI*v)`, reference uses `sin(PI*v) * cos(2*PI*u)`. **DIFFERENT** -- nwave uses `sin(2*PI*u)` for z, reference uses `cos(2*PI*u)` for z.

This is a 90-degree azimuthal rotation (x and z are swapped with sin/cos roles reversed). This does NOT cause seam artifacts on its own because the procedural patterns use 3D noise which is rotationally invariant. The full sphere is still covered correctly.

**Key insight: The generation step is correct for both implementations. The coordinate difference is just a rotation, not a coverage issue.**

### Evidence E4: YAML loader forces cube_map mode for procedural textures

```cpp
// yaml_scene_loader.cpp, lines 96-99
material = ProceduralTexture::generate(pattern, width, height, seed);
// Use cube_map sampling for seam-free sphere mapping
auto* proc_img = dynamic_cast<ImageTexture*>(material.get());
if (proc_img) proc_img->set_texture_scale(-1.0f);
```

`texture_scale = -1.0f` triggers cube_map sampling in the shader.

### Evidence E5: Cube map sampling path in shader (ray_trace.metal, lines 953-957)

```metal
if (mat.texture_scale < 0.0f && rec.shape_type == SHAPE_SPHERE) {
    // Cube map mode: seam-free texture mapping for spheres
    albedo = sample_sphere_cube_map(texture_data, mat.texture_offset,
                                     mat.texture_width, mat.texture_height,
                                     rec.normal);
```

The cube map path uses `rec.normal` (the world-space normal flipped to face the ray).

### Evidence E6: Cube map sampling function (ray_trace.metal, lines 323-351)

```metal
float3 sample_sphere_cube_map(constant uchar* texture_data, int texture_offset,
                               float tex_width, float tex_height, float3 n) {
    float3 abs_n = abs(n);

    float face_u, face_v;
    if (abs_n.x >= abs_n.y && abs_n.x >= abs_n.z) {
        // X dominant
        float inv = 1.0f / abs_n.x;
        face_u = (n.x > 0) ? -n.z * inv : n.z * inv;
        face_v = -n.y * inv;
    } else if (abs_n.y >= abs_n.x && abs_n.y >= abs_n.z) {
        // Y dominant
        float inv = 1.0f / abs_n.y;
        face_u = n.x * inv;
        face_v = (n.y > 0) ? n.z * inv : -n.z * inv;
    } else {
        // Z dominant
        float inv = 1.0f / abs_n.z;
        face_u = (n.z > 0) ? n.x * inv : -n.x * inv;
        face_v = -n.y * inv;
    }

    // Map from [-1,1] to [0,1]
    float u = face_u * 0.5f + 0.5f;
    float v = face_v * 0.5f + 0.5f;

    return sample_texture_clamped(texture_data, texture_offset,
                                   tex_width, tex_height, u, v);
}
```

### Evidence E7: The cube map function maps to UV [0,1]x[0,1] from a SINGLE texture

The cube map function selects a dominant axis and computes a (u,v) in [0,1] from [-1,1] face coordinates. It then samples from the SAME equirectangular texture. This is the fundamental problem: a cube map face projection applied to an equirectangular texture means each face sees a different distorted rectangle of the texture.

An equirectangular texture is designed to be sampled with equirectangular UV coordinates (sphere_uv: theta/phi). When sampled with cube map face coordinates, the mapping is incorrect: the face UVs do not correspond to the correct texture locations for the underlying spherical pattern.

### Evidence E8: Texture dimensions for procedural textures

From `nwave_bowling.yaml`, all procedural textures use: `width: 1024, height: 512`

The aspect ratio is 2:1, which IS the standard equirectangular aspect ratio. The shader's equirectangular detection at line 952 checks:
```metal
bool is_equirectangular = (mat.texture_width > mat.texture_height * 1.8f);
```
1024 > 512 * 1.8 = 921.6 -- this is TRUE. So the `is_equirectangular` flag is true.

However, this check NEVER fires because `mat.texture_scale < 0.0f` is checked FIRST (line 953), and the cube map path takes precedence.

### Evidence E9: The correct sampling path is BYPASSED

The shader has this priority chain:
1. `texture_scale < 0.0f` => cube_map mode (forced by YAML loader for procedural textures)
2. `is_equirectangular` => standard sphere UV with wrapping (CORRECT path for equirectangular textures)
3. `texture_scale > 0.0f` => scaled decal mode
4. Default => dual-hemisphere blending

The YAML loader at line 99 forces `texture_scale = -1.0f`, so ALL procedural textures go through the cube map path (priority 1). The equirectangular path (priority 2) -- which is the CORRECT path for these textures -- is never reached.

### Evidence E10: sample_texture_clamped clamps UV, does not wrap

```metal
float3 sample_texture_clamped(constant uchar* texture_data, int texture_offset,
                              float tex_width, float tex_height, float u, float v) {
    int w = int(tex_width);
    int h = int(tex_height);
    u = clamp(u, 0.0f, 1.0f - 1.0f / float(w));
    v = clamp(v, 0.0f, 1.0f - 1.0f / float(h));
```

This clamps UV instead of wrapping. For equirectangular textures, horizontal wrapping at the U seam is essential for seamless rendering. Clamping causes the edge texels to be repeated, creating a visible line.

---

## 4. Toyota 5 Whys Analysis

### Branch A: Cube Map / Equirectangular Mismatch (PRIMARY ROOT CAUSE)

**WHY 1 (Symptom): Why are dark/light lines visible on procedural texture spheres?**

The cube map sampling function (`sample_sphere_cube_map`) projects the equirectangular texture using cube face coordinates that do not match the equirectangular UV layout. This produces visible discontinuities at the boundaries where the dominant axis switches (at 45-degree latitude/longitude lines from each face center).

Evidence: E6, E7. The cube map function computes face_u/face_v by dividing two normal components. These produce a rectilinear projection of the texture, but the texture data was generated for equirectangular projection. The content at face UV (0.3, 0.7) does NOT correspond to the same sphere point that was stored there during equirectangular generation.

**WHY 2 (Context): Why does the cube map sampling produce wrong coordinates for these textures?**

The cube map sampling function was designed for non-tileable rectangular image textures (photographs, non-equirectangular images). It maps the sphere into 6 cube faces, each getting a square sub-region of the texture. When applied to an equirectangular texture, the face UV coordinates do not match the spherical UV coordinates that the texture was generated with.

Evidence: E7. The cube map face UV is computed as:
```
face_u = face_x_component / dominant_component  // linear ratio in [-1,1]
```
But the equirectangular texture stores its data at:
```
u = phi / (2*PI)     // angular mapping
v = theta / PI        // angular mapping
```
These are fundamentally different parameterizations.

**WHY 3 (System): Why is the cube map path active for equirectangular textures?**

The YAML scene loader unconditionally forces `texture_scale = -1.0f` for ALL procedural textures (line 99 of yaml_scene_loader.cpp). This overrides the equirectangular detection logic in the shader. The code has a comment stating "Use cube_map sampling for seam-free sphere mapping," indicating this was an intentional but misguided choice.

Evidence: E4, E9. The loader code:
```cpp
material = ProceduralTexture::generate(pattern, width, height, seed);
// Use cube_map sampling for seam-free sphere mapping
auto* proc_img = dynamic_cast<ImageTexture*>(material.get());
if (proc_img) proc_img->set_texture_scale(-1.0f);
```

**WHY 4 (Design): Why was cube map mode chosen for procedural textures?**

The development history shows that previous research identified atan2 seam artifacts with standard spherical UV mapping for non-tileable textures (documented in `docs/research/seamless-sphere-texture-mapping.md` and `docs/research/sphere-texture-seam-production-solutions.md`). Cube map mode was implemented as a workaround for those seam artifacts. When procedural textures were added, they were grouped with "non-tileable" textures and given the same cube map treatment.

Evidence: E4 (comment reads "Use cube_map sampling for seam-free sphere mapping"), research documents describe the cube map as the recommended approach for non-tileable images.

**WHY 5 (Root Cause): What is the fundamental root cause?**

**ROOT CAUSE A: Procedural equirectangular textures are inherently seamless (they tile horizontally by construction via the continuous 3D noise function), but the pipeline treats them as non-tileable, routing them through the cube map sampling path instead of the correct equirectangular sampling path.**

The equirectangular sampling path (`sample_texture`) already handles horizontal U-wrapping correctly (line 202 of ray_trace.metal: `x0 = ((x0 % w) + w) % w`). Procedural equirectangular textures are specifically generated to be seamless at the u=0/u=1 boundary because the 3D noise function is continuous on the sphere. There is no seam to fix, but the code assumes there is.

---

### Branch B: Normal Space Mismatch in Cube Map Sampling

**WHY 1 (Symptom): Why do lines appear at specific orientations?**

The cube map function uses `rec.normal` (line 957), which is the world-space normal flipped to face the incoming ray. But the equirectangular texture was generated from the object-space sphere normal (the outward normal). For transformed spheres (rotated, scaled), `rec.normal` and the object-space outward normal point in different directions.

Evidence: E5 (uses `rec.normal`), compared to sphere UV computation at line 671 which uses `object_space_normal`.

**WHY 2 (Context): Why does the cube map path use rec.normal instead of object_space_normal?**

The `sample_sphere_cube_map` function was designed for image textures that should appear fixed in world space (like a projected environment). The sphere UV path correctly saves and uses `object_space_normal` (line 651), but the cube map path was not updated to use the same approach.

Evidence: Lines 670-671:
```metal
if (shape.shape_type == SHAPE_SPHERE) {
    sphere_uv(object_space_normal, rec.u, rec.v);
}
```
But line 955-957:
```metal
albedo = sample_sphere_cube_map(texture_data, mat.texture_offset,
                                 mat.texture_width, mat.texture_height,
                                 rec.normal);  // world-space, not object-space
```

**WHY 3 (System): Why is the normal handling inconsistent between paths?**

The sphere_uv and cube_map sampling were implemented at different times with different design goals. sphere_uv was implemented to support equirectangular maps (Earth, Moon) that rotate with the object. cube_map was implemented later for non-tileable textures where texture orientation was less critical.

**WHY 4 (Design): Why was the cube map function not given the object-space normal?**

The cube map function signature takes `float3 n` without documenting whether this should be object-space or world-space. The caller passes `rec.normal` by convention, which is the world-space normal.

**WHY 5 (Root Cause):**

**ROOT CAUSE B: The cube map sampling path uses rec.normal (world-space, facing the ray) instead of the object-space outward normal. For non-identity transforms, this produces a different texture lookup than what the equirectangular generation computed.**

This is a secondary contributor. For spheres without transforms, `rec.normal` and `object_space_normal` are equivalent (except for the front_face flip, which inverts the normal for back-faces). For transformed spheres, they diverge.

---

### Branch C: Cube Face Boundary Discontinuities

**WHY 1 (Symptom): Why are the lines at specific angular positions?**

The cube map function partitions the sphere into 6 regions based on the dominant component of the normal vector. At the boundaries between these regions (where two components have equal magnitude, i.e., 45-degree lines), the `face_u` and `face_v` calculations switch discontinuously.

Evidence: E6. The branching conditions:
```metal
if (abs_n.x >= abs_n.y && abs_n.x >= abs_n.z) {
    // X dominant
} else if (abs_n.y >= abs_n.x && abs_n.y >= abs_n.z) {
    // Y dominant
} else {
    // Z dominant
}
```

At the boundary where `abs_n.x == abs_n.z`, the face selection switches from X-dominant to Z-dominant. The face_u/face_v formulas are different for each face, and with an equirectangular texture as the source, the texture content at the same (u,v) is different depending on which face formula computed it.

**WHY 2:** The cube map was designed for 6 separate face textures, not a single equirectangular texture. With a single texture, the face selection creates artificial boundaries.

**WHY 3:** Using a single equirectangular texture with cube map sampling is architecturally incoherent -- the two parameterizations are incompatible.

**WHY 4:** The mismatch was introduced when cube map sampling was repurposed from its intended use case (6-face textures or non-equirectangular images) to equirectangular textures.

**WHY 5 (Root Cause):**

**ROOT CAUSE C: The cube map function introduces 12 face-edge discontinuities when sampling from a single equirectangular texture, because the face UV parameterization does not match the equirectangular parameterization. These discontinuities manifest as the visible dark/light lines.**

---

### Branch D: sample_texture_clamped Prevents U-Wrapping

**WHY 1 (Symptom): Why is there an abrupt color change at certain edges?**

The cube map path calls `sample_texture_clamped`, which clamps UV to [0, 1-1/w] instead of wrapping. For equirectangular textures, the U coordinate should wrap (u=1 connects to u=0 for seamless horizontal tiling).

Evidence: E10. The clamped sampler:
```metal
u = clamp(u, 0.0f, 1.0f - 1.0f / float(w));
```

Compare with the wrapping sampler (`sample_texture`, line 202):
```metal
x0 = ((x0 % w) + w) % w;
x1 = ((x1 % w) + w) % w;
```

**WHY 2:** sample_texture_clamped was designed for triplanar mapping and decals, where wrapping is incorrect. It was reused for cube map face sampling without considering that the underlying texture might need wrapping.

**WHY 3-5:** Same chain as Branch A -- the wrong sampling function is used because the wrong sampling path is active.

**ROOT CAUSE D: Using sample_texture_clamped for equirectangular content prevents horizontal U-wrapping, producing edge artifacts at the boundaries of the cube face UV range.**

This is a contributing factor that amplifies the discontinuities from Root Cause C.

---

## 5. Root Cause Summary

| ID | Root Cause | Severity | Confidence |
|----|-----------|----------|------------|
| A | Procedural equirectangular textures are routed through cube map sampling instead of equirectangular sampling, despite being inherently seamless | **Critical** | High -- directly evidenced in code |
| B | Cube map path uses world-space normal instead of object-space normal | Medium | High -- evidenced by code comparison |
| C | Cube map face-edge discontinuities when applied to equirectangular data | **Critical** | High -- mathematical certainty |
| D | sample_texture_clamped prevents horizontal wrapping needed by equirectangular textures | Medium | High -- directly evidenced in code |

Root causes A and C are the primary causes. B and D are contributing factors that worsen the artifacts.

---

## 6. Backward Chain Validation

### Validation of Root Cause A:

Forward trace: YAML loader sets `texture_scale = -1.0f` -> shader checks `texture_scale < 0` -> cube map path activated -> `sample_sphere_cube_map` called with equirectangular texture -> face UV != equirectangular UV -> visible discontinuities at face boundaries.

**If we remove `texture_scale = -1.0f`**, the shader would reach the equirectangular check (`is_equirectangular`). Since width=1024 > height*1.8=921.6, it would evaluate TRUE, and the shader would call `sample_texture(...)` with `rec.u, rec.v` computed by `sphere_uv()`. This path handles U-wrapping correctly, and the texture was generated for exactly this parameterization. **The seams would disappear.**

**Validation: CONFIRMED.** The forward trace produces the observed symptom, and removing the root cause eliminates it.

### Validation of Root Cause C:

Forward trace: cube map function partitions sphere into 6 regions -> at each boundary, face_u/face_v change discontinuously -> same texture sampled at different (u,v) on either side of boundary -> color discontinuity visible.

**Verification:** Consider the boundary where `abs_n.x == abs_n.z` and both are larger than `abs_n.y`. On the X-dominant side: `face_u = -n.z/abs_n.x`, `face_v = -n.y/abs_n.x`. On the Z-dominant side: `face_u = n.x/abs_n.z`, `face_v = -n.y/abs_n.z`. At the exact boundary (abs_n.x == abs_n.z), X-dominant gives face_u = -n.z/abs_n.x and Z-dominant gives face_u = n.x/abs_n.z. Since abs_n.x == abs_n.z, these simplify to -n.z/abs_n.x vs n.x/abs_n.x. Unless n.x == -n.z (which only happens at specific angles), these are different values. **Different face_u means different texture sample, hence visible seam.**

**Validation: CONFIRMED.** Mathematical analysis confirms discontinuity.

### Cross-validation:

Root causes A and C are consistent and do not contradict each other. A causes C: routing through cube map (A) creates the face boundary discontinuities (C). Fixing A (use equirectangular path) eliminates C (no cube faces).

Root cause B adds additional distortion for transformed spheres. Root cause D amplifies edge artifacts. All four root causes are consistent with the symptom of visible dark/light lines.

---

## 7. Proposed Fixes

### Fix 1 (IMMEDIATE -- addresses Root Causes A, C, D): Remove cube_map override for procedural textures

**File:** `src/infrastructure/yaml_scene_loader.cpp`

**Before (lines 96-99):**
```cpp
material = ProceduralTexture::generate(pattern, width, height, seed);
// Use cube_map sampling for seam-free sphere mapping
auto* proc_img = dynamic_cast<ImageTexture*>(material.get());
if (proc_img) proc_img->set_texture_scale(-1.0f);
```

**After:**
```cpp
material = ProceduralTexture::generate(pattern, width, height, seed);
// Procedural equirectangular textures are inherently seamless at the
// u=0/u=1 boundary (3D noise is continuous on the sphere).
// Use default texture_scale (0.0f) to let the shader detect the 2:1
// aspect ratio and use the equirectangular sampling path with U-wrapping.
```

**Effect:** With `texture_scale` at its default 0.0f, the shader will:
1. Skip the cube map path (texture_scale is not < 0)
2. Detect `is_equirectangular` as TRUE (1024 > 512 * 1.8)
3. Use `sample_texture(...)` with `rec.u, rec.v` from `sphere_uv()`
4. This path wraps U correctly and matches the equirectangular generation

**Risk:** Low. The only change is removing two lines of code and a comment. The equirectangular sampling path already exists and is tested with Earth/Moon textures.

### Fix 2 (ADDITIONAL -- addresses Root Cause B): Use object_space_normal for cube map if retained

If cube map sampling is retained for any future use case, ensure it uses the object-space normal:

**File:** `src/infrastructure/metal/shaders/ray_trace.metal`

**Before (line 955-957):**
```metal
albedo = sample_sphere_cube_map(texture_data, mat.texture_offset,
                                 mat.texture_width, mat.texture_height,
                                 rec.normal);
```

**After:**
```metal
// Use object-space normal so texture rotates with the sphere
float3 cube_normal = object_space_normal;
albedo = sample_sphere_cube_map(texture_data, mat.texture_offset,
                                 mat.texture_width, mat.texture_height,
                                 cube_normal);
```

Note: `object_space_normal` is already computed and available in the scope (line 651/745). This fix applies to both the brute-force and BVH intersection paths.

**Risk:** Low. Only affects the cube_map path. Does not change behavior for equirectangular textures (which is Fixed by Fix 1).

### Fix 3 (PREVENTIVE -- addresses systemic issue): Add texture_mapping YAML field for procedural textures

Allow the scene author to explicitly choose the sampling mode rather than having the loader hardcode it:

**File:** `src/infrastructure/yaml_scene_loader.cpp`

**After:**
```cpp
} else if (type == "procedural_texture") {
    std::string pattern_name = mat_node["pattern"].as<std::string>();
    int width = mat_node["width"] ? mat_node["width"].as<int>() : 1024;
    int height = mat_node["height"] ? mat_node["height"].as<int>() : 512;
    uint32_t seed = mat_node["seed"] ? mat_node["seed"].as<uint32_t>() : 42;
    auto pattern = pattern_from_string(pattern_name);
    material = ProceduralTexture::generate(pattern, width, height, seed);
    // Allow explicit texture_mapping override; default is equirectangular (no override)
    if (mat_node["texture_mapping"]) {
        std::string mapping = mat_node["texture_mapping"].as<std::string>();
        auto* proc_img = dynamic_cast<ImageTexture*>(material.get());
        if (mapping == "cube_map" && proc_img) {
            proc_img->set_texture_scale(-1.0f);
        }
        // "equirectangular" or absent = default (texture_scale 0.0)
    }
}
```

**Risk:** Low. Adds flexibility without changing default behavior.

---

## 8. Verification of Fix Against Reference Algorithm

The reference algorithm (boytchev/texture-generator) generates textures using:
```javascript
vector.setFromSphericalCoords(1, Math.PI*v, 2*Math.PI*u);
```

And displays them using Three.js `EquirectangularReflectionMapping`, which maps the texture back to a sphere using equirectangular UV coordinates (equivalent to the `sphere_uv()` function in the nwave shader).

After Fix 1, the nwave pipeline will:
1. **Generate** the texture using equirectangular-to-3D mapping (procedural_texture.cpp, already correct)
2. **Sample** the texture using equirectangular UV from `sphere_uv()` -> `sample_texture()` with U-wrapping

This matches the reference algorithm's generate-then-sample pipeline exactly. The 90-degree azimuthal rotation between the two implementations (Evidence E3) is cosmetic -- it rotates the pattern on the sphere but does not affect seamlessness.

---

## 9. Prevention Strategy

### Immediate mitigations (restore service):
1. Apply Fix 1 (remove cube_map override for procedural textures). This is a 2-line deletion.
2. Re-render the bowling scene to verify seam elimination.

### Permanent fixes (prevent recurrence):
1. Apply Fix 3 (explicit texture_mapping field) to prevent hardcoded overrides.
2. Add a comment in `procedural_texture.cpp` documenting that the output is equirectangular and should be sampled with equirectangular UV.
3. Add a unit test that verifies procedural textures have `texture_scale == 0.0f` (not -1.0f) by default.

### Early detection:
1. Add a debug rendering mode that visualizes texture UV coordinates on spheres (false-color u as red, v as green) to quickly identify parameterization mismatches.
2. Add a debug assertion in the shader that warns when `is_equirectangular` is true but `texture_scale < 0` (conflicting directives).

---

## 10. Appendix: Code Cross-Reference

| File | Lines | Role in Bug |
|------|-------|------------|
| `src/infrastructure/yaml_scene_loader.cpp` | 96-99 | Forces cube_map mode on procedural textures |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 162-167 | `sphere_uv()` -- correct equirectangular UV computation |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 180-218 | `sample_texture()` -- correct equirectangular sampler with U-wrapping |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 222-247 | `sample_texture_clamped()` -- clamps UV, no wrapping |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 323-351 | `sample_sphere_cube_map()` -- cube face projection, wrong for equirectangular data |
| `src/infrastructure/metal/shaders/ray_trace.metal` | 951-986 | Material texture dispatch logic (priority chain) |
| `src/domain/materials/procedural_texture.cpp` | 518-528 | Equirectangular generation (correct) |
| `scenes/nwave_bowling.yaml` | 74-93 | Procedural texture material definitions |

---

## 11. Appendix: The "Half Sphere" Hypothesis

The user suspected the algorithm maps only half the sphere. Investigation found:

1. The **generation** step covers the full sphere (u in [0,1] maps phi to [0, 2*PI], v in [0,1] maps theta to [0, PI]). This is NOT the issue.

2. However, the **cube map sampling** effectively uses only a portion of the texture for each face. Each cube face maps a distorted rectangle from the equirectangular texture. The face at the "back" of the cube (Z-negative) maps u=0.5 as its center, but the face at the "front" (Z-positive) maps u=0 and u=1 (the seam region) as its center. The overlapping and non-overlapping regions create the illusion that parts of the sphere show the wrong texture content or miss content entirely.

So while the user's diagnosis was not precisely correct (it is not "half the sphere"), the intuition was directionally right: the cube map sampling does not use the equirectangular texture correctly, causing large regions of the sphere to show the wrong texture content, which can look like missing or doubled coverage.
