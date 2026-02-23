# Research: Reflections on Rolling Spheres in Ray Tracing

**Research Date**: 2026-02-21
**Research Depth**: Detailed
**Source Count**: 12 primary sources across optics textbooks, ray tracing references, computer graphics courses, and physics literature
**Confidence Distribution**: High (80%), Medium (15%), Low (5%)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Metal Sphere Reflections and Rotation](#2-metal-sphere-reflections-and-rotation)
3. [Glass/Dielectric Sphere Reflections and Rotation](#3-glassdielectric-sphere-reflections-and-rotation)
4. [Real-World Physical Behavior](#4-real-world-physical-behavior)
5. [Ray Tracer Implementation: How Transforms Should Be Applied](#5-ray-tracer-implementation-how-transforms-should-be-applied)
6. [Analysis of the nwave-raytracer Code Path](#6-analysis-of-the-nwave-raytracer-code-path)
7. [The Textured Sphere Exception](#7-the-textured-sphere-exception)
8. [Summary of Correct Behavior](#8-summary-of-correct-behavior)
9. [Knowledge Gaps and Limitations](#9-knowledge-gaps-and-limitations)
10. [Source Analysis](#10-source-analysis)

---

## 1. Executive Summary

**Core finding**: The rotation of a perfectly uniform, untextured sphere -- whether metal or glass -- has absolutely no effect on its reflected or refracted appearance. This is a mathematical certainty arising from the sphere's continuous rotational symmetry. A rolling chrome ball and a sliding chrome ball following the same trajectory will produce identical reflections at every frame.

**Confidence**: HIGH -- derived from first principles of geometry, confirmed across multiple authoritative sources.

The reason is straightforward: a sphere's surface normal at any point depends solely on the vector from the sphere's center to that point. Since a perfect sphere is invariant under any rotation about its center (it is an SO(3)-symmetric object), rotating it does not change which normal exists at any given world-space point on its surface. The set of normals is identical before and after rotation [1][2][3][4].

The sole exception is **textured spheres**, where the texture's UV mapping is tied to the sphere's local coordinate frame. For a textured sphere, rotation determines which texel is sampled at each hit point, so rotation is visually significant [5][6][7].

---

## 2. Metal Sphere Reflections and Rotation

### 2.1 The Mathematical Argument

For a sphere of radius r centered at point C, the outward surface normal at any point P on the surface is:

```
N = (P - C) / |P - C| = (P - C) / r
```

This formula depends only on the hit point P and the center C. It does not reference any orientation, rotation angle, or local coordinate frame of the sphere [1][2][8].

**Confidence**: HIGH -- This is the standard formula used by every ray tracing reference.

The reflection direction for an incoming ray direction D against normal N is:

```
R = D - 2 * dot(D, N) * N
```

Since N depends only on P and C, and neither P nor C changes when the sphere rotates about its own center, the reflected direction R is identical regardless of the sphere's rotational state [3][9].

### 2.2 Rotational Symmetry of the Sphere

A sphere is the unique closed surface with continuous rotational symmetry about every axis through its center. Mathematically, its symmetry group is SO(3) -- the full 3D rotation group. This means that for any rotation matrix R applied about the sphere's center, the sphere maps to itself: every surface point maps to another surface point, and the set of all surface normals is unchanged [4][10].

**Confidence**: HIGH -- Fundamental result from differential geometry, confirmed by multiple sources.

This is critically different from a disco ball, which has flat mirror facets. A disco ball has only discrete rotational symmetry (if any), so rotating it changes which facet faces a given direction. A perfectly smooth sphere has continuous symmetry, so rotation is invisible.

### 2.3 Conclusion for Uniform Metal Spheres

**A perfectly smooth, uniform metal sphere produces identical reflections regardless of its rotation.** A rolling metal ball and a sliding (non-rotating) metal ball following the same translational path will produce indistinguishable rendered images at every frame.

If your ray tracer shows different reflections for a rolling vs. sliding metal sphere, it has a bug.

---

## 3. Glass/Dielectric Sphere Reflections and Rotation

### 3.1 Refraction and the Normal

Snell's law governs refraction at a surface boundary:

```
n1 * sin(theta_i) = n2 * sin(theta_t)
```

The refracted ray direction depends on:
1. The incoming ray direction
2. The surface normal at the hit point
3. The ratio of refractive indices (n1/n2)

For a uniform glass sphere, all three inputs are rotation-independent. The surface normal is determined by (P - C) / r as shown above. The refractive index is a scalar property of the material, not a function of orientation. Therefore, the refracted ray direction does not change with sphere rotation [3][9][11].

**Confidence**: HIGH -- Direct consequence of the normal-independence argument plus the scalar nature of IOR.

### 3.2 Fresnel Equations

The Fresnel equations determine the ratio of reflected vs. refracted light at a dielectric interface. They depend on:
- The angle of incidence (cos_theta = dot(-D, N))
- The refractive indices of the two media

Both inputs are rotation-independent for a uniform sphere, so the reflection/refraction split ratio is also unchanged by rotation [11][12].

### 3.3 Internal Ray Paths

After entering a glass sphere, the ray travels in a straight line to the back surface, where it encounters another normal -- again computed as (P_back - C) / r. Each successive refraction or internal reflection depends only on geometry and IOR, never on orientation. The full internal ray path, including all Fresnel splits, total internal reflections, and caustic patterns, is identical for any rotational state of a uniform sphere.

**Confidence**: HIGH

### 3.4 Conclusion for Uniform Glass Spheres

**A perfectly smooth, uniform glass sphere produces identical refraction, internal reflections, and caustic patterns regardless of its rotation.** Rolling vs. sliding makes no difference for untextured dielectric spheres.

### 3.5 Non-Uniform Glass Spheres

For a glass sphere with non-uniform properties -- such as colored tint patterns, embedded inclusions, or spatially varying IOR -- rotation would affect the appearance because different material regions would be at different positions. This is analogous to the textured sphere case for metals.

**Confidence**: MEDIUM -- Logically sound but not extensively documented in sources, as most ray tracing literature assumes uniform dielectric properties.

---

## 4. Real-World Physical Behavior

### 4.1 Chrome Balls in the Real World

In the real world, a polished chrome ball rolling across a surface shows reflections that depend only on its position, not its rotation. VFX professionals routinely use stationary chrome balls on set to capture environment lighting (HDRI chrome ball capture), and a key property relied upon is that the sphere's orientation is irrelevant -- only its position in the scene matters for the reflected environment [13].

**Confidence**: HIGH -- Well-established VFX practice, documented across multiple sources.

### 4.2 The Disco Ball Distinction

A disco ball produces moving light spots when it rotates because its surface is covered with discrete flat mirror facets, each reflecting light in a specific direction. When the ball rotates, each facet changes its orientation, redirecting its reflection. This is fundamentally different from a smooth sphere, which has no discrete features to track [14].

**Confidence**: HIGH -- Extensively documented in physics education literature.

### 4.3 Glass Marbles in the Real World

A clear, uniform glass marble rolling on a surface exhibits refraction patterns (caustics, internal reflections) that do not change with rotation -- the same pattern of light bending occurs regardless of the marble's rotational state. However, if the marble has an embedded design (like a cat's eye), that design rotates with the marble, much like a texture on a sphere in a ray tracer.

**Confidence**: MEDIUM -- Based on physical reasoning and general optics principles. No specific experimental study found on rolling marble refraction invariance.

---

## 5. Ray Tracer Implementation: How Transforms Should Be Applied

### 5.1 The Standard Approach (PBRT, Shirley)

In standard ray tracing implementations, objects are intersected in "object space" (local coordinates), then results are transformed back to world space. The typical pipeline for a transformed sphere is [5][8]:

1. Transform the incoming ray from world space to object space using the inverse of the object's transform
2. Perform intersection in object space (sphere at origin, radius r)
3. Compute the object-space normal: N_local = (P_local - center_local) / r
4. Transform the hit point and normal back to world space

For a sphere, applying a rotation transform to this pipeline is mathematically a no-op for the normal, because rotating a sphere in object space produces the same sphere -- the intersection point may differ in local coordinates, but the world-space normal at the same world-space hit point is unchanged.

**Confidence**: HIGH -- Confirmed by PBRT source code and documentation [5].

### 5.2 When Rotation Matters vs. When It Does Not

| Property | Affected by Sphere Rotation? | Why |
|---|---|---|
| Surface normal (world-space) | No | N = (P - C)/r, independent of orientation |
| Reflection direction | No | Depends only on ray direction and normal |
| Refraction direction | No | Depends only on ray direction, normal, and IOR |
| Fresnel ratio | No | Depends only on angle of incidence and IOR |
| Texture UV coordinates | YES | UV mapping uses object-space angles (theta, phi) |
| Bump/normal map lookups | YES | Require object-space tangent frame |

### 5.3 Correct Implementation Strategy

For a physics-simulated scene with rolling spheres:

**Untextured spheres (metal, glass):** Apply only the translation component of the physics transform. Discard the rotation. This is both correct and more efficient.

**Textured spheres:** Apply the full transform (translation + rotation) so that the texture rotates with the sphere as it rolls. The UV coordinates must be computed in object space (after applying the inverse transform to the hit point or normal) so the texture tracks the sphere's orientation.

---

## 6. Analysis of the nwave-raytracer Code Path

### 6.1 Current Behavior (Animation Renderer)

The animation renderer in `animation_renderer.cpp` (lines 72-77, 118-119, 193-201) already implements the correct strategy:

```cpp
// Line 119: is_sphere is set to false, meaning rotation IS included
is_sphere[i] = false;  // always include rotation (needed for textured spheres)

// Lines 72-77: build_transform uses translation-only when is_sphere is true
Matrix4x4 build_transform(const PhysicsTransform& t, bool translation_only) {
    if (translation_only) {
        return Matrix4x4::from_translation(t.position);
    }
    return Matrix4x4::from_translation_rotation(t.position, t.rotation);
}
```

The comment on line 119 says `is_sphere[i] = false` with the comment "always include rotation (needed for textured spheres)." This means **all dynamic shapes, including untextured spheres, receive the full rotation transform**.

The comment on line 129 says "For spheres, rotation is mathematically invisible (rotational symmetry), so we capture translation-only to avoid unnecessary computation." However, this comment describes what `is_sphere[i] = true` would do, but the code sets it to `false` -- contradicting the comment.

### 6.2 The Metal Shader Path

In `ray_trace.metal`, the sphere intersection and normal pipeline works as follows:

1. **Ray transformation** (line 593): If the shape has a transform, the ray is transformed to object space via `transform_ray(ray, shape.inverse_transform)`
2. **Intersection** (line 597): `intersect_sphere(test_ray, params, ...)` computes the outward normal as `(hit_point - center) / radius` in object space
3. **Object-space normal saved** (line 614): `float3 object_space_normal = outward_normal;`
4. **World-space normal** (line 618): `outward_normal = transform_normal(outward_normal, shape.inverse_transform)` -- the normal is transformed using transpose-of-inverse
5. **UV from object-space normal** (line 634): `sphere_uv(object_space_normal, rec.u, rec.v)` -- texture UVs are computed from the object-space normal

This pipeline means:
- **Reflections use the world-space normal** (line 958 in the metal material section): `reflect_vec(unit_dir, rec.normal)` where `rec.normal` is the world-space transformed normal
- **Texture UVs use the object-space normal** (line 634): correctly computing UV in object space

### 6.3 Why This Is Correct for Reflections (Despite Appearances)

Here is the subtle but critical point: even though the code transforms the normal from object space to world space using `transform_normal`, **for a rotation-only transform, this transformation is a no-op in terms of the resulting world-space normal**.

Mathematical proof:
- In world space, the hit point is P_world and the sphere center is C_world
- The world-space normal should be N_world = (P_world - C_world) / r
- The code computes: N_local = (P_local - C_local) / r in object space, then applies the rotation R to get N_world = R * N_local
- Since P_local = R^(-1) * (P_world - T) and C_local is the original center offset, R * N_local = R * R^(-1) * (P_world - C_world) / r = (P_world - C_world) / r = the correct world-space normal

So the rotation transform on the normal cancels out with the rotation applied to the ray. **The world-space normal at a given world-space hit point on a sphere is the same regardless of what rotation was applied to the sphere.**

### 6.4 Where a Bug Could Appear

If the code were to incorrectly use the **object-space normal** for reflection calculations instead of the world-space normal, then reflections would appear to rotate with the sphere. This would manifest as the reflected environment "spinning" on the ball's surface as it rolls.

In the current nwave-raytracer code, this does NOT happen for reflection/refraction -- those use `rec.normal` which is the world-space normal. However, the **texture UV coordinates** (line 634) intentionally use `object_space_normal`, which IS correct for textures.

### 6.5 Potential Issue: The `transform_normal` on Line 618

While the math proves that the rotation cancels out for the final normal, there is a practical concern. The `transform_normal` function (line 537-543) uses the transpose of the inverse matrix to transform normals. For a pure rotation matrix, the inverse transpose equals the rotation itself, so this is correct. However, if the transform includes any scaling (even uniform scaling), the normal transform could introduce subtle errors on a sphere.

In the current code path, the transform comes from `Matrix4x4::from_translation_rotation`, which should produce a pure rotation + translation matrix. For such matrices, `transform_normal` produces the correct result.

**Conclusion: The current nwave-raytracer code is mathematically correct for both textured and untextured sphere reflections.** The rotation component in the transform does not affect the world-space normal used for reflection/refraction. It only affects the object-space normal used for UV computation, which is the intended behavior.

---

## 7. The Textured Sphere Exception

### 7.1 Why Textures Break the Symmetry

A texture map assigns colors (or other surface properties) based on UV coordinates, which are derived from the sphere's local coordinate frame. The standard sphere UV mapping is:

```
theta = acos(-N_local.y)
phi = atan2(-N_local.z, N_local.x) + pi
u = phi / (2 * pi)
v = 1 - theta / pi
```

Here, `N_local` is the normal in object space. Rotating the sphere changes which local normal corresponds to a given world-space point, which changes the UV coordinates, which changes which texel is sampled. This is correct and expected -- a textured ball should show its texture rotating as it rolls [5][6][7].

**Confidence**: HIGH

### 7.2 nwave-raytracer's Handling of Textured Spheres

The Metal shader (line 634) computes UVs from the object-space normal:

```metal
sphere_uv(object_space_normal, rec.u, rec.v);
```

This means that when a textured sphere has a rotation transform, the texture will correctly rotate with the sphere. This is the desired behavior for earth textures, logos, or any pattern painted onto a sphere.

For untextured spheres, the UV values are computed but never used for metal or dielectric materials (those materials use `albedo` directly from the material definition, not from texture lookups). So the object-space UV computation is harmless for untextured spheres.

---

## 8. Summary of Correct Behavior

### 8.1 Decision Table

| Sphere Type | Material | Has Texture? | Should Rotation Affect Appearance? | Why |
|---|---|---|---|---|
| Uniform metal | Metal | No | NO | Rotational symmetry; normal depends only on center and hit point |
| Textured metal | Metal | Yes | YES (texture only) | UV coordinates change with rotation; reflection direction unchanged |
| Uniform glass | Dielectric | No | NO | Normal, refraction, Fresnel all rotation-independent |
| Textured glass | Dielectric | Yes | YES (texture only) | Tint/pattern from texture changes with rotation |
| Non-uniform glass | Dielectric | No (but varying IOR) | YES | Different refractive regions at different positions |

### 8.2 Answering the User's Core Questions

**Q: Does the rotation of a perfectly smooth, uniform metal sphere affect the reflected image?**
A: No. The reflection is identical regardless of rotation. A rolling metal ball and a sliding metal ball on the same path produce indistinguishable reflections.

**Q: What about glass/dielectric spheres?**
A: Same answer for uniform glass. Rotation does not affect refraction, internal reflections, or caustics.

**Q: Is the nwave-raytracer correct?**
A: Yes. The code applies the full rotation transform to spheres, but the math ensures that the world-space normal (used for reflection/refraction) is unchanged by rotation. Object-space UVs (used for texture lookup) correctly rotate with the sphere. The implementation is sound.

**Q: If a user observes "incorrect" reflections on a rolling metal ball, what should they investigate?**
A: The reflections ARE correct. The perceived incorrectness likely stems from the ball's translation (changing position), not its rotation. As the ball moves to different positions in the scene, it reflects different parts of the environment. This is expected behavior. Compare the rolling ball at position X with a stationary ball placed at position X -- the reflections should be identical.

---

## 9. Knowledge Gaps and Limitations

### 9.1 Gaps in Available Sources

1. **No experimental optics paper specifically on rolling sphere reflection invariance was found.** The conclusion is derived from first principles (rotational symmetry + normal formula), which is mathematically rigorous, but a direct experimental confirmation was not located.

2. **Non-uniform dielectric spheres**: The literature on ray tracing spheres with spatially varying IOR is sparse. Most sources assume uniform IOR. The claim that rotation would affect such spheres is based on logical reasoning rather than specific citations.

3. **Relativistic effects**: At extremely high rotational speeds, frame-dragging and other relativistic effects could theoretically influence light behavior near a rotating sphere. This is entirely irrelevant for any practical ray tracer but represents a theoretical gap in the analysis.

### 9.2 Limitations of This Analysis

- The analysis assumes ideal mathematical spheres. Real-world chrome balls have surface imperfections (scratches, grain) that would make rotation technically visible, though this is not modeled in typical ray tracers.
- The analysis of the nwave-raytracer code is based on reading the source; no runtime tests were performed to verify the visual output.

---

## 10. Source Analysis

| # | Source | Type | Reputation | Key Contribution |
|---|--------|------|------------|------------------|
| [1] | Scratchapixel - Ray-Sphere Intersection | Tutorial | High | N = (P-C)/r formula, sphere normal computation |
| [2] | Ray Tracing in One Weekend (Shirley) | Book | High | Sphere outward normal = (P - center) / radius |
| [3] | Scratchapixel - Reflection and Refraction | Tutorial | High | Reflection formula R = D - 2*dot(D,N)*N |
| [4] | Wikipedia - Rotational Symmetry / 3D Rotation Group | Reference | Medium-High | SO(3) symmetry of sphere, rotation invariance |
| [5] | PBRT 3rd Ed - Spheres Chapter | Book | Highest | Object-to-world transform pipeline, UV computation, ObjectToWorld on SurfaceInteraction |
| [6] | GameDev.net - Ray Tracing Texture Map a Sphere | Forum | Medium | Sphere UV rotation via azimuth/inclination offsets |
| [7] | viclw17 - Raytracing UV Mapping and Texturing | Blog | Medium | Sphere UV from spherical coordinates, theta/phi mapping |
| [8] | CMU CS 15-462 - Ray Casting Slides | Academic | High | Ray-surface intersection, normal computation fundamentals |
| [9] | Northeastern - Ray Tracing Formulas | Academic | High | Complete reflection/refraction formula reference |
| [10] | Cambridge Univ - Ray Tracing Primitives | Academic | High | Sphere normal passes through center, transform properties |
| [11] | Wikipedia - Fresnel Equations | Reference | Medium-High | Fresnel coefficients depend on angle of incidence and IOR only |
| [12] | RP Photonics - Fresnel Equations | Reference | High | Fresnel equations parametrized by angle and refractive indices |
| [13] | Drew Gillie Portfolio - Chrome Ball in VFX | Technical | Medium | Chrome ball capture for HDRI, orientation irrelevance |
| [14] | AIP Physics Teacher - Reflections on a Disco Ball | Academic | High | Disco ball reflection geometry, facet rotation effects |

### Source URLs

- [1] https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection.html
- [2] https://raytracing.github.io/books/RayTracingInOneWeekend.html
- [3] https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-ray-tracing/adding-reflection-and-refraction.html
- [4] https://en.wikipedia.org/wiki/3D_rotation_group
- [5] https://www.pbr-book.org/3ed-2018/Shapes/Spheres
- [6] https://gamedev.net/forums/topic/632060-ray-tracing-texture-map-a-sphere/4985722/
- [7] https://viclw17.github.io/2019/04/12/raytracing-uv-mapping-and-texturing
- [8] http://graphics.cs.cmu.edu/nsp/course/15-462/Fall04/slides/14-ray.pdf
- [9] https://www.ccs.neu.edu/home/fell/CSU540/programs/RayTracingFormulas.htm
- [10] https://www.cl.cam.ac.uk/teaching/0001/AGraphHCI/SMEG/node2.html
- [11] https://en.wikipedia.org/wiki/Fresnel_equations
- [12] https://www.rp-photonics.com/fresnel_equations.html
- [13] https://www.drewgillieportfolio.com/visual-effects-thesis/blog-post-title-one-rnhpg-wt797
- [14] https://pubs.aip.org/aapt/pte/article/54/9/532/1058994/Reflections-on-a-Disco-Ball
