# Texture Mapping for nwave-raytracer

## Research Summary

| Property | Value |
|---|---|
| Topic | Procedural and image-based texture mapping for ray tracers |
| Date | 2026-02-20 |
| Sources consulted | 14 |
| Sources from trusted domains | 8 |
| Overall confidence | High |

---

## Table of Contents

1. [Procedural Textures vs Image Textures](#1-procedural-textures-vs-image-textures)
2. [Procedural Marble Texture](#2-procedural-marble-texture)
3. [Procedural Wood Texture](#3-procedural-wood-texture)
4. [Image-Based Textures (UV Mapping)](#4-image-based-textures-uv-mapping)
5. [Architecture Integration for nwave-raytracer](#5-architecture-integration-for-nwave-raytracer)
6. [Metal GPU Shader Considerations](#6-metal-gpu-shader-considerations)
7. [Recommended Implementation Path](#7-recommended-implementation-path)
8. [Source Analysis](#8-source-analysis)

---

## 1. Procedural Textures vs Image Textures

**Confidence: High (5 sources)**

There are two fundamental approaches to texturing in ray tracers: procedural textures that compute color mathematically from a 3D point, and image textures that look up color from a 2D image file using UV coordinates.

### 1.1 Procedural Textures (Solid Textures)

Procedural textures evaluate a mathematical function at the 3D world-space (or object-space) hit point to determine color. They require no UV coordinates and no image data. The canonical reference for this approach is Ken Perlin's noise function (1985, improved 2002), which forms the basis for marble, wood, cloud, and other natural material appearances.

**Advantages:**
- No UV coordinate computation needed; every object "just works" regardless of geometry [PBRT-Solid].
- Infinite resolution: procedural evaluation at render time naturally scales detail based on camera proximity [PBRT-Solid].
- Zero texture memory: the function is the texture [GPU-Gems-Ch5].
- No seam artifacts: solid textures have no 2D wrapping boundaries [PBRT-Solid].

**Disadvantages:**
- Harder to art-direct: fine-tuning the appearance of procedural textures requires adjusting mathematical parameters rather than painting pixels [PBRT-Solid].
- Antialiasing is more difficult because there is no precomputed frequency information [PBRT-Solid].
- Limited to patterns that can be described mathematically.

### 1.2 Image-Based Textures

Image textures load a bitmap (PNG, JPG, HDR) and map 2D UV coordinates at the hit point to pixel lookups in the image. Each shape must provide a UV parameterization.

**Advantages:**
- Complete artistic control: any photographic or painted detail can be applied.
- Well-understood filtering and antialiasing (mipmapping, bilinear/trilinear interpolation).
- Industry standard for realistic rendering.

**Disadvantages:**
- Requires UV coordinate generation for every shape type.
- Finite resolution: zooming in reveals pixelation unless mipmaps are used.
- Memory cost proportional to texture resolution.
- Seam and distortion artifacts on curved surfaces (especially at sphere poles).

### 1.3 Recommendation for nwave-raytracer

Start with procedural textures. The codebase already has the `HitRecord.point` (3D position) available at every intersection, and the Metal shader also computes `rec.point`. Procedural textures require no changes to the intersection routines, no UV computation, and no image data transfer to the GPU. Image textures can be added as a second phase.

---

## 2. Procedural Marble Texture

**Confidence: High (5 sources)**

Marble textures are built from three layered components: Perlin noise, turbulence (multi-octave noise summation), and a sinusoidal color mapping that creates the characteristic veining pattern.

### 2.1 Perlin Noise Foundation

Perlin noise is a gradient noise function that returns a smooth, continuous, pseudorandom value for any 3D input point. It has these properties:
- Value is zero at all integer lattice points [PBRT-Noise].
- Nearby points return similar values (spatial coherence) [Shirley-RTTNW].
- Band-limited frequency content (no sharp discontinuities) [PBRT-Noise].

**Algorithm (Improved Perlin Noise, 2002):**

1. **Lattice coordinates**: For input point `p`, compute integer floor `(i, j, k)` and fractional offsets `(u, v, w)`.

2. **Smoothing function**: Apply a quintic (5th-degree) smoothstep to fractional offsets to eliminate second-derivative discontinuities:
   ```
   // Quintic (PBRT/Improved Perlin):
   s(t) = 6t^5 - 15t^4 + 10t^3

   // Or Hermitian cubic (Shirley's simpler version):
   s(t) = 3t^2 - 2t^3
   ```

3. **Permutation table**: A precomputed array of 256 integers (0-255 in random order), duplicated to avoid modulus operations. Three independent permutations (`perm_x`, `perm_y`, `perm_z`) are XOR-combined to select random gradient vectors at each lattice corner:
   ```cpp
   // Shirley's approach:
   randvec[perm_x[(i+di) & 255] ^ perm_y[(j+dj) & 255] ^ perm_z[(k+dk) & 255]]

   // PBRT's approach:
   NoisePerm[NoisePerm[NoisePerm[ix] + iy] + iz]
   ```

4. **Gradient vectors**: At each of the 8 surrounding lattice corners, a random unit vector is stored. The dot product of this gradient with the offset vector from the corner to the input point produces a scalar contribution.

5. **Trilinear interpolation**: The 8 gradient dot products are interpolated using the smoothed fractional offsets:
   ```cpp
   static double perlin_interp(const vec3 c[2][2][2],
                                double u, double v, double w) {
       auto uu = u*u*(3-2*u);
       auto vv = v*v*(3-2*v);
       auto ww = w*w*(3-2*w);
       auto accum = 0.0;
       for (int i = 0; i < 2; i++)
           for (int j = 0; j < 2; j++)
               for (int k = 0; k < 2; k++) {
                   vec3 weight_v(u-i, v-j, w-k);
                   accum += (i*uu + (1-i)*(1-uu))
                          * (j*vv + (1-j)*(1-vv))
                          * (k*ww + (1-k)*(1-ww))
                          * dot(c[i][j][k], weight_v);
               }
       return accum;
   }
   ```

### 2.2 Turbulence (Fractional Brownian Motion)

Turbulence accumulates multiple octaves of noise at progressively higher frequencies and lower amplitudes. This produces natural-looking multi-scale detail.

```cpp
double turb(const point3& p, int depth = 7) const {
    auto accum = 0.0;
    auto temp_p = p;
    auto weight = 1.0;
    for (int i = 0; i < depth; i++) {
        accum += weight * noise(temp_p);
        weight *= 0.5;
        temp_p *= 2;
    }
    return std::fabs(accum);
}
```

Each octave doubles the frequency (halves the feature size) and halves the amplitude. The absolute value produces sharper, crease-like features suitable for veining effects [PBRT-Noise, Shirley-RTTNW].

### 2.3 Marble Pattern: sin(x + turbulence)

The key insight for marble is using the turbulence value to perturb the phase of a sine function. This creates undulating vein-like stripes:

```cpp
color value(double u, double v, const point3& p) const override {
    // The sine of a coordinate, phase-shifted by turbulence, creates veins
    return color(0.5, 0.5, 0.5)
         * (1.0 + sin(scale * p.z() + 10.0 * noise.turb(p, 7)));
}
```

- `scale * p.z()` creates parallel planar stripes along one axis.
- `10.0 * noise.turb(p, 7)` shifts the phase of each stripe by a turbulent amount, creating the organic veining pattern characteristic of marble.
- The multiplicative constant (10.0) controls how "turbulent" the veins are.
- The `scale` parameter controls how closely spaced the base stripes are [Shirley-RTTNW].

### 2.4 Color Mapping

For realistic marble, map the sine output to a color ramp rather than using a simple grayscale:

```cpp
color marble_color(double t) const {
    // t is in [0, 1] from the remapped sine
    // Interpolate between white marble and dark vein color
    color white(0.95, 0.93, 0.90);
    color vein(0.15, 0.12, 0.10);
    return white * t + vein * (1.0 - t);
}
```

PBRT takes this further by using a cubic spline through multiple control colors for richer marble variation [PBRT-Noise].

---

## 3. Procedural Wood Texture

**Confidence: Medium (3 sources)**

Wood grain patterns are created by computing the radial distance from a central axis (simulating tree rings), then perturbing this distance with noise to create natural-looking grain variation.

### 3.1 Cylindrical Ring Pattern

The foundation is concentric rings around a vertical axis:

```cpp
color value(double u, double v, const point3& p) const override {
    // Distance from the Y axis (tree trunk center)
    double dist_from_center = sqrt(p.x() * p.x() + p.z() * p.z());

    // Create ring pattern using sine
    double ring_value = sin(ring_frequency * dist_from_center
                          + turb_power * noise.turb(p * turb_scale, 7));

    // Remap from [-1, 1] to [0, 1]
    double t = 0.5 * (1.0 + ring_value);

    // Interpolate between light and dark wood colors
    color light_wood(0.76, 0.60, 0.42);
    color dark_wood(0.40, 0.26, 0.13);
    return light_wood * t + dark_wood * (1.0 - t);
}
```

### 3.2 Parameters

- `ring_frequency`: Controls the number of visible rings (typically 5-20). Higher values produce more tightly packed rings.
- `turb_power`: Controls how much noise perturbs the rings (typically 0.05-0.15). Low values preserve visible ring structure; high values create more chaotic grain [Lodev-Noise].
- `turb_scale`: Scales the input point before computing turbulence, controlling the spatial scale of the grain variation.

### 3.3 Grain Variation Enhancements

For more realistic wood:
- Add a secondary noise component along the Y-axis to simulate longitudinal grain variation.
- Use different noise frequencies for radial vs. longitudinal perturbation.
- Apply a slight rotation to the noise coordinates to break symmetry.

---

## 4. Image-Based Textures (UV Mapping)

**Confidence: High (5 sources)**

### 4.1 UV Coordinate Formulas by Shape

UV coordinates map each point on a 3D surface to a 2D (u, v) pair in [0, 1] x [0, 1].

**Sphere** (spherical mapping):
```cpp
// p is the unit-length vector from center to hit point: (hit - center) / radius
void get_sphere_uv(const vec3& p, double& u, double& v) {
    double phi = atan2(p.z(), p.x());       // azimuthal angle [-pi, pi]
    double theta = asin(clamp(p.y(), -1, 1)); // polar angle [-pi/2, pi/2]
    u = 1.0 - (phi + M_PI) / (2.0 * M_PI);  // [0, 1]
    v = (theta + M_PI / 2.0) / M_PI;          // [0, 1]
}
```
Sources: [Shirley-RTTNW], [1000Bunnies-UV], [PBRT-TexCoord]

**Plane** (planar mapping):
```cpp
// Project hit point onto two axes of the plane
void get_plane_uv(const point3& hit, double scale, double& u, double& v) {
    // For a Y-normal plane (floor):
    u = hit.x() * scale - floor(hit.x() * scale);
    v = hit.z() * scale - floor(hit.z() * scale);
}
```

**Box** (per-face planar mapping):
```cpp
// Determine which face was hit based on the normal direction,
// then project the remaining two coordinates to [0, 1]
void get_box_uv(const point3& hit, const vec3& normal,
                const point3& bmin, const point3& bmax,
                double& u, double& v) {
    vec3 size = bmax - bmin;
    if (abs(normal.x()) > 0.9) {       // X-face
        u = (hit.z() - bmin.z()) / size.z();
        v = (hit.y() - bmin.y()) / size.y();
    } else if (abs(normal.y()) > 0.9) { // Y-face
        u = (hit.x() - bmin.x()) / size.x();
        v = (hit.z() - bmin.z()) / size.z();
    } else {                             // Z-face
        u = (hit.x() - bmin.x()) / size.x();
        v = (hit.y() - bmin.y()) / size.y();
    }
}
```

**Cylinder** (cylindrical mapping):
```cpp
// theta around the Y axis, height along Y
void get_cylinder_uv(const point3& hit, const point3& center,
                     double radius, double height,
                     double& u, double& v) {
    double dx = hit.x() - center.x();
    double dz = hit.z() - center.z();
    double theta = atan2(dz, dx);
    u = 1.0 - (theta + M_PI) / (2.0 * M_PI);
    v = (hit.y() - center.y()) / height;
}
```

### 4.2 Texture Sampling with Bilinear Interpolation

Point sampling (nearest-neighbor) produces blocky artifacts. Bilinear interpolation smooths between the four surrounding texels:

```cpp
color sample_bilinear(const unsigned char* data, int width, int height,
                      double u, double v) {
    double fx = u * (width - 1);
    double fy = (1.0 - v) * (height - 1);  // flip V
    int x0 = (int)floor(fx), y0 = (int)floor(fy);
    int x1 = min(x0 + 1, width - 1), y1 = min(y0 + 1, height - 1);
    double sx = fx - x0, sy = fy - y0;

    auto px = [&](int x, int y) -> color {
        int idx = 3 * (y * width + x);
        return color(data[idx]/255.0, data[idx+1]/255.0, data[idx+2]/255.0);
    };

    color c00 = px(x0, y0), c10 = px(x1, y0);
    color c01 = px(x0, y1), c11 = px(x1, y1);
    color top = c00 * (1-sx) + c10 * sx;
    color bot = c01 * (1-sx) + c11 * sx;
    return top * (1-sy) + bot * sy;
}
```
Sources: [Scratchapixel-Bilinear], [Demofox-Bilinear]

### 4.3 Image Loading with stb_image

The `stb_image.h` single-header library is the standard choice for loading texture images in ray tracers [stb-github, Shirley-RTTNW]:

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class image_texture : public texture {
public:
    image_texture(const std::string& filename) {
        data_ = stbi_load(filename.c_str(), &width_, &height_, &channels_, 3);
        if (!data_) {
            // fallback: return magenta for missing textures
            width_ = height_ = 1;
            channels_ = 3;
            fallback_ = {255, 0, 255};
            data_ = fallback_.data();
        }
    }

    ~image_texture() { if (data_ != fallback_.data()) stbi_image_free(data_); }

    color value(double u, double v, const point3& p) const override {
        return sample_bilinear(data_, width_, height_, u, v);
    }
private:
    unsigned char* data_ = nullptr;
    int width_ = 0, height_ = 0, channels_ = 0;
    std::vector<unsigned char> fallback_;
};
```

---

## 5. Architecture Integration for nwave-raytracer

**Confidence: High (4 sources)**

### 5.1 Current Architecture Analysis

The nwave-raytracer currently has:

| Component | Current State | Texture Relevance |
|---|---|---|
| `Material` base class | Pure virtual `scatter()` returning `Color3 attenuation` | Attenuation is the "color" -- this becomes the texture evaluation point |
| `Lambertian` | Stores `Color3 albedo_` as a constant | Replace with `Texture*` pointer |
| `Metal` | Stores `Color3 albedo_` as a constant | Replace with `Texture*` pointer |
| `HitRecord` | Already has `double u` and `double v` fields | Ready for UV-based textures (currently unused) |
| `GPUMaterial` struct | Stores `float albedo[3]` as a constant | Must be extended for texture type identification |
| Metal shader | Reads `mat.albedo` as constant `float3` | Must add procedural texture evaluation functions |

**Critical finding**: `HitRecord` already has `u` and `v` fields (lines 16-17 of `hit_record.h`). This means the infrastructure for UV-based textures is partially in place, though no shape currently populates these fields.

### 5.2 The Texture Abstraction Pattern

Both Shirley and PBRT use the same core pattern: a `Texture` class that returns a color given a point (and optionally UV coordinates).

**Shirley's approach** (simpler, recommended as starting point):
```cpp
class Texture {
public:
    virtual ~Texture() = default;
    virtual Color3 value(double u, double v, const Point3& p) const = 0;
};
```

**PBRT's approach** (more general):
```cpp
template <typename T>
class Texture {
public:
    virtual T Evaluate(const SurfaceInteraction&) const = 0;
};
// Used as Texture<float> for scalar params, Texture<Spectrum> for color
```

For nwave-raytracer, the Shirley approach is recommended. It is simpler, sufficient for marble/wood/image textures, and does not require template machinery.

### 5.3 Proposed Class Hierarchy

```
Texture (abstract)
  |-- SolidColor         : returns constant Color3 (backward-compatible)
  |-- MarbleTexture      : sin(scale * p.z + turbulence)
  |-- WoodTexture        : ring pattern + turbulence
  |-- CheckerTexture     : alternating two textures based on position
  |-- ImageTexture       : UV-mapped image lookup (Phase 2)
```

### 5.4 Material Refactoring

The `Lambertian` class changes from:
```cpp
// BEFORE: constant albedo
class Lambertian : public Material {
    Color3 albedo_;
};
```

To:
```cpp
// AFTER: texture-based albedo
class Lambertian : public Material {
    std::shared_ptr<Texture> albedo_texture_;
public:
    // Backward-compatible: constant color wraps in SolidColor
    Lambertian(const Color3& albedo)
        : albedo_texture_(std::make_shared<SolidColor>(albedo)) {}

    // New: accept any texture
    Lambertian(std::shared_ptr<Texture> tex)
        : albedo_texture_(std::move(tex)) {}

    bool scatter(const Ray& ray_in, const HitRecord& rec,
                 Color3& attenuation, Ray& scattered) const override {
        // ...same scatter logic...
        attenuation = albedo_texture_->value(rec.u, rec.v, rec.point);
        return true;
    }
};
```

This pattern preserves full backward compatibility: existing scenes that use `Lambertian(Color3(0.8, 0.3, 0.1))` continue to work unchanged because the `Color3` constructor wraps in `SolidColor`.

The same refactoring applies to `Metal` (texture replaces `albedo_`).

### 5.5 Perlin Noise Class (CPU Side)

```cpp
class Perlin {
public:
    Perlin();

    double noise(const Point3& p) const;
    double turbulence(const Point3& p, int depth = 7) const;

private:
    static constexpr int POINT_COUNT = 256;
    Vec3 random_vectors_[POINT_COUNT];
    int perm_x_[POINT_COUNT];
    int perm_y_[POINT_COUNT];
    int perm_z_[POINT_COUNT];

    static void generate_perm(int* p);
    static double interp(const Vec3 c[2][2][2], double u, double v, double w);
};
```

### 5.6 YAML Scene Loader Extension

```yaml
materials:
  - name: marble_floor
    type: lambertian
    texture:
      type: marble
      scale: 4.0
      vein_color: [0.15, 0.12, 0.10]
      base_color: [0.95, 0.93, 0.90]
      turbulence_depth: 7

  - name: wooden_table
    type: lambertian
    texture:
      type: wood
      ring_frequency: 12.0
      turbulence_power: 0.1
      light_color: [0.76, 0.60, 0.42]
      dark_color: [0.40, 0.26, 0.13]

  - name: earth_sphere
    type: lambertian
    texture:
      type: image
      file: textures/earth.jpg

  - name: red_paint
    type: lambertian
    albedo: [0.8, 0.1, 0.1]  # backward-compatible: no texture block
```

The loader detects whether a material has a `texture` block or a plain `albedo`. When `albedo` is present, it wraps in `SolidColor`. When `texture` is present, it instantiates the appropriate texture class.

---

## 6. Metal GPU Shader Considerations

**Confidence: High (4 sources)**

### 6.1 Procedural Textures in the Metal Shader (Recommended First Step)

Procedural textures are ideal for GPU because they require **no data transfer**. The noise function is implemented entirely within the shader. The only change needed to `GPUMaterial` is a texture type tag and a few parameters.

**Extended GPUMaterial struct:**
```cpp
// C++ side (gpu_types.h)
struct alignas(16) GPUMaterial {
    uint32_t material_type;    //  0
    uint32_t texture_type;     //  4: 0=solid, 1=marble, 2=wood, 3=checker
    float    _pad1;            //  8
    float    _pad2;            // 12
    float    albedo[3];        // 16: base color (or solid color)
    float    param1;           // 28: fuzz/ior/scale
    float    tint[3];          // 32: secondary color (vein color, dark wood, etc.)
    float    param2;           // 44: turbulence_depth or ring_frequency
};                             // 48 bytes (unchanged size)
```

The `_pad0` field at offset 4 is repurposed as `texture_type`, and `_pad3` at offset 44 becomes `param2`. This keeps the struct at 48 bytes, maintaining Metal buffer compatibility with no layout changes.

**Metal shader Perlin noise (inlined in shader):**

The permutation table can be embedded as a `constant` array within the shader file. Metal Shading Language supports `constant` address space arrays and `thread` local variables, which are sufficient for Perlin noise evaluation.

```metal
// Permutation table (256 entries, Ken Perlin's standard permutation)
constant int perm[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    // ... full 256 entries, repeated once for wraparound ...
};

float3 noise_grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 13 ? x : z);
    return float3(
        ((h & 1) ? -u : u) + ((h & 2) ? -v : v),
        0.0, 0.0  // simplified; full gradient for 3D
    );
}

float perlin_noise(float3 p) {
    int xi = int(floor(p.x)) & 255;
    int yi = int(floor(p.y)) & 255;
    int zi = int(floor(p.z)) & 255;
    float xf = p.x - floor(p.x);
    float yf = p.y - floor(p.y);
    float zf = p.z - floor(p.z);

    // Quintic smoothstep
    float u = xf * xf * xf * (xf * (xf * 6.0 - 15.0) + 10.0);
    float v = yf * yf * yf * (yf * (yf * 6.0 - 15.0) + 10.0);
    float w = zf * zf * zf * (zf * (zf * 6.0 - 15.0) + 10.0);

    // Hash corners
    int aaa = perm[perm[perm[xi]+yi]+zi];
    int aba = perm[perm[perm[xi]+yi+1]+zi];
    int aab = perm[perm[perm[xi]+yi]+zi+1];
    int abb = perm[perm[perm[xi]+yi+1]+zi+1];
    int baa = perm[perm[perm[xi+1]+yi]+zi];
    int bba = perm[perm[perm[xi+1]+yi+1]+zi];
    int bab = perm[perm[perm[xi+1]+yi]+zi+1];
    int bbb = perm[perm[perm[xi+1]+yi+1]+zi+1];

    // Gradient dot products and trilinear interpolation
    float x1 = mix(grad(aaa, xf, yf, zf), grad(baa, xf-1, yf, zf), u);
    float x2 = mix(grad(aba, xf, yf-1, zf), grad(bba, xf-1, yf-1, zf), u);
    float y1 = mix(x1, x2, v);

    float x3 = mix(grad(aab, xf, yf, zf-1), grad(bab, xf-1, yf, zf-1), u);
    float x4 = mix(grad(abb, xf, yf-1, zf-1), grad(bbb, xf-1, yf-1, zf-1), u);
    float y2 = mix(x3, x4, v);

    return mix(y1, y2, w);
}

float turbulence(float3 p, int depth) {
    float accum = 0.0;
    float weight = 1.0;
    float3 temp_p = p;
    for (int i = 0; i < depth; i++) {
        accum += weight * perlin_noise(temp_p);
        weight *= 0.5;
        temp_p *= 2.0;
    }
    return abs(accum);
}
```

**Marble texture evaluation in shader:**
```metal
float3 evaluate_marble(float3 hit_point, float3 base_color,
                       float3 vein_color, float scale) {
    float t = 0.5 * (1.0 + sin(scale * hit_point.z
                               + 10.0 * turbulence(hit_point, 7)));
    return mix(vein_color, base_color, t);
}
```

### 6.2 Image Textures on the GPU (Phase 2)

For image-based textures, Metal provides native texture objects with hardware-accelerated sampling:

**CPU side (Objective-C++, in metal_render_backend.mm):**
```objc
// Create texture descriptor
MTLTextureDescriptor* desc =
    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                      width:width
                                                     height:height
                                                  mipmapped:NO];
id<MTLTexture> texture = [device newTextureWithDescriptor:desc];

MTLRegion region = MTLRegionMake2D(0, 0, width, height);
[texture replaceRegion:region mipmapLevel:0
             withBytes:imageData bytesPerRow:4 * width];

// Bind to compute encoder
[encoder setTexture:texture atIndex:0];
```

**Shader side:**
```metal
kernel void ray_trace_kernel(
    // ... existing buffers ...
    texture2d<float, access::read> image_texture [[texture(0)]],
    // ...
) {
    // Sample with built-in bilinear filtering
    constexpr sampler s(coord::normalized, address::repeat, filter::linear);
    float4 tex_color = image_texture.sample(s, float2(rec_u, rec_v));
}
```

**Challenges:**
- The current shader has no UV coordinates in `HitRecord`. The Metal `HitRecord` struct needs `float u` and `float v` fields added, and each intersection function must compute and return them.
- Multiple textures require a texture array or multiple texture bindings, adding complexity to the command encoder setup.
- Texture indices must be stored in the `GPUMaterial` struct to identify which texture each material uses.

### 6.3 GPU Strategy Summary

| Approach | Data Transfer | Shader Changes | Complexity |
|---|---|---|---|
| Procedural (noise-based) | None (parameters in GPUMaterial) | Add noise functions + texture evaluation | Low-Medium |
| Image (MTLTexture) | Image data upload per texture | Add UV to HitRecord + texture sampling | Medium-High |

---

## 7. Recommended Implementation Path

### Phase 1: Procedural Textures (CPU + GPU)

**Step 1: Core Texture Abstraction (CPU)**
1. Create `src/domain/textures/texture.h` with the `Texture` base class.
2. Create `SolidColor` texture that wraps a constant `Color3`.
3. Refactor `Lambertian` and `Metal` to store `shared_ptr<Texture>` instead of `Color3 albedo_`. Preserve backward-compatible constructors that accept `Color3`.
4. Update `scatter()` to call `texture_->value(rec.u, rec.v, rec.point)`.

**Step 2: Perlin Noise (CPU)**
1. Create `src/domain/textures/perlin.h` and `perlin.cpp` with the `Perlin` class.
2. Implement `noise()` with permutation tables and trilinear interpolation.
3. Implement `turbulence()` with 7-octave summation.

**Step 3: Marble and Wood Textures (CPU)**
1. Create `MarbleTexture` using `sin(scale * p.z + turb_power * perlin.turb(p))`.
2. Create `WoodTexture` using `sin(ring_freq * sqrt(x^2 + z^2) + turb * perlin.turb(p))`.
3. Add color ramp support for both.

**Step 4: YAML Loader Extension**
1. Extend `yaml_scene_loader.cpp` to parse `texture:` blocks.
2. Support `type: marble`, `type: wood`, `type: checker`, and fallback to `albedo:`.

**Step 5: GPU Procedural Textures**
1. Add `texture_type` and `param2` fields to `GPUMaterial` (reuse padding bytes).
2. Implement `perlin_noise()` and `turbulence()` functions in `ray_trace.metal`.
3. Add texture evaluation dispatch in the shader's material handling section.
4. Update `scene_flattener.cpp` to serialize texture parameters.

### Phase 2: Image Textures (CPU + GPU)

**Step 6: UV Coordinate Generation**
1. Add UV computation to each shape's `hit()` method, populating `rec.u` and `rec.v`.
2. Start with sphere (spherical mapping) and plane (planar mapping).
3. Extend to box (per-face) and cylinder (cylindrical).

**Step 7: Image Texture Class (CPU)**
1. Add `stb_image.h` as a dependency.
2. Create `ImageTexture` class with bilinear sampling.
3. Extend YAML loader to parse `type: image` with `file:` path.

**Step 8: Image Textures on GPU**
1. Add `float u, v` to Metal shader's `HitRecord` struct.
2. Compute UV in each `intersect_*` function.
3. Create `MTLTexture` objects from loaded images.
4. Bind textures to compute encoder and sample in shader.

### Estimated Effort

| Phase | Step | Description | Effort |
|---|---|---|---|
| 1 | 1 | Texture abstraction + SolidColor | Small |
| 1 | 2 | Perlin noise class | Medium |
| 1 | 3 | Marble + Wood textures | Small |
| 1 | 4 | YAML loader extension | Small |
| 1 | 5 | GPU procedural textures | Medium |
| 2 | 6 | UV coordinate generation | Medium |
| 2 | 7 | Image texture class (CPU) | Small |
| 2 | 8 | Image textures on GPU | Medium-Large |

---

## 8. Source Analysis

| # | Source | Type | Independence | Used For |
|---|---|---|---|---|
| 1 | [Ray Tracing: The Next Week (Shirley)](https://raytracing.github.io/books/RayTracingTheNextWeek.html) | Book (free online) | Primary | Texture class design, Perlin noise, marble pattern, material integration |
| 2 | [PBRT: Texture Interface and Basic Textures](https://www.pbr-book.org/3ed-2018/Texture/Texture_Interface_and_Basic_Textures) | Book (free online) | Primary | Template-based Texture<T> design, ConstantTexture, ScaleTexture |
| 3 | [PBRT: Solid and Procedural Texturing](https://www.pbr-book.org/3ed-2018/Texture/Solid_and_Procedural_Texturing) | Book (free online) | Primary | 3D vs 2D textures, advantages/disadvantages comparison |
| 4 | [PBRT: Noise](https://www.pbr-book.org/3ed-2018/Texture/Noise) | Book (free online) | Primary | Improved Perlin noise, fBm, turbulence, marble/wood applications |
| 5 | [GPU Gems: Implementing Improved Perlin Noise (Ch. 5)](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-5-implementing-improved-perlin-noise) | NVIDIA technical | Independent | GPU noise implementation, gradient optimization, 16-gradient trick |
| 6 | [GPU Gems 2: Implementing Improved Perlin Noise (Ch. 26)](https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-26-implementing-improved-perlin-noise) | NVIDIA technical | Independent | Permutation table optimization for GPU, texture-based lookups |
| 7 | [Lodev.org: Texture Generation using Random Noise](https://lodev.org/cgtutor/randomnoise.html) | Tutorial | Independent | Marble sine pattern, wood ring pattern, turbulence octaves, color mapping |
| 8 | [UV Mapping and Texturing (1000 Forms of Bunnies)](https://viclw17.github.io/2019/04/12/raytracing-uv-mapping-and-texturing) | Blog/tutorial | Independent | Sphere UV formulas, stb_image integration, image_texture implementation |
| 9 | [Metal by Example: Textures and Samplers](https://metalbyexample.com/textures-and-samplers/) | Tutorial | Independent | MTLTexture creation, sampler syntax, texture binding in Metal |
| 10 | [Apple: Processing a Texture in a Compute Function](https://developer.apple.com/documentation/metal/processing_a_texture_in_a_compute_function) | Official docs | Independent | Metal compute kernel texture binding, access specifiers |
| 11 | [Scratchapixel: Bilinear Interpolation](https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/interpolation/bilinear-filtering.html) | Tutorial | Independent | Bilinear interpolation algorithm for texture sampling |
| 12 | [PBRT: Texture Coordinate Generation](https://pbr-book.org/3ed-2018/Texture/Texture_Coordinate_Generation) | Book (free online) | Primary (same as #2-4) | UV generation strategies for different shapes |
| 13 | [stb_image.h (nothings/stb)](https://github.com/nothings/stb/blob/master/stb_image.h) | Library source | Independent | Image loading API reference |
| 14 | [Book of Shaders: Noise](https://thebookofshaders.com/11/) | Tutorial | Independent | Visual explanation of noise functions for GPU |

### Cross-Reference Verification

| Claim | Sources Confirming | Confidence |
|---|---|---|
| Perlin noise uses permutation tables + gradient interpolation | #1, #4, #5, #6 | High |
| Marble = sin(coordinate + turbulence) | #1, #4, #7 | High |
| Texture class: virtual value(u, v, point) | #1, #2 | High |
| Materials should store Texture* instead of constant color | #1, #2 | High |
| HitRecord needs u,v for image textures | #1, #8, #12 | High |
| GPU Perlin: permutation table as constant array | #5, #6, #14 | High |
| Metal texture2d with sampler for image textures | #9, #10 | High |
| Wood uses cylindrical distance + noise perturbation | #4, #7 | Medium |
| stb_image for loading PNG/JPG textures | #1, #8, #13 | High |

---

## Knowledge Gaps

| Gap | What was searched | Why insufficient |
|---|---|---|
| Metal-specific Perlin noise MSL code | Searched for Metal shader Perlin implementations | Found references to existence (MetalNoise GitHub repo, Andy Stanton blog) but no published complete MSL code. The GPU Gems GLSL/HLSL examples can be transliterated to MSL with minor syntax changes. |
| Antialiasing procedural textures on GPU | Searched for procedural texture antialiasing ray tracer | PBRT discusses clamping octaves based on ray differentials, but no concrete GPU ray tracer implementation found. For this project, the multi-sample-per-pixel approach already provides stochastic antialiasing. |
| Performance benchmarks: procedural vs image texture on Metal | Searched for Metal compute shader texture performance comparison | No benchmarks found for ray tracing specifically. GPU Gems notes procedural noise is "fast enough" for real-time rendering, which suggests it is more than adequate for offline ray tracing. |

---

## Interpretation (Analysis)

*The following are interpretations based on the evidence, not directly sourced claims.*

1. **Phase 1 is architecturally self-contained.** The procedural texture path requires changes only to the material system (Texture abstraction, Perlin class) and the GPU material struct/shader. It does not touch the shape intersection code, the BVH, the scene flattener's shape serialization, or the render loop structure. This makes it a low-risk addition.

2. **The GPUMaterial padding bytes are a fortunate design choice.** The current 48-byte struct has several `_pad` fields that can be repurposed for texture parameters without changing the struct size or breaking the Metal buffer layout. Specifically, `_pad0` (offset 4) becomes `texture_type`, and `_pad3` (offset 44) becomes `param2`.

3. **The existing `u, v` fields in `HitRecord` suggest texture support was anticipated.** These fields exist but are never populated. This is a common pattern from Shirley's books where UV fields are added to the hit record early, before the texture chapter.

4. **Wood texture has fewer authoritative sources than marble.** The marble pattern (sin + turbulence) is universally described across all ray tracing references. Wood texture is mentioned less frequently and with more variation in approach, suggesting it is more of a creative choice than a standardized algorithm.
