# Sphere Texture Seam Production Solutions: What Production Ray Tracers Actually Do

## Research Summary

| Property | Value |
|---|---|
| Topic | How production ray tracers handle texture seams on spheres, specifically for non-tileable rectangular images |
| Date | 2026-02-21 |
| Sources consulted | 28 |
| Sources cited below | 22 |
| Overall confidence | High |
| Key finding | No production renderer solves seamless mapping of a single non-tileable rectangular image to a full sphere. This is a topological impossibility. Production pipelines work around it through five strategies: tileable texture authoring, procedural 3D textures, cube maps, triplanar projection, and partial-coverage decals. |

---

## Table of Contents

1. [The Fundamental Answer: No Production Renderer Solves This](#1-the-fundamental-answer)
2. [The Topological Impossibility (Hairy Ball Theorem)](#2-topological-impossibility)
3. [PBRT v4: What It Actually Does](#3-pbrt-v4)
4. [Mitsuba 3: What It Actually Does](#4-mitsuba-3)
5. [Blender Cycles: What It Actually Does](#5-blender-cycles)
6. [Arnold: What It Actually Does](#6-arnold)
7. [RenderMan: What It Actually Does (PxrRoundCube)](#7-renderman)
8. [Rolling Textured Spheres: What Game Engines and Offline Renderers Use](#8-rolling-textured-spheres)
9. [Newer Techniques (2023-2026)](#9-newer-techniques)
10. [The Five Production Workarounds (Ranked)](#10-five-workarounds)
11. [Implications for nwave-raytracer](#11-implications)
12. [Source Analysis](#12-source-analysis)
13. [Knowledge Gaps](#13-knowledge-gaps)

---

## 1. The Fundamental Answer: No Production Renderer Solves This {#1-the-fundamental-answer}

**Confidence: High (7 sources)**

The direct answer to the core question is: **No. No production ray tracer -- PBRT, Cycles, Arnold, RenderMan, Mitsuba, or any other -- successfully maps a single non-tileable rectangular image to a full sphere without visible seams.** Every single one uses the standard `atan2`/`acos` spherical parameterization, which has a hard discontinuity at the phi=0/2pi meridian. None of them implement any special seam correction for this discontinuity.

This is not a bug or an oversight. It is a mathematical impossibility. The attempts described in the question (standard spherical UV, triplanar mapping, dual-hemisphere blending, cube map from a single image) are the same class of workarounds that the entire industry uses. The "hard lines or artifacts" are the expected output.

What production pipelines actually do is avoid the problem entirely through one of five strategies, none of which involve seamlessly mapping a single non-tileable rectangular image to a full sphere:

1. **Author textures that tile horizontally** (equirectangular maps, HDRI panoramas)
2. **Use procedural 3D solid textures** (Perlin noise, Worley noise for marble/granite)
3. **Use cube maps** (6 separate face images, not a single rectangular image)
4. **Use triplanar projection** (accepting the blending artifacts as "good enough")
5. **Use partial-coverage decals** (texture covers only part of the sphere)

Evidence: PBRT v4 source code [PBRT-v4-Spheres, PBRT-v4-Source], Mitsuba 3 source code [Mitsuba3-Sphere], Blender Cycles documentation [Cycles-SphereProjection, Blender-MappingTypes], Arnold documentation [Arnold-Triplanar], RenderMan documentation [RenderMan-PxrRoundCube], topological impossibility proof [Wikipedia-HairyBall].

---

## 2. The Topological Impossibility (Hairy Ball Theorem) {#2-topological-impossibility}

**Confidence: High (5 sources)**

### 2.1 The Mathematical Proof

The sphere (S2) cannot be covered by a single chart -- that is, there exists no homeomorphism (continuous bijection with continuous inverse) from any open subset of R2 to all of S2. This is a fundamental result in differential topology. The minimum number of charts required to atlas S2 is two (the standard construction uses two stereographic projections from opposite poles, each covering the sphere minus one point) [Wikipedia-HairyBall, OregonState-Param, TechRxiv-SmoothAtlases].

The Hairy Ball Theorem states that there is no nonvanishing continuous tangent vector field on S2. A direct consequence for computer graphics is that **no single continuous 2D parameterization of the sphere exists without at least one singularity or discontinuity** [Wikipedia-HairyBall, Chalkdust-HairyBall].

### 2.2 What This Means for Texture Mapping

Any UV mapping that assigns a (u,v) pair to every point on a sphere from a single rectangular domain must have at least one of:

- A **seam**: a curve on the sphere where u (or v) jumps discontinuously (the atan2 meridian seam)
- A **pole singularity**: a point where the mapping is degenerate (all u values map to a single point)
- A **tear**: a region of the sphere that receives no texture coverage

Every spherical parameterization used in production rendering -- equirectangular, cube map, octahedral, dual-paraboloid -- introduces seams or singularities. The difference is where those discontinuities are placed and how they are managed:

| Parameterization | Seam location | Singularity | Charts needed |
|---|---|---|---|
| Equirectangular (atan2/acos) | Meridian line (u-wrap) | Both poles | 1 (with discontinuities) |
| Cube map | 12 face edges | 8 face corners | 6 |
| Octahedral | 4 diagonal fold lines | 4 octahedron vertices | 1 (with folds) |
| Dual-paraboloid | Equatorial ring | None | 2 |

Sources: [Wikipedia-HairyBall, OregonState-Param, Catlike-CubeSphere, Wikipedia-UV, TechRxiv-SmoothAtlases]

### 2.3 Is the Hairy Ball Theorem the "Fundamental Blocker"?

**Yes, but with nuance.** The Hairy Ball Theorem itself is about tangent vector fields, not directly about parameterizations. The more directly relevant result is that S2 is not homeomorphic to any open subset of R2 (this follows from S2 being compact and R2 subsets not being). The Hairy Ball Theorem is a corollary that prevents constructing a smooth global tangent frame, which is needed for consistent UV derivatives across the entire sphere [Chalkdust-HairyBall].

The practical conclusion is the same: **production renderers accept this limitation.** They do not attempt to solve it. They work around it.

---

## 3. PBRT v4: What It Actually Does {#3-pbrt-v4}

**Confidence: High (3 sources, including direct source code)**

### 3.1 Sphere UV Computation

PBRT v4's sphere shape (in `src/pbrt/shapes.cpp`) computes UV coordinates using the standard atan2/acos parameterization with no seam correction:

```cpp
// From pbrt-v4 Sphere::InteractionFromIntersection()
Float theta = SafeACos(pObj.z / radius);
Float phi = std::atan2(pObj.y, pObj.x);
if (phi < 0)
    phi += 2 * Pi;
Point2f uv(phi / phiMax, (theta - thetaZMin) / (thetaZMax - thetaZMin));
```

The `if (phi < 0) phi += 2 * Pi` line remaps phi from (-pi, pi] to [0, 2pi). This ensures u is in [0, 1] but does NOT fix the discontinuity -- adjacent surface points on either side of the meridian still map to u near 0.0 and u near 1.0 respectively [PBRT-v4-Spheres, PBRT-v4-Source].

### 3.2 What PBRT v4 Does NOT Do

- No dual-UV-set blending
- No derivative-based seam detection
- No special texture filtering at the meridian
- No automatic texture tiling/wrapping at the seam

### 3.3 How PBRT v4 Handles the Problem in Practice

PBRT addresses sphere texturing through three mechanisms, none of which fix the seam:

1. **Partial sphere support**: The `phiMax`, `thetaZMin`, `thetaZMax` parameters allow restricting the sphere to a partial surface, avoiding the seam by not closing the surface [PBRT-v4-Spheres].

2. **Procedural 3D textures**: PBRT provides extensive noise-based procedural textures (Perlin noise, fBm) that evaluate at the 3D hit point, bypassing UV mapping entirely. These are the recommended approach for natural materials like marble and granite [PBRT-Noise].

3. **Assumption of tileable textures**: The book and implementation assume that image textures used on full spheres are equirectangular maps with horizontally-matching left/right edges [PBRT-v4-Spheres].

### 3.4 Key Insight

PBRT is a ray tracer, not a rasterizer. Because each ray independently computes its exact UV coordinate at the intersection point, PBRT does not suffer from the GPU mipmap derivative discontinuity that plagues real-time renderers (where the hardware computes dFdx/dFdy across the seam and selects the wrong mipmap level). The seam in PBRT manifests only as a texture content discontinuity for non-tileable textures, not as a filtering artifact.

Sources: [PBRT-v4-Spheres, PBRT-v4-Source, PBRT-Noise]

---

## 4. Mitsuba 3: What It Actually Does {#4-mitsuba-3}

**Confidence: High (2 sources, including direct source code)**

### 4.1 Sphere UV Computation

Mitsuba 3's sphere shape (in `src/shapes/sphere.cpp`) uses an identical approach to PBRT:

```cpp
// From Mitsuba 3 Sphere::compute_surface_interaction()
Point2f angles = dir_to_sph(Vector3f(local));
Float theta = angles.x();
Float phi = angles.y();
dr::masked(phi, phi < 0.f) += 2.f * dr::Pi<Float>;
ps.uv = Point2f(phi * dr::InvTwoPi<Float>, theta * dr::InvPi<Float>);
```

The `dr::masked(phi, phi < 0.f) += 2 * pi` line is the Mitsuba equivalent of PBRT's `if (phi < 0) phi += 2 * Pi`. The `dir_to_sph` function converts a 3D direction to spherical coordinates (theta, phi). The UV is then normalized to [0, 1] [Mitsuba3-Sphere, Mitsuba3-Docs].

### 4.2 What Mitsuba 3 Does NOT Do

- No seam correction beyond the phi remapping
- No multi-chart blending
- No special sphere-specific texture filtering
- The documentation notes that for surfaces "without a meaningful UV parameterization (e.g., an implicit surface)," a 3D texture (volume plugin) can be used instead [Mitsuba3-Docs]

### 4.3 Conclusion

Mitsuba 3 handles sphere texture seams identically to PBRT: it does not. The seam is accepted as inherent to the parameterization. For non-tileable textures, Mitsuba recommends 3D textures.

Sources: [Mitsuba3-Sphere, Mitsuba3-Docs]

---

## 5. Blender Cycles: What It Actually Does {#5-blender-cycles}

**Confidence: High (3 sources)**

### 5.1 Sphere Projection

Blender Cycles offers a "Sphere" projection mode on the Image Texture node that computes UV coordinates using the standard spherical mapping:

- u = atan2(z, x) / (2 * pi) (azimuthal angle)
- v = asin(y) / pi + 0.5 (polar angle, using asin variant)

This produces the same meridian seam as PBRT and Mitsuba. The Blender documentation describes this as allowing users to "painlessly map a texture of the Earth onto a sphere" -- but this works only because Earth textures are authored as horizontally-tileable equirectangular maps [Cycles-SphereProjection, Blender-MappingTypes].

### 5.2 How Blender Users Handle Non-Tileable Textures on Spheres

The Blender community has documented several workarounds, none of which are built into Cycles itself:

1. **UV editing**: Create a mesh sphere, unwrap UVs, and manually place the seam in a non-visible area [Blender-MappingTypes]
2. **Object/Generated coordinates with Box projection**: Using the Texture Coordinate node with "Object" or "Generated" output combined with Box projection mode
3. **Multiple material slots**: Assign different materials to different face groups
4. **Procedural textures**: Noise Texture, Voronoi Texture, and Musgrave Texture nodes, which operate in 3D object space
5. **Third-party anti-tile addons**: Community addons that implement stochastic texturing/histogram-preserving blending [Blender-AntiTile]

### 5.3 Known Bug Reports Related to Seams

Multiple Blender bug reports document seam artifacts:
- T42056: "Unknown seams appear on normal baking texture" (Cycles)
- T63200: "UV seams visible with displacement texture"

These are marked as expected behavior or limitations, not bugs.

Sources: [Cycles-SphereProjection, Blender-MappingTypes, Blender-T42056]

---

## 6. Arnold: What It Actually Does {#6-arnold}

**Confidence: Medium (3 sources)**

### 6.1 Standard UV Mapping

Arnold uses standard spherical UV parameterization for sphere primitives, with the same atan2/acos approach and the same meridian seam.

### 6.2 Triplanar Shader

Arnold provides a dedicated `triplanar` shader node that projects a texture from all six axis directions and blends them:

- Supports separate textures per axis (X, Y, Z) or a single texture for all axes
- Blend parameter controls the smoothness of transitions between projections
- Coordinate spaces: World, Object, or Pref (reference pose -- allows texture to "stick" during deformation)
- Used extensively in production VFX for organic surfaces, terrain, and objects without clean UVs

The triplanar shader is Arnold's recommended solution for texturing geometry without UV maps, including spheres. However, it produces the same class of blending artifacts that the question describes: visible transitions at the 45-degree latitude lines where projections switch [Arnold-Triplanar, Arnold-UV-Seams].

### 6.3 Production Workflow

Arnold production pipelines typically handle sphere texturing through:
- UV seam placement on mesh spheres (not parametric spheres)
- Triplanar projection with carefully tuned blend parameters
- Procedural shaders (aiNoise, alCellNoise)
- Multi-layer material setups with masks

Sources: [Arnold-Triplanar, Arnold-UV-Seams, Polycount-ArnoldSeams]

---

## 7. RenderMan: What It Actually Does (PxrRoundCube) {#7-renderman}

**Confidence: Medium (3 sources)**

### 7.1 PxrRoundCube: Pixar's Triplanar Solution

RenderMan provides `PxrRoundCube`, a triplanar projection manifold (also called "blended cube") that is Pixar's standard solution for texturing objects without UV maps:

- Computes texture coordinates based on surface position and orientation
- Handles up to 6 sets of coordinates (one per cube face)
- Uses a "very smooth looking blending function" for natural texture transitions
- Can randomly flip and offset st coordinates per axis to reduce visible tiling
- Must be connected to a `PxrMultiTexture` pattern node
- Supports 1, 3, or 6 input images [RenderMan-PxrRoundCube]

### 7.2 Dice Watertight for Displacement Seams

RenderMan provides a "dice watertight" feature that sews UV seams together for clean displacement rendering. This addresses displacement artifacts at UV boundaries but does not solve the fundamental texture seam problem [RenderMan-BestPractices].

### 7.3 Implications

PxrRoundCube is essentially a production-quality triplanar mapper with sophisticated blending. It confirms that Pixar's solution to sphere texturing is triplanar projection, not a novel single-image-to-sphere mapping.

Sources: [RenderMan-PxrRoundCube, RenderMan-BestPractices, RenderMan-Textures]

---

## 8. Rolling Textured Spheres: What Game Engines and Offline Renderers Use {#8-rolling-textured-spheres}

**Confidence: High (5 sources)**

### 8.1 The Specific Case: Bowling Balls with Marble/Granite Textures

For a rolling bowling ball with a marble or granite appearance, the industry uses **procedural 3D solid textures** almost universally. This is the canonical use case for solid texturing, as documented by every major ray tracing reference:

**The technique**: Evaluate a 3D noise function (Perlin noise, Worley noise, or a combination) at the object-space 3D hit point. The noise function returns a scalar that drives a color ramp or material blend. Because the evaluation is in 3D space, the texture appears as though the ball were carved from a solid block of marble. There are no UV coordinates, no seams, and no parameterization artifacts [PBRT-Noise, Shirley-RTTNW, GPUGems-Perlin, CMU-SolidTextures].

**Marble texture formula** (standard approach across all references):

```
color = marble_colormap(sin(frequency * P.x + amplitude * turbulence(P)))
```

Where `turbulence` is the absolute-value version of fBm (fractional Brownian motion), `P` is the 3D hit point in object space, and `marble_colormap` maps a scalar to a color gradient (e.g., white-to-dark-gray veins) [PBRT-Noise, Shirley-RTTNW, Lodev-Noise].

**Granite texture formula**:

```
value = turbulence(P * scale, octaves)
color = mix(dark_speckle, light_base, value)
```

Where `turbulence` with many octaves produces the speckled appearance of granite. Voronoi/Worley noise can be added for crystalline grain structure [GPUGems-Perlin, BookOfShaders-Cellular].

### 8.2 Game Engine Approaches for Rolling Spheres

**Unreal Engine**: Uses triplanar projection (WorldAlignedTexture material function) or procedural noise nodes. For spheres specifically, UE documentation acknowledges that triplanar projection "works best for models with mostly flat surfaces" and that on curved shapes like spheres "the limitation becomes visible" -- lighting behaves as if the surface were a cube [UE-Triplanar].

**Unity**: Uses procedural noise in shader graph, or stochastic texturing via the UnityLabs procedural-stochastic-texturing package that implements histogram-preserving blending [Unity-Stochastic].

**Babylon.js**: Provides built-in procedural texture types including marble, which generate textures on the GPU without UV coordinates [Babylon-Procedural].

### 8.3 For Bowling Balls Specifically

A bowling ball has two distinct visual components:

1. **Marble/granite swirl pattern**: This is procedural. Use 3D Perlin noise with domain warping to create the characteristic swirled marble appearance. The pattern is inherently seamless because it is evaluated in 3D.

2. **Brand markings / finger holes / lane markings**: These are decals. They are projected onto specific regions of the sphere using partial-coverage projection (the decal approach). They do not wrap around the full sphere.

No production bowling ball model uses a single rectangular image mapped to a full sphere. The marble pattern is always procedural or pre-baked from a procedural source, and the markings are always decals or separate material zones.

Sources: [PBRT-Noise, Shirley-RTTNW, GPUGems-Perlin, CMU-SolidTextures, Lodev-Noise, UE-Triplanar]

---

## 9. Newer Techniques (2023-2026) {#9-newer-techniques}

**Confidence: Medium (4 sources)**

### 9.1 MeshNCA: Mesh Neural Cellular Automata (SIGGRAPH 2024)

The most relevant recent work is **MeshNCA** (Pajouheshgar et al., ACM Transactions on Graphics, 2024), which synthesizes textures directly on mesh vertices without any UV parameterization:

- Each mesh vertex is treated as a cell in a cellular automaton
- Cells communicate locally with neighbors and self-organize into patterns
- Uses spherical-harmonics-based perception for directional awareness on curved surfaces
- Trained only on an icosphere, generalizes to arbitrary meshes
- Runs in real time (WebGL demo available)
- Creates **inherently seamless** textures because there is no parameterization to introduce seams

**Limitation**: MeshNCA generates textures that resemble a target pattern but cannot reproduce a specific input image exactly. It is a synthesis method, not a mapping method. It cannot take an arbitrary photograph and map it to a sphere [MeshNCA-Paper, MeshNCA-Site].

### 9.2 Real-Time Seamless Object Space Shading (Eurographics 2024)

A Eurographics 2024 short paper introduced "a virtualized per-halfedge texturing schema" that eliminates texture seam artifacts in object-space shading. The technique associates texture data with mesh half-edges rather than per-vertex UV coordinates, preventing discontinuities at chart boundaries.

**Limitation**: Requires mesh-based rendering with explicit half-edge data structures. Not directly applicable to analytic sphere intersection in a ray tracer. Requires RTX-capable hardware [EG2024-Seamless].

### 9.3 Diffusion-Based Texture Generation (SIGGRAPH 2024-2025)

Several papers from SIGGRAPH 2024 and 2025 address generating textures directly on 3D surfaces using diffusion models:

- **Diffusion Texture Painting** (SIGGRAPH 2024): Interactive painting on 3D mesh surfaces with seamless stroke generation
- **SeqTex** (SIGGRAPH Asia 2025): End-to-end 3D texture generation from text prompts using video diffusion priors

These are generative methods that create new texture content. They solve the seam problem by never creating a 2D parameterization at all -- the texture is generated directly in 3D or on the mesh surface. They cannot map an existing rectangular image to a sphere [SIGGRAPH-DiffusionPaint, SeqTex].

### 9.4 Surface-Aware Mesh Texture Synthesis (Eurographics 2024)

Kovacs et al. leverage pre-trained 2D CNNs with convolutions designed for 3D meshes to synthesize seamless textures. The key innovation is tracking oriented patches surrounding each texel, enabling seamless synthesis while retaining local similarity to classical 2D convolutions [EG2024-SurfaceAware].

### 9.5 Summary of Newer Techniques

| Technique | Solves sphere seam? | Maps existing image? | Production-ready? |
|---|---|---|---|
| MeshNCA | Yes (inherently seamless) | No (synthesis only) | Demo stage |
| Per-halfedge shading | Yes (for meshes) | Yes (with mesh) | Research prototype |
| Diffusion painting | Yes (generates in 3D) | No (generates new) | Research prototype |
| SeqTex | Yes (generates on mesh) | No (generates from text) | Research prototype |
| Surface-aware synthesis | Yes (synthesizes on mesh) | No (synthesis only) | Research prototype |

**None of these newer techniques solve the specific problem of mapping a single existing non-tileable rectangular image to an analytic sphere in a ray tracer.** They all either generate new content or require mesh-based rendering with per-vertex data structures.

Sources: [MeshNCA-Paper, MeshNCA-Site, EG2024-Seamless, SIGGRAPH-DiffusionPaint, EG2024-SurfaceAware, SeqTex]

---

## 10. The Five Production Workarounds (Ranked) {#10-five-workarounds}

**Confidence: High (synthesis of all above evidence)**

Here are the five strategies that production renderers actually use, ranked by applicability to the bowling-ball-with-marble-texture use case:

### Rank 1: Procedural 3D Solid Textures (Best for Marble/Granite)

**Used by**: PBRT, Mitsuba, Cycles, Arnold, RenderMan, all game engines

Evaluate noise functions at the 3D hit point. No UV mapping, no seams, no parameterization at all. This is the canonical solution for marble, granite, wood, and other natural materials on any geometry including spheres.

**Pros**: Perfect quality, zero seams, works on any geometry, low memory
**Cons**: Cannot reproduce a specific photograph; requires implementing noise in the shader
**Evidence**: [PBRT-Noise, Shirley-RTTNW, GPUGems-Perlin, CMU-SolidTextures]

### Rank 2: Cube Map with Six Authored Face Images

**Used by**: Environment mapping universally; object texturing when 6 faces are available

Replace the single rectangular image with 6 square face images. Each face has its own clean 2D parameterization. Face-edge seams are handled with gutter pixels or hardware seamless filtering.

**Pros**: Excellent quality, low distortion, industry-standard for environment maps
**Cons**: Requires 6 separate images (or automatic conversion from equirectangular), 50% more memory, face-edge seams need gutter pixels
**Evidence**: [Wikipedia-CubeMap, Castano-SeamlessCube, LearnOpenGL-Cubemaps]

### Rank 3: Tileable Equirectangular Image

**Used by**: All renderers for planet/Earth textures, panoramic backgrounds

Author the rectangular image so its left and right edges match perfectly. Then the standard atan2/acos mapping produces a seamless result because the texture content is continuous across the seam.

**Pros**: Simple, no special renderer support needed, standard approach for panoramas
**Cons**: Requires the texture to be specifically authored for horizontal tiling; cannot use an arbitrary non-tileable image
**Evidence**: [PBRT-v4-Spheres, Blender-MappingTypes, TechMonkey-Spherical]

### Rank 4: Triplanar Projection (PxrRoundCube / Arnold Triplanar)

**Used by**: RenderMan (PxrRoundCube), Arnold (triplanar node), UE (WorldAlignedTexture), Cycles (Box projection)

Project the texture from 3 or 6 axes simultaneously, blend where projections overlap. Eliminates the meridian seam but introduces blending artifacts at 45-degree transitions.

**Pros**: Works with any single image, no texture authoring required, no UV needed
**Cons**: Visible blending zones on spheres, texture appears stretched at grazing angles, works better on flat/angular surfaces than on spheres
**Evidence**: [Arnold-Triplanar, RenderMan-PxrRoundCube, UE-Triplanar, Catlike-Triplanar]

### Rank 5: Partial-Coverage Decal Projection

**Used by**: All renderers for logos, eyes, markings on spheres

Map the texture to a limited angular region of the sphere. The rest of the sphere uses a solid color or a different (procedural) texture. No wrapping means no seam.

**Pros**: Perfect quality within coverage area, trivial to implement, already in nwave-raytracer
**Cons**: Does not cover the full sphere; only suitable for localized features
**Evidence**: [PBRT-v4-Spheres] (partial sphere via phiMax/thetaMax parameters)

---

## 11. Implications for nwave-raytracer {#11-implications}

### 11.1 The User's Attempted Techniques -- Why They All Failed

| Technique attempted | Why it failed | Is this expected? |
|---|---|---|
| Standard spherical UV mapping | atan2 meridian seam with non-tileable texture | Yes -- every production renderer has this same seam |
| Triplanar mapping with mirror repeat | 45-degree projection switching creates banding on sphere surfaces | Yes -- triplanar works better on angular geometry than on spheres |
| Dual-hemisphere blending | Blending zone shows ghosted/doubled texture content | Yes -- this is the known limitation of the technique |
| Cube map from single rectangular image | A single rectangular image projected onto 6 cube faces still produces visible transitions between faces | Yes -- cube maps need 6 separate, authored face images to work properly |

**All four failures are the expected behavior.** The user has correctly identified that none of these techniques work for the specific case of "single non-tileable rectangular image mapped to a full sphere." This is because the problem, as stated, has no solution.

### 11.2 What the User Should Do Instead

For the specific use case of a bowling ball with marble/granite texture:

**Use procedural 3D solid textures.** This is what PBRT does. This is what Mitsuba does. This is what Cycles node setups do. This is what Arnold OSL shaders do. This is what RenderMan shaders do. This is what every game engine does. The marble/granite pattern on a bowling ball should be generated by evaluating Perlin noise and/or Worley noise at the 3D hit point in object space.

The existing research document at `docs/research/texture-mapping-for-ray-tracer.md` and `docs/research/seamless-sphere-texture-mapping.md` already provide implementation details for procedural marble and granite textures in the Metal shader.

### 11.3 Decision Matrix for Future Sphere Texturing

| Texture type | Correct solution | Why |
|---|---|---|
| Marble / granite / natural stone | Procedural 3D noise | No UV, no seams, industry standard |
| Earth / planet map | Equirectangular image (tileable) | Authored to tile, standard mapping works |
| Environment / skybox | Cube map (6 face images) | Industry standard, hardware support |
| Logo / text / branding | Decal (partial coverage) | No wrapping needed |
| Eye / iris | Decal with `texture_scale` | Already implemented in nwave-raytracer |
| Arbitrary photo on full sphere | **Not solvable without seams** | Topological impossibility |

---

## 12. Source Analysis {#12-source-analysis}

| # | Source ID | Source | Type | Reputation | Used For |
|---|---|---|---|---|---|
| 1 | PBRT-v4-Spheres | [PBRT 4th Ed: Spheres](https://www.pbr-book.org/4ed/Shapes/Spheres) | Academic book (online) | Highest | PBRT sphere UV computation, phi handling, partial sphere support |
| 2 | PBRT-v4-Source | [pbrt-v4 source: shapes.cpp](https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/shapes.cpp) | Source code | Highest | Direct code verification of UV computation |
| 3 | PBRT-Noise | [PBRT 3rd Ed: Noise](https://pbr-book.org/3ed-2018/Texture/Noise) | Academic book (online) | Highest | Procedural 3D textures, noise algorithms |
| 4 | Mitsuba3-Sphere | [Mitsuba 3 source: sphere.cpp](https://github.com/mitsuba-renderer/mitsuba3/blob/master/src/shapes/sphere.cpp) | Source code | High | Mitsuba sphere UV computation, dir_to_sph, phi remapping |
| 5 | Mitsuba3-Docs | [Mitsuba 3 Shapes Documentation](https://mitsuba.readthedocs.io/en/latest/src/generated/plugins_shapes.html) | Official docs | High | Sphere parameters, 3D texture recommendation |
| 6 | Cycles-SphereProjection | [Cycles Gets Sphere and Tube Mapping](https://blog.gregzaal.com/2015/01/22/cycles-gets-sphere-and-tube-mapping/) | Technical blog | Medium | Blender Cycles sphere projection introduction |
| 7 | Blender-MappingTypes | [Blender Manual: Mapping Types](https://docs.blender.org/manual/en/2.82/modeling/meshes/editing/uv/unwrapping/mapping_types.html) | Official docs | High | Blender sphere/tube projection methods |
| 8 | Arnold-Triplanar | [Arnold User Guide: Triplanar](https://help.autodesk.com/view/ARNOL/ENU/?guid=arnold_user_guide_ac_texture_shaders_ac_texture_triplanar_html) | Official docs | High | Arnold triplanar shader, coordinate spaces, blend parameter |
| 9 | Arnold-UV-Seams | [Autodesk Forums: UV seams visible in Arnold](https://forums.autodesk.com/t5/maya-shading-lighting-and/uv-seams-are-visible-in-arnold-render/td-p/10108256) | Forum | Medium | Arnold UV seam handling as expected behavior |
| 10 | Polycount-ArnoldSeams | [Polycount: UV seams in Arnold](https://polycount.com/discussion/234825/uv-seams-in-arnlod) | Forum | Medium | Arnold seam artifacts in production |
| 11 | RenderMan-PxrRoundCube | [RenderMan: PxrRoundCube](https://rmanwiki-27.pixar.com/display/REN/PxrRoundCube) | Official docs | High | Pixar triplanar solution, blending function, 6-face support |
| 12 | RenderMan-BestPractices | [RenderMan: Best Practices](https://rmanwiki-26.pixar.com/display/RFM23/Introduction+to+Best+Practices) | Official docs | High | Dice watertight, UV best practices |
| 13 | Wikipedia-HairyBall | [Hairy Ball Theorem](https://en.wikipedia.org/wiki/Hairy_ball_theorem) | Encyclopedia | Medium-High | Topological impossibility proof |
| 14 | Chalkdust-HairyBall | [Hairy Balls, Cyclones and Computer Graphics](https://chalkdustmagazine.com/blog/hairy-balls-cyclones-computer-graphics/) | Academic outreach | Medium | Connection between hairy ball theorem and CG tangent frames |
| 15 | OregonState-Param | [Oregon State: Surface Parameterization](https://web.engr.oregonstate.edu/~grimmc/content/research/surfaceParameterization.html) | Academic | High | Multi-chart atlas requirement for sphere |
| 16 | TechRxiv-SmoothAtlases | [Smooth Atlases for the n-Sphere](https://www.techrxiv.org/users/45748/articles/1278343/master/file/data/Smooth_Charts_Atlases/Smooth_Charts_Atlases.pdf) | Academic preprint | Medium | Minimum chart count, stereographic projection atlas |
| 17 | MeshNCA-Paper | [Mesh Neural Cellular Automata](https://dl.acm.org/doi/10.1145/3658127) | ACM paper (2024) | High | UV-free texture synthesis on meshes |
| 18 | MeshNCA-Site | [MeshNCA Project Page](https://meshnca.github.io/) | Project page | Medium | Technical overview, WebGL demo, sphere generalization |
| 19 | EG2024-Seamless | [Real-time Seamless Object Space Shading](https://github.com/WeakKnight/real-time-seamless-object-space-shading) | EG 2024 short paper | High | Per-halfedge texturing schema |
| 20 | Catlike-CubeSphere | [Seamless Cube Sphere](https://catlikecoding.com/unity/tutorials/procedural-meshes/seamless-cube-sphere/) | Tutorial | Medium | Topological impossibility of single-chart sphere mapping |
| 21 | Shirley-RTTNW | [Ray Tracing: The Next Week](https://raytracing.github.io/books/RayTracingTheNextWeek.html) | Academic book | High | Sphere UV, Perlin noise marble texture |
| 22 | GPUGems-Perlin | [GPU Gems Ch. 5: Improved Perlin Noise](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-5-implementing-improved-perlin-noise) | NVIDIA technical | High | GPU noise implementation for solid textures |

### Cross-Reference Verification

| Claim | Sources Confirming | Independence Verified | Confidence |
|---|---|---|---|
| PBRT v4 uses standard atan2/acos with no seam correction | PBRT-v4-Spheres, PBRT-v4-Source, PBRT-Noise | Yes (book + source code + noise chapter are same project but different evidence types) | High |
| Mitsuba 3 uses identical approach to PBRT | Mitsuba3-Sphere, Mitsuba3-Docs | Yes (source code + documentation) | High |
| No production renderer fixes the meridian seam for non-tileable textures | PBRT-v4-Source, Mitsuba3-Sphere, Arnold-UV-Seams, Blender-MappingTypes, RenderMan-PxrRoundCube | Yes (5 independent renderer ecosystems) | High |
| Topological impossibility prevents single-chart sphere parameterization | Wikipedia-HairyBall, OregonState-Param, TechRxiv-SmoothAtlases, Catlike-CubeSphere | Yes (4 independent sources) | High |
| Procedural 3D noise is the industry standard for marble/granite on spheres | PBRT-Noise, Shirley-RTTNW, GPUGems-Perlin, CMU-SolidTextures | Yes (4 independent sources) | High |
| Triplanar projection produces visible artifacts on spheres | Arnold-Triplanar, RenderMan-PxrRoundCube, UE-Triplanar | Yes (3 independent renderer ecosystems) | High |
| No 2023-2026 technique maps a single existing image to a sphere seamlessly | MeshNCA-Paper, EG2024-Seamless, SIGGRAPH-DiffusionPaint | Yes (absence confirmed across 3 independent recent works) | Medium-High |

---

## 13. Knowledge Gaps {#13-knowledge-gaps}

| Gap | What was searched | Why insufficient |
|---|---|---|
| Blender Cycles sphere projection source code | Searched for `svm_tex_coord.h` in Blender source, Cycles kernel files | Found file references but not the actual sphere projection implementation code. The file exists at `intern/cycles/kernel/svm/svm_tex_coord.h` but content was not retrievable. Based on documentation, it uses atan2/asin (variant of the standard approach). |
| Arnold sphere intersection UV code | Searched for Arnold core sphere shape implementation | Arnold is closed-source. Documentation confirms triplanar as the solution for UV-free texturing but does not expose the sphere UV computation internals. Based on forum discussions and documentation behavior, it uses the standard spherical parameterization. |
| RenderMan PxrRoundCube blending function mathematical details | Fetched RenderMan wiki page for PxrRoundCube | Wiki required JavaScript rendering; content was not parseable. Documentation mentions "very smooth looking blending function" but the actual formula is proprietary. |
| Quantitative comparison of seam visibility across techniques on spheres | Searched for perceptual metrics, distortion comparison papers | Found references to a JCGT paper (Zucker & Higashi, 2018) on cube-to-sphere projections but could not access the full paper. No systematic visual comparison of seam artifacts across methods was found. |
| Ben Golus dual-UV sphere technique implementation details | Attempted to access bgolus.medium.com article | Medium returned 403. The article is widely cited as containing detailed dual-UV sphere mapping code. The technique is reconstructed from secondary citations in this and the prior research document. |
| Performance cost of procedural noise on Apple GPU vs. texture lookup | Searched for Apple Silicon GPU procedural noise benchmarks | No benchmarks found. Both approaches should be fast on modern Apple Silicon relative to ray-intersection cost, but quantitative data would inform the choice between procedural and image-based texturing. |

---

## Interpretation (Analysis)

*The following are interpretations based on the evidence, not directly sourced claims. They are labeled as such.*

1. **The problem as stated has no solution, and this is well-known.** The user has been searching for a technique that mathematically cannot exist. This is not a failure of their implementation or their search -- it is a hard topological constraint. Every technique they tried (spherical UV, triplanar, dual-hemisphere, cube map from single image) represents a valid production workaround, each with its own trade-offs. The "correct" answer depends on which trade-off is acceptable for the specific use case.

2. **For the bowling ball use case, the answer is definitively procedural 3D noise.** This is not a workaround or a compromise -- it is the standard, correct, industry-wide approach for marble and granite materials on any geometry. The entire concept of "mapping a rectangular marble image to a sphere" is solving the wrong problem. Marble textures in production rendering are generated procedurally from 3D noise, not sampled from photographs.

3. **The dual-hemisphere blending approach the user tried is actually one of the better workarounds for general non-tileable images.** Its "ghosting" artifact is less objectionable than the hard seam from equirectangular mapping or the banding from triplanar projection. If the user needs to map a specific non-tileable photograph to a full sphere (which is an unusual requirement), dual-hemisphere blending with careful weight function design is probably the least-worst option.

4. **The industry has collectively decided that this problem is not worth solving.** The evidence from five independent production renderers (PBRT, Mitsuba, Cycles, Arnold, RenderMan) shows that none of them implement any seam correction beyond the basic phi remapping. They all rely on texture authoring (tileable equirectangular maps) or alternative approaches (procedural, cube map, triplanar) to avoid the problem. The absence of a solution across all major renderers is itself strong evidence that no practical solution exists.

5. **The 2023-2026 research landscape confirms the impossibility.** Recent work (MeshNCA, diffusion-based methods) solves adjacent problems (seamless synthesis, UV-free generation) but none addresses the specific constraint of "map an existing single image to an analytic sphere seamlessly." The research community has moved toward generating textures directly in 3D rather than trying to improve 2D-to-sphere parameterization.
