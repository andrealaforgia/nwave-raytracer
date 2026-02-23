# Seamless Sphere Texture Mapping for nwave-raytracer

## Research Summary

| Property | Value |
|---|---|
| Topic | Algorithms and techniques for seam-free texture mapping on spheres in GPU ray tracers |
| Date | 2026-02-21 |
| Sources consulted | 22 |
| Sources cited below | 18 |
| Overall confidence | High |

---

## Table of Contents

1. [Problem Analysis: Why Sphere Texturing Produces Seams](#1-problem-analysis)
2. [Approach 1: Cube Mapping (Recommended)](#2-cube-mapping)
3. [Approach 2: Dual-Hemisphere Blending](#3-dual-hemisphere-blending)
4. [Approach 3: Octahedral Mapping](#4-octahedral-mapping)
5. [Approach 4: 3D Procedural Textures (Seam-Free by Nature)](#5-procedural-textures)
6. [Approach 5: Texture Bombing / Stochastic Texturing](#6-texture-bombing)
7. [Approach 6: Equirectangular with Seam Repair](#7-equirectangular-repair)
8. [Approach 7: Decal / Partial Coverage Projection](#8-decal-projection)
9. [How Production Renderers Handle This](#9-production-renderers)
10. [Filtering and Blending Techniques at Seam Boundaries](#10-filtering-blending)
11. [Non-Tileable Texture Strategies](#11-non-tileable)
12. [Recommendation for nwave-raytracer](#12-recommendation)
13. [Source Analysis](#13-source-analysis)
14. [Knowledge Gaps](#14-knowledge-gaps)

---

## 1. Problem Analysis: Why Sphere Texturing Produces Seams {#1-problem-analysis}

**Confidence: High (6 sources)**

The fundamental mathematical problem is that no homeomorphism exists between a rectangular 2D domain and the surface of a sphere without introducing at least one singularity or discontinuity. This is a topological constraint -- the Hairy Ball Theorem and the impossibility of covering S2 with a single chart guarantee that any 2D parameterization of a sphere must have at least one seam or pole singularity [Catlike-CubeSphere, Khronos-Sphere, Wikipedia-UV].

### 1.1 The Meridian Seam (atan2 Discontinuity)

The standard spherical UV mapping computes:
```
phi = atan2(-normal.z, normal.x) + PI    // azimuthal angle [0, 2*PI]
theta = acos(-normal.y)                   // polar angle [0, PI]
u = phi / (2 * PI)                        // [0, 1]
v = 1 - theta / PI                        // [0, 1]
```

The `atan2` function has a discontinuity where the angle wraps from approximately 2*PI back to 0. At this meridian, two adjacent surface points have `u` values near 0.0 and near 1.0 respectively. When bilinear filtering samples between these texels, it interpolates across the entire width of the texture, producing a visible "smear" seam line [PBRT-Spheres, Golus-SphereQuad, Khronos-Sphere].

**Current nwave-raytracer behavior**: The shader's `sphere_uv()` function (line 158-163 of `ray_trace.metal`) produces exactly this discontinuity. The `sample_texture()` function wraps U coordinates via modular arithmetic (line 198-199), which correctly handles the horizontal seam for tileable textures. However, for non-tileable textures, the left and right edges do not match, creating a visible seam.

### 1.2 Pole Singularity

At the poles (v=0 and v=1), all u values map to the same point. A horizontal row of texels compresses into a single point, causing visible distortion -- texels are stretched to near-infinite width. Equirectangular images have significant data redundancy near the poles for this reason [Wikipedia-CubeMap, PBRT-Spheres].

### 1.3 The Non-Tileable Texture Problem

When the texture is specifically designed to tile (left edge matches right edge), the U-wrap seam becomes invisible. This is why equirectangular Earth maps work well -- they are authored to tile horizontally. For non-tileable textures (eye textures, decorative patterns, granite photos), the left and right edges are completely different images, making the wrap seam highly visible [TechMonkey-Spherical].

### 1.4 Why Previous Attempts Failed

Analysis of the current shader's attempted solutions:

| Attempt | Why it fails |
|---|---|
| `sphere_uv` (standard) | atan2 wrap creates hard seam at the meridian for non-tileable textures |
| Triplanar mapping | Projections from 3 axes create blending boundaries at 45-degree transitions; works for planar surfaces but produces visible projection-switch artifacts on spheres |
| `mirror_repeat` | Mirrors the texture at boundaries, but creates visible mirror-lines where the texture reverses direction; the mirror itself becomes the seam |
| `sphere_uv` + `mirror_repeat` combination | Moves the seam from the meridian to two mirror points; reduces artifact severity but does not eliminate it |

The core issue: all these methods attempt to map a single rectangular image onto a topologically incompatible surface. The solutions that actually work either (a) change the domain topology (cube map, multi-chart), (b) avoid UV mapping entirely (3D procedural), or (c) limit texture coverage to avoid the seam region (decal/partial coverage).

---

## 2. Approach 1: Cube Mapping (Recommended for Non-Tileable Textures) {#2-cube-mapping}

**Confidence: High (6 sources)**

Cube mapping replaces the single rectangular texture with six square face images, one per face of a cube circumscribing the sphere. The surface normal (or direction from center) selects which face to sample and the 2D coordinates within that face.

### 2.1 Algorithm

Given a sphere hit with outward normal `n = (nx, ny, nz)`:

1. **Face selection**: Find the component with the largest absolute value:
```metal
float3 a = abs(n);
int face;
float2 uv;
if (a.x >= a.y && a.x >= a.z) {
    face = (n.x > 0) ? 0 : 1;  // +X or -X
    uv = float2(-sign(n.x) * n.z, -n.y) / a.x;
} else if (a.y >= a.x && a.y >= a.z) {
    face = (n.y > 0) ? 2 : 3;  // +Y or -Y
    uv = float2(n.x, sign(n.y) * n.z) / a.y;
} else {
    face = (n.z > 0) ? 4 : 5;  // +Z or -Z
    uv = float2(sign(n.z) * n.x, -n.y) / a.z;
}
// Map from [-1,1] to [0,1]
uv = uv * 0.5 + 0.5;
```

2. **Texture lookup**: Sample from the appropriate face texture using the computed UV.

3. **Seam filtering**: At face edges, bilinear filtering must sample from the adjacent face. For ray tracers that sample raw byte buffers (like nwave-raytracer), this requires custom cross-face filtering.

Sources: [Wikipedia-CubeMap, LearnOpenGL-Cubemaps, Castano-SeamlessCube]

### 2.2 Cross-Face Seam Filtering

The seam at cube face boundaries occurs because bilinear filtering at the edge of one face does not know about the texels on the adjacent face. Three solutions exist:

**A. Gutter/Padding approach (Recommended for nwave-raytracer)**:
Add a 1-2 texel border around each face containing copies of the adjacent face's edge texels. This allows standard bilinear filtering to cross the face boundary naturally without any shader changes [Castano-SeamlessCube, Polycount-EdgePadding].

**B. Hardware seamless cube map filtering**:
Modern GPUs with `ARB_seamless_cube_map` (OpenGL) or D3D10+ handle cross-face filtering automatically. Metal's `texturecube` type provides this natively. However, nwave-raytracer samples from raw byte buffers, not MTLTexture objects, so this hardware feature is unavailable [Castano-SeamlessCube].

**C. Coordinate warping**:
Scale non-dominant coordinates slightly inward to prevent sampling at the exact face edge:
```metal
float3 fix_cube_lookup(float3 v, float cube_size) {
    float M = max(max(abs(v.x), abs(v.y)), abs(v.z));
    float scale = (cube_size - 1.0) / cube_size;
    if (abs(v.x) != M) v.x *= scale;
    if (abs(v.y) != M) v.y *= scale;
    if (abs(v.z) != M) v.z *= scale;
    return v;
}
```
Source: [Castano-SeamlessCube]

### 2.3 Applying a Single Rectangular Texture via Cube Mapping

For the case where you have a single rectangular texture (not six pre-made cube faces), there are two approaches:

**A. Pre-convert to cube map faces (offline)**:
Split the rectangular texture into 6 face images during scene loading. For a non-tileable texture like an eye, the texture content is designed for one face (e.g., the +Z face), and the other five faces get a solid fill color or are derived from the edges.

**B. Use the single largest face with fallback**:
Map the texture to one cube face only (e.g., front-facing +Z). Other faces display a solid color or stretched edge pixels. This is essentially the "decal" approach (Section 8) implemented via cube map topology.

### 2.4 Pros and Cons

| Aspect | Assessment |
|---|---|
| Seam quality | Excellent -- no meridian seam, no pole singularity. Face-edge seams solvable with gutter pixels |
| Distortion | Low -- maximum 2:1 area distortion at face corners (vs. infinite at poles for spherical) |
| Non-tileable textures | Good -- each face is independently addressable |
| Implementation complexity | Medium -- requires face selection logic and 6-face texture storage |
| Memory cost | 6 square faces vs. 1 rectangular; approximately 50% more total pixels at equivalent quality |
| GPU cost | One branch for face selection, then standard 2D lookup; negligible |

---

## 3. Approach 2: Dual-Hemisphere Blending {#3-dual-hemisphere-blending}

**Confidence: Medium (3 sources)**

This technique maps the texture twice from two opposing directions, then blends between them in an overlap zone. It eliminates the meridian seam by ensuring that at every point on the sphere, at least one of the two projections is far from its own seam.

### 3.1 Algorithm

1. Compute two independent UV sets from opposing projections. For example, the standard `sphere_uv` gives one set, and a rotated version (rotated 90 degrees around the Y axis) gives another:
```metal
// Projection A: standard sphere UV
float phi_a = atan2(-n.z, n.x) + M_PI_F;
float u_a = phi_a / (2.0 * M_PI_F);

// Projection B: rotated 90 degrees
float phi_b = atan2(-n.x, -n.z) + M_PI_F;
float u_b = phi_b / (2.0 * M_PI_F);

// Both share the same V (theta-based)
float v = 1.0 - acos(-n.y) / M_PI_F;

// Blend factor: use distance from each projection's seam
// Projection A's seam is at u_a ~ 0 or 1
// Projection B's seam is at u_b ~ 0 or 1
float blend_a = min(u_a, 1.0 - u_a);  // 0 at seam, 0.5 at center
float blend_b = min(u_b, 1.0 - u_b);  // 0 at seam, 0.5 at center

// Normalized blend weight
float weight_b = blend_b / (blend_a + blend_b + 1e-6);

float3 color_a = sample_texture(..., u_a, v);
float3 color_b = sample_texture(..., u_b, v);
float3 final = mix(color_a, color_b, weight_b);
```

2. Near projection A's seam, `blend_a` approaches 0, so `weight_b` approaches 1, using projection B (which is far from its seam). The converse holds near projection B's seam.

Sources: [Golus-SphereQuad, UE-SeamlessSphere, GameDev-SphereTex]

### 3.2 Pros and Cons

| Aspect | Assessment |
|---|---|
| Seam quality | Good -- eliminates the meridian seam; pole distortion remains |
| Distortion | Moderate -- equirectangular distortion present in both projections |
| Non-tileable textures | Moderate -- the blending zone shows a mix of two differently-projected views of the same texture, which may look soft/doubled |
| Implementation complexity | Low -- only requires computing a second UV set and blending |
| Memory cost | None -- same texture used twice |
| GPU cost | 2x texture samples + blend computation; moderate |

### 3.3 Limitation

The blending zone (roughly 25% of the sphere on each side) shows a ghosted/soft transition between two projections. This is acceptable for organic/natural textures (granite, marble) but may produce double-vision artifacts for textures with sharp features (eyes, text, logos).

---

## 4. Approach 3: Octahedral Mapping {#4-octahedral-mapping}

**Confidence: Medium (3 sources)**

Octahedral mapping projects the sphere onto an octahedron, unfolds it into a single square 2D texture. This provides a single-chart, near-uniform parameterization with better area preservation than equirectangular mapping.

### 4.1 Algorithm

Given unit normal `n = (nx, ny, nz)`:
```metal
// Project onto octahedron
float2 oct = float2(n.x, n.z) / (abs(n.x) + abs(n.y) + abs(n.z));

// Unfold bottom hemisphere
if (n.y < 0.0) {
    float2 sign_oct = float2(oct.x >= 0.0 ? 1.0 : -1.0,
                              oct.y >= 0.0 ? 1.0 : -1.0);
    oct = (1.0 - abs(oct.yx)) * sign_oct;
}

// Map from [-1,1] to [0,1]
float2 uv = oct * 0.5 + 0.5;
```

For filtering across the fold line (bottom hemisphere unfolding), the mirrored-repeat wrapping mode can be used [GPUOpen-OctCube].

Source: [GPUOpen-OctCube, Engelhardt-OctEnvMap, Knarkowicz-OctNormal]

### 4.2 Pros and Cons

| Aspect | Assessment |
|---|---|
| Seam quality | Good -- single chart with diagonal seams at hemisphere fold; filterable with mirror-repeat |
| Distortion | Low-moderate -- better area distribution than equirectangular; maximum ~2x distortion at octahedron edges |
| Non-tileable textures | Poor -- the texture must be specifically authored for octahedral layout, which is unusual for stock textures |
| Implementation complexity | Low -- simple math, single texture lookup |
| Memory cost | Single square texture; efficient packing |
| GPU cost | 19 VALU ops for coordinate computation vs. 10 for cubemap [GPUOpen-OctCube] |

### 4.3 Applicability

Octahedral mapping excels for environment maps, normal maps, and procedurally generated textures where the artist controls the texture layout. It is poorly suited for applying an existing rectangular photo texture to a sphere because the texture would need to be reprojected into octahedral layout first.

---

## 5. Approach 4: 3D Procedural Textures (Seam-Free by Nature) {#5-procedural-textures}

**Confidence: High (5 sources)**

Procedural textures that evaluate a mathematical function at the 3D world-space (or object-space) hit point bypass UV mapping entirely. Since there is no 2D parameterization, there are no seams and no pole singularities.

### 5.1 Available Techniques

**A. Perlin noise / fBm (Fractional Brownian Motion)**:
The foundation for marble, granite, cloud, and other organic textures. Evaluates gradient noise at the 3D hit point with multiple octaves for multi-scale detail.
```metal
float3 granite_color(float3 p) {
    float n = turbulence(p * 4.0, 7);
    return mix(float3(0.2, 0.2, 0.22),   // dark speckle
               float3(0.85, 0.82, 0.78),  // light granite
               n);
}
```
Sources: [PBRT-Noise, Shirley-RTTNW, GPUGems-PerlinNoise]

**B. Voronoi/Worley noise (Cellular noise)**:
Computes distance to nearest random feature points in 3D space. Produces cellular, cracked, cobblestone, and organic cell patterns. Inherently 3D, no UV mapping required.
```metal
float worley_noise(float3 p) {
    float3 cell = floor(p);
    float min_dist = 1e10;
    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    for (int z = -1; z <= 1; z++) {
        float3 neighbor = cell + float3(x, y, z);
        float3 point = neighbor + hash33(neighbor);  // random point in cell
        float dist = length(p - point);
        min_dist = min(min_dist, dist);
    }
    return min_dist;
}
```
Sources: [BookOfShaders-Cellular, BrianSharpe-GPUCellular, Wikipedia-Worley]

**C. Domain warping**:
Feed noise output back as input coordinate displacement to create complex organic patterns:
```metal
float3 warped_texture(float3 p) {
    float3 q = float3(fbm(p), fbm(p + 5.2), fbm(p + 1.3));
    return fbm(p + 4.0 * q);
}
```
Source: [IQ-DomainWarp]

**D. Simplex noise**:
An improvement over Perlin noise with fewer directional artifacts and lower computational cost. Particularly well-suited for GPU implementation.

### 5.2 Pros and Cons

| Aspect | Assessment |
|---|---|
| Seam quality | Perfect -- no seams possible; function is continuous everywhere in 3D |
| Distortion | None -- evaluation is at the 3D hit point |
| Non-tileable textures | Not applicable -- textures are generated, not sampled from images |
| Implementation complexity | Medium -- requires implementing noise functions in the shader |
| Memory cost | Minimal -- only permutation tables (512 bytes) |
| GPU cost | Moderate -- 7 octaves of noise requires ~56 trilinear interpolations; can be expensive for complex patterns |

### 5.3 Limitation

Procedural textures cannot reproduce arbitrary photographic content. An eye texture, a specific logo, or a photographed granite slab cannot be recreated procedurally. They are ideal for natural materials (marble, wood, clouds, lava) but not for image-based textures.

---

## 6. Approach 5: Texture Bombing / Stochastic Texturing {#6-texture-bombing}

**Confidence: Medium (4 sources)**

Texture bombing places randomized copies of a source texture at irregular positions across the surface, blending overlapping regions. Histogram-preserving blending ensures the statistical appearance is maintained without ghosting artifacts.

### 6.1 Texture Bombing Algorithm

1. Divide 3D object space into a grid of cells
2. Each cell receives a randomly offset and rotated copy of the source texture
3. Where cells overlap, blend using distance-weighted averaging
4. For sphere surfaces, use the 3D hit point as the coordinate

The technique requires sampling from 4 cells (2D) or 8 cells (3D) per pixel, at approximately "eight texture samples and 44 math instructions" on GPU [GPUGems-Bombing].

### 6.2 Histogram-Preserving Blending

Standard alpha blending of texture copies produces ghosting (reduced contrast, new colors). Histogram-preserving blending transforms the texture into a Gaussian distribution before blending, then inverts the transform after:

1. Precompute a histogram-matching LUT: texture values -> Gaussian values
2. At render time, transform each sample to Gaussian space
3. Blend in Gaussian space (linear blending preserves Gaussians)
4. Inverse-transform back to texture space

This preserves the original texture's color distribution through the blending process [INRIA-HPBlend, UnityGrenoble-HPBlend].

### 6.3 Pros and Cons

| Aspect | Assessment |
|---|---|
| Seam quality | Good -- no seams; the texture is scattered randomly |
| Distortion | Low -- depends on projection used for cell coordinate system |
| Non-tileable textures | Excellent -- specifically designed for making non-tileable textures appear seamless |
| Implementation complexity | High -- requires histogram precomputation, multiple texture fetches, and blending logic |
| Memory cost | Low -- single source texture plus small LUTs |
| GPU cost | High -- 8+ texture samples per pixel with blending math |

### 6.4 Applicability

Best suited for stochastic textures (granite, sand, bark, moss) where the pattern has no large-scale structure that would be disrupted by random placement. Not suitable for textures with recognizable features (eyes, text) as the random repetition would be visible.

---

## 7. Approach 6: Equirectangular with Seam Repair {#7-equirectangular-repair}

**Confidence: Medium (3 sources)**

Rather than changing the mapping algorithm, this approach modifies the texture itself to tile seamlessly at the meridian.

### 7.1 Texture Preprocessing

The texture can be made horizontally tileable through:

**A. Edge padding with cross-fade**:
1. Copy a strip of pixels from the left edge
2. Alpha-blend this strip onto the right edge (and vice versa)
3. The blended zone width determines the transition smoothness

**B. Polar Coordinates effect**:
Use image editing to convert the texture to polar coordinates, edit the seam in the center of the image where it becomes accessible, then convert back [TechMonkey-Spherical].

**C. Content-aware fill**:
Modern image editors can fill the seam zone with content-aware algorithms that match both edges.

### 7.2 Runtime Seam Softening

If texture preprocessing is not possible, a runtime approach blends the seam zone in the shader:
```metal
float3 sample_with_seam_blend(constant uchar* tex_data, int offset,
                               float w, float h, float u, float v,
                               float blend_width) {
    float3 main_color = sample_texture(tex_data, offset, w, h, u, v);

    // Distance from the seam (u=0 or u=1)
    float dist_from_seam = min(u, 1.0 - u);

    if (dist_from_seam < blend_width) {
        // Sample from the opposite side of the texture
        float mirror_u = 1.0 - u;
        float3 opposite_color = sample_texture(tex_data, offset, w, h, mirror_u, v);

        float t = dist_from_seam / blend_width;
        return mix(opposite_color, main_color, t);
    }
    return main_color;
}
```

### 7.3 Pros and Cons

| Aspect | Assessment |
|---|---|
| Seam quality | Moderate -- seam becomes soft/blurred rather than hard, but the blend zone is visible for patterned textures |
| Distortion | Same as standard spherical (pole singularity remains) |
| Non-tileable textures | Moderate -- forces tileability through blending, which may produce visible softening |
| Implementation complexity | Low (runtime) to Medium (preprocessing) |
| Memory cost | None (runtime) |
| GPU cost | Negligible -- 1 extra sample in the blend zone only |

---

## 8. Approach 7: Decal / Partial Coverage Projection {#8-decal-projection}

**Confidence: Medium (3 sources)**

For textures like eyes that need to appear on only part of the sphere, the simplest seam elimination is to avoid wrapping entirely. Map the texture to a limited angular region of the sphere, with a solid background color (or procedural texture) covering the rest.

### 8.1 Current Implementation in nwave-raytracer

The shader already implements this approach for scaled textures (lines 865-876 of `ray_trace.metal`):
```metal
if (rec.shape_type == SHAPE_SPHERE && mat.texture_scale > 0.0f) {
    float scale = mat.texture_scale;
    float su = (rec.u - 0.5f) / scale + 0.5f;
    float sv = (rec.v - 0.5f) / scale + 0.5f;
    if (su >= 0.0f && su <= 1.0f && sv >= 0.0f && sv <= 1.0f) {
        albedo = sample_texture_clamped(..., su, sv);
    } else {
        albedo = float3(0.95f, 0.95f, 0.95f); // white sclera
    }
}
```

### 8.2 Enhanced Decal with Edge Feathering

Add smooth falloff at decal edges to blend into the background:
```metal
float decal_alpha = 1.0;
float edge_dist = min(min(su, 1.0 - su), min(sv, 1.0 - sv));
float feather_width = 0.05;  // 5% of texture size
if (edge_dist < feather_width) {
    decal_alpha = edge_dist / feather_width;
}
albedo = mix(background_color, texture_color, decal_alpha);
```

### 8.3 Pros and Cons

| Aspect | Assessment |
|---|---|
| Seam quality | Perfect within coverage area -- no wrapping means no seam |
| Distortion | Low at center, increasing toward coverage edges (equirectangular distortion) |
| Non-tileable textures | Excellent -- the texture is used exactly once, as-is |
| Implementation complexity | Already implemented in the shader |
| Memory cost | None |
| GPU cost | Negligible |
| Coverage | Partial sphere only -- not a solution for full-sphere coverage |

---

## 9. How Production Renderers Handle This {#9-production-renderers}

**Confidence: High (4 sources)**

### 9.1 PBRT (Physically Based Rendering)

PBRT handles the sphere UV seam through careful numerical treatment:

1. **atan2 wrap correction**: `if (phi < 0) phi += 2 * Pi` maps the angle to [0, 2*PI] [PBRT-Spheres]
2. **Pole perturbation**: When both x and y are zero (at a pole), PBRT applies `pHit.x = 1e-5f * radius` to avoid undefined atan2 output [PBRT-Spheres]
3. **Partial sphere support**: The parameterization supports restricted angular ranges via `phiMax`, `thetaMin`, `thetaMax`, allowing texture mapping on partial spheres that avoid the seam entirely [PBRT-Spheres]
4. **Solid textures**: PBRT heavily relies on procedural 3D textures (noise-based) for natural materials, which avoids UV-related seams completely [PBRT-Noise]

PBRT does NOT solve the fundamental meridian seam for non-tileable textures on full spheres. It assumes textures designed for spherical mapping are authored to tile horizontally.

### 9.2 Blender Cycles

Blender Cycles provides multiple sphere mapping modes:

1. **Sphere Projection**: Uses `atan2(z, x)` for U and `asin(y)` for V, producing the standard equirectangular mapping with the inherent meridian seam [Blender-MappingTypes]
2. **Generated Coordinates**: Maps texture using the object's bounding box, similar to a boxy projection [Blender-Cycles-Sphere]
3. **Cube Map Projection**: Available as a separate projection type that eliminates the meridian seam

For non-tileable textures, Blender artists commonly use one of:
- The Texture Coordinate node with "Object" output for triplanar-like projection
- UV editing to place the seam in a non-visible area
- Multiple UV layers with blend masks

### 9.3 Industry Practice Summary

Production rendering pipelines typically handle this problem through:

1. **Texture authoring**: Textures intended for spheres are authored as equirectangular maps with matched left/right edges
2. **UV seam placement**: For mesh-based spheres, UV seams are placed in non-visible areas
3. **Procedural textures**: Natural materials use 3D noise functions
4. **Cube maps**: Environment maps and skyboxes universally use cube mapping
5. **Decal projection**: Logos, eyes, and non-repeating features use projected decals on partial coverage

Sources: [PBRT-Spheres, Blender-MappingTypes, Blender-Cycles-Sphere, PBRT-Noise]

---

## 10. Filtering and Blending Techniques at Seam Boundaries {#10-filtering-blending}

**Confidence: High (4 sources)**

### 10.1 Gutter Pixels / Edge Padding

Add extra pixels around texture region boundaries that duplicate the content from the adjacent region. For cube maps, each face gets a 1-2 pixel border from its neighboring faces. At bake time, "at least 4 pixels of edge padding around each shell" is recommended for 512x512 textures; "eight to sixteen pixels" for production quality to protect against mipmap blending [Polycount-EdgePadding, Castano-SeamlessCube].

### 10.2 Coordinate Warping

Scale texture coordinates inward slightly so the bilinear filter footprint never reaches the face edge:
```metal
float scale = (face_size - 1.0) / face_size;
uv = (uv - 0.5) * scale + 0.5;
```
This approach loses one texel row at each edge but prevents any cross-boundary filtering artifacts [Castano-SeamlessCube].

### 10.3 Continuity Mapping (Multi-Chart Textures)

For multi-chart parameterizations (including cube maps), the "Continuity Mapping" technique provides two components:

1. **Traveler's Map**: A bidirectional mapping between areas outside and inside each chart boundary, allowing filtering to "travel" across chart boundaries correctly [Continuity-ACM]
2. **Sewing the Seams**: Stitching triangles evaluated per-fragment to perform consistent linear interpolation between non-adjacent texel values at chart boundaries [Continuity-ACM]

This technique requires no modification of artist-provided textures and achieves continuity with small memory and computational overhead. However, it is designed for mesh-based rendering with per-vertex UVs and is complex to adapt to a ray tracer.

### 10.4 Mipmap-Level Seam Artifacts

When using mipmaps, the seam problem is amplified: at lower mipmap levels, the filter footprint is larger, sampling more texels from the wrong side of the seam. The atan2 discontinuity causes the GPU to compute an enormous screen-space derivative, selecting the smallest mipmap level (a single averaged color) for the entire seam meridian [Golus-SphereQuad].

For ray tracers without hardware mipmapping (like nwave-raytracer), this specific artifact does not occur. However, the bilinear filter at the U-wrap boundary in `sample_texture()` does cross the seam, which is the same class of problem.

---

## 11. Non-Tileable Texture Strategies {#11-non-tileable}

**Confidence: Medium (3 sources)**

For textures that were not designed to wrap (photos, artistic textures, scanned materials), these strategies apply:

### 11.1 Strategy Matrix

| Texture Type | Recommended Approach | Rationale |
|---|---|---|
| Eye / iris texture | Decal projection (existing `texture_scale`) | Texture covers a small area; no wrapping needed |
| Decorative pattern (abstract) | Dual-hemisphere blending | Pattern has no single-point features that would ghost |
| Granite / marble photo | 3D procedural OR texture bombing | Stochastic nature suits random placement; alternatively, generate procedurally |
| Earth / planet map | Equirectangular (already works) | These textures ARE designed to tile; this is the correct mapping |
| Logo / text | Decal projection | Must appear once, undeformed |
| Generic non-tileable image | Cube mapping (single face + padding) | Places seams at 90-degree transitions where they are least visible |

### 11.2 Making a Non-Tileable Texture Tileable (Offline)

If runtime solutions are insufficient, the texture can be preprocessed:

1. **Mirror and blend**: Duplicate the image horizontally, mirror one copy, blend the seam. This doubles the effective pattern period but eliminates the hard seam.

2. **Histogram-preserving blending**: Use the INRIA technique to generate a seamlessly-tiling version from a non-tiling input. This works well for stochastic textures but destroys large-scale structure [INRIA-HPBlend].

3. **Content-aware extension**: Use AI/algorithmic inpainting to extend the texture edges so they match when wrapped.

---

## 12. Recommendation for nwave-raytracer {#12-recommendation}

### 12.1 Immediate Solution: Enhanced Dual-Hemisphere Blending

**Rationale**: This requires the smallest shader change and provides immediate seam elimination for the current texture pipeline. No changes to the scene loader, material system, or texture data format are needed.

**Implementation** (add to `ray_trace.metal`, replacing the current non-equirectangular sphere branch):

```metal
// Dual-hemisphere spherical UV with seam blending
float3 sample_sphere_dual_hemisphere(constant uchar* texture_data, int texture_offset,
                                      float tex_width, float tex_height,
                                      float3 n) {
    // Projection A: standard sphere UV
    float theta = acos(clamp(-n.y, -1.0f, 1.0f));
    float v = 1.0f - theta / M_PI_F;

    float phi_a = atan2(-n.z, n.x) + M_PI_F;
    float u_a = phi_a / (2.0f * M_PI_F);

    // Projection B: rotated 90 degrees around Y axis
    float phi_b = atan2(n.x, n.z) + M_PI_F;
    float u_b = phi_b / (2.0f * M_PI_F);

    // Blend weight: prefer whichever projection is farther from its seam
    float dist_a = min(u_a, 1.0f - u_a);  // 0 at seam, 0.5 at center
    float dist_b = min(u_b, 1.0f - u_b);  // 0 at seam, 0.5 at center
    float weight_b = dist_b / (dist_a + dist_b + 1e-6f);

    float3 color_a = sample_texture_clamped(texture_data, texture_offset,
                                             tex_width, tex_height, u_a, v);
    float3 color_b = sample_texture_clamped(texture_data, texture_offset,
                                             tex_width, tex_height, u_b, v);
    return mix(color_a, color_b, weight_b);
}
```

**Cost**: 2 texture samples instead of 1. No additional memory. No pipeline changes.

**Limitations**: The blending zone (around the seam region) may show slight softening. This is acceptable for granite/marble/organic textures. For high-precision textures (eyes), continue using the existing `texture_scale` decal approach.

### 12.2 Medium-Term Solution: Cube Mapping with Gutter Pixels

For higher quality results, especially with textures that have visible structure:

1. **Scene loader change**: Accept either a single texture (auto-replicate to 6 faces) or 6 separate face images per cube-mapped material
2. **Texture data format**: Store 6 face images contiguously, each with a 2-pixel gutter border
3. **Shader change**: Replace sphere UV computation with cube face selection + per-face 2D lookup
4. **Material struct change**: Add a flag distinguishing `TEXTURE_SPHERICAL` from `TEXTURE_CUBEMAP`

This is more invasive but provides the highest quality solution for arbitrary textures on spheres.

### 12.3 For Natural Materials: Expand Procedural Textures

The nwave-raytracer already has the infrastructure for procedural textures based on the previous research document. For marble, granite, wood, and similar natural materials, 3D Perlin/Worley noise provides inherently seam-free results without any UV mapping. This should be the preferred approach for these texture types.

### 12.4 Decision Matrix

| Use Case | Recommended Approach | Implementation Effort |
|---|---|---|
| Eye texture on sphere | Decal projection (already implemented) | None (existing) |
| Earth/planet map | Equirectangular (already works for tileable textures) | None (existing) |
| Granite/marble on sphere | 3D procedural noise (Perlin + Worley) | Medium (noise functions needed) |
| Decorative non-tileable pattern | Dual-hemisphere blending | Low (shader-only change) |
| High-quality arbitrary texture | Cube mapping with gutter pixels | Medium-High (pipeline change) |
| Logo/brand on sphere | Decal projection with feathering | Low (shader-only change) |

---

## 13. Source Analysis {#13-source-analysis}

| # | Source ID | Source | Type | Used For |
|---|---|---|---|---|
| 1 | PBRT-Spheres | [PBRT: Spheres](https://www.pbr-book.org/3ed-2018/Shapes/Spheres) | Academic book | Sphere UV implementation, atan2 handling, pole perturbation |
| 2 | PBRT-Noise | [PBRT: Noise](https://www.pbr-book.org/3ed-2018/Texture/Noise) | Academic book | 3D procedural textures, noise algorithms |
| 3 | Shirley-RTTNW | [Ray Tracing: The Next Week](https://raytracing.github.io/books/RayTracingTheNextWeek.html) | Academic book | Sphere UV, Perlin noise, texture architecture |
| 4 | Wikipedia-CubeMap | [Cube Mapping (Wikipedia)](https://en.wikipedia.org/wiki/Cube_mapping) | Encyclopedia | Cube map algorithm, face selection, distortion comparison |
| 5 | Wikipedia-UV | [UV Mapping (Wikipedia)](https://en.wikipedia.org/wiki/UV_mapping) | Encyclopedia | Topological constraints, mapping impossibility |
| 6 | Castano-SeamlessCube | [Seamless Cube Map Filtering](http://www.ludicon.com/castano/blog/articles/seamless-cube-map-filtering/) | Technical blog | Gutter pixels, coordinate warping, `fix_cube_lookup` code |
| 7 | GPUOpen-OctCube | [Fetching From Cubes and Octahedrons](https://gpuopen.com/learn/fetching-from-cubes-and-octahedrons/) | AMD technical | Octahedral mapping formulas, VALU cost comparison |
| 8 | Golus-SphereQuad | [Rendering a Sphere on a Quad](https://bgolus.medium.com/rendering-a-sphere-on-a-quad-13c92025570c) | Technical blog | atan2 mipmap artifact, dual UV set technique |
| 9 | GPUGems-PerlinNoise | [GPU Gems Ch. 5: Improved Perlin Noise](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-5-implementing-improved-perlin-noise) | NVIDIA technical | GPU noise implementation |
| 10 | GPUGems-Bombing | [GPU Gems Ch. 20: Texture Bombing](https://developer.nvidia.com/gpugems/gpugems/part-iii-materials/chapter-20-texture-bombing) | NVIDIA technical | Texture bombing algorithm, cell-based placement, GPU cost |
| 11 | INRIA-HPBlend | [High-Performance By-Example Noise (INRIA)](https://dl.acm.org/doi/10.1145/3233304) | Academic paper | Histogram-preserving blending for non-tileable textures |
| 12 | Continuity-ACM | [Continuity Mapping for Multi-Chart Textures](https://dl.acm.org/doi/10.1145/1618452.1618455) | Academic paper | Traveler's Map, seam stitching for multi-chart textures |
| 13 | TechMonkey-Spherical | [Creating Seamless Spherical Textures](https://www.techmonkeybusiness.com/articles/Spherical_Textures.html) | Tutorial | Texture preprocessing for spherical tiling |
| 14 | BookOfShaders-Cellular | [Book of Shaders: Cellular Noise](https://thebookofshaders.com/12/) | Tutorial | Worley/Voronoi noise for GPU |
| 15 | BrianSharpe-GPUCellular | [Optimized Artifact-Free GPU Cellular Noise](https://briansharpe.wordpress.com/2011/12/01/optimized-artifact-free-gpu-cellular-noise/) | Technical blog | GPU Worley noise without discontinuity artifacts |
| 16 | Polycount-EdgePadding | [Edge Padding (Polycount)](http://wiki.polycount.com/wiki/Edge_padding) | Wiki | Gutter pixel sizing recommendations |
| 17 | Blender-MappingTypes | [Blender Manual: Mapping Types](https://docs.blender.org/manual/en/2.82/modeling/meshes/editing/uv/unwrapping/mapping_types.html) | Official docs | Blender sphere/tube projection methods |
| 18 | Catlike-CubeSphere | [Seamless Cube Sphere (Catlike Coding)](https://catlikecoding.com/unity/tutorials/procedural-meshes/seamless-cube-sphere/) | Tutorial | Topological impossibility of seamless 2D-to-sphere mapping |

### Cross-Reference Verification

| Claim | Sources Confirming | Confidence |
|---|---|---|
| No single-chart seamless parameterization of a sphere exists | Catlike-CubeSphere, Wikipedia-UV, PBRT-Spheres | High |
| atan2 produces discontinuity at the meridian | PBRT-Spheres, Golus-SphereQuad, Shirley-RTTNW, Khronos-Sphere | High |
| Cube mapping eliminates meridian seam and pole singularity | Wikipedia-CubeMap, Castano-SeamlessCube, LearnOpenGL-Cubemaps | High |
| Cube map face seams solved by gutter pixels or coordinate warping | Castano-SeamlessCube, Polycount-EdgePadding, Continuity-ACM | High |
| 3D procedural textures have no seam artifacts | PBRT-Noise, Shirley-RTTNW, GPUGems-PerlinNoise | High |
| Octahedral mapping uses 19 VALU ops vs. 10 for cubemap | GPUOpen-OctCube | Medium (single source) |
| Histogram-preserving blending prevents ghosting in texture tiling | INRIA-HPBlend, UnityGrenoble-HPBlend | High |
| PBRT perturbs pole coordinates to avoid undefined atan2 | PBRT-Spheres | Medium (single source, authoritative) |
| Production renderers rely on authored tileable textures for full-sphere coverage | PBRT-Spheres, Blender-MappingTypes, TechMonkey-Spherical | High |
| Texture bombing requires 8 samples for 3D extension | GPUGems-Bombing | Medium (single source, authoritative) |

---

## 14. Knowledge Gaps {#14-knowledge-gaps}

| Gap | What was searched | Why insufficient |
|---|---|---|
| Metal-specific cube map sampling from raw byte buffers | Searched for Metal shader cube map implementation, texturecube sampling | Found extensive information on MTLTexture cube maps (hardware-managed), but no examples of cube map sampling from raw `constant uchar*` byte buffers. The implementation will require custom face selection + per-face byte buffer indexing. |
| Quantitative distortion comparison across all mapping methods | Searched for distortion metrics comparing equirectangular, cube map, octahedral, dual-paraboloid | Found a JCGT paper (Zucker & Higashi, 2018) on cube-to-sphere projections that contains these metrics, but the PDF was not parseable. The paper's abstract indicates "no method has statistically significant benefit over others in preserving information." |
| Ben Golus's two-UV-set technique details | Attempted to fetch https://bgolus.medium.com content | Received 403 from Medium. The article is widely cited as containing a detailed dual-UV sphere mapping solution with code. The algorithm described in Section 3 is reconstructed from secondary citations and the author's description in search snippets. |
| Blender Cycles source code for sphere projection | Searched for Cycles node implementation of spherical projection | Found user-facing documentation but not the actual C/CUDA/OSL source code. The implementation likely uses `atan2` + `acos` similar to PBRT. |
| Performance benchmarks: dual-hemisphere blending vs. cube map on Apple GPU | No benchmarks found | Both approaches involve simple math; the dual-hemisphere approach requires 2 texture fetches vs. 1 for cube mapping, but cube mapping adds face-selection branching. On modern Apple Silicon, both should be negligible relative to the ray-intersection cost. |

---

## Interpretation (Analysis)

*The following are interpretations based on the evidence, not directly sourced claims.*

1. **The dual-hemisphere blending approach is the best first step for nwave-raytracer.** It requires only adding a new function to the Metal shader and modifying the material dispatch logic. No changes to texture loading, material structs, scene format, or the CPU-side pipeline are needed. The trade-off (slight softening in the blend zone) is acceptable for non-precision textures.

2. **The existing `texture_scale` decal approach already solves the eye-texture case.** The eye texture seam problem should not require any new algorithm -- the existing implementation maps the texture to a partial sphere region. If seams are still visible with `texture_scale`, the scale value may be too large (covering past the safe zone) or the texture itself may have edge artifacts.

3. **Cube mapping is the correct long-term solution for arbitrary non-tileable textures on full spheres.** It is the industry standard for a reason: it distributes distortion evenly, eliminates the pole singularity, and with gutter pixels, eliminates face-edge seams. The implementation cost is moderate (face selection logic + 6-face storage) but pays off in versatility.

4. **For the specific textures described (granite, marble, decorative patterns), 3D procedural noise is likely the best solution.** These materials are exactly the use case for Perlin noise + Worley noise. The previous research document already provides the implementation path for procedural textures in the Metal shader. Adding granite (Worley + Perlin combination) and marble (sin + turbulence) to the procedural texture library would eliminate the need to map any rectangular image for these materials.

5. **The triplanar mapping approach in the current shader is fundamentally wrong for spheres.** Triplanar mapping works well for arbitrary mesh surfaces (terrain, walls, rocks) but on a perfect sphere, the three projections compete for dominance at 45-degree latitude lines, creating visible banding. It should be retained only for non-sphere shapes.
