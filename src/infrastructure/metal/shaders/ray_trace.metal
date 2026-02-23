#include <metal_stdlib>
using namespace metal;

// ---------------------------------------------------------------------------
// GPU struct definitions -- must match C++ gpu_types.h layout exactly
// ---------------------------------------------------------------------------

struct GPUCamera {
    packed_float3 lookfrom;        //  0
    float  _pad0;                  // 12
    packed_float3 pixel00_loc;     // 16
    float  _pad1;                  // 28
    packed_float3 pixel_delta_u;   // 32
    float  _pad2;                  // 44
    packed_float3 pixel_delta_v;   // 48
    float  _pad3;                  // 60
    packed_float3 background_top;  // 64
    float  _pad4;                  // 76
    packed_float3 background_bottom; // 80
    float  _pad5;                  // 92
    uint   image_width;            // 96
    uint   image_height;           //100
    uint   samples_per_pixel;      //104
    uint   max_depth;              //108
    float  ambient_factor;         //112
    uint   batch_index;            //116
    uint   num_batches;            //120
    float  _pad6;                  //124
};

// GPUShape: 128 bytes
struct GPUShape {
    uint   shape_type;           //  0
    uint   material_index;       //  4
    uint   has_transform;        //  8
    float  _pad0;                // 12
    float  params[12];           // 16  (48 bytes)
    float4x4 inverse_transform;  // 64  (64 bytes)
};                               //128 total

// GPUMaterial: 48 bytes
struct GPUMaterial {
    uint   material_type;   //  0
    int    texture_offset;  //  4: -1 = no texture, >= 0 = byte offset into texture data
    float  texture_width;   //  8
    float  texture_height;  // 12
    packed_float3 albedo;   // 16
    float  param1;          // 28
    packed_float3 tint;     // 32
    float  texture_scale;   // 44: UV scale (0 = default/full coverage)
};                          // 48 total

// GPULight: 64 bytes
struct GPULight {
    uint   light_type;      //  0
    float  _pad0;           //  4
    float  _pad1;           //  8
    float  _pad2;           // 12
    packed_float3 position; // 16
    float  intensity;       // 28
    packed_float3 color;    // 32
    float  _pad3;           // 44
    float4 _reserved;       // 48
};                          // 64 total

// Shape type tags
constant uint SHAPE_SPHERE   = 0;
constant uint SHAPE_PLANE    = 1;
constant uint SHAPE_BOX      = 2;
constant uint SHAPE_CYLINDER = 3;
constant uint SHAPE_TRIANGLE = 4;

// Light type tags
constant uint LIGHT_POINT       = 0;
constant uint LIGHT_DIRECTIONAL = 1;

// Material type tags
constant uint MAT_LAMBERTIAN    = 0;
constant uint MAT_METAL         = 1;
constant uint MAT_DIELECTRIC    = 2;
constant uint MAT_EMISSIVE      = 3;
constant uint MAT_CHECKER_METAL = 4;

// ---------------------------------------------------------------------------
// Ray struct
// ---------------------------------------------------------------------------
struct Ray {
    float3 origin;
    float3 direction;
};

// Hit record -- includes front_face for dielectric refraction
struct HitRecord {
    float  t;
    float3 point;
    float3 normal;       // always faces against the ray
    bool   front_face;   // true if ray is outside the surface
    uint   material_index;
    uint   shape_type;   // which primitive type was hit
    float  u;            // texture coordinate U
    float  v;            // texture coordinate V
};

// ---------------------------------------------------------------------------
// PCG random number generation
// ---------------------------------------------------------------------------
uint pcg_hash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random_float(thread uint& seed) {
    seed = pcg_hash(seed);
    return float(seed) / float(0xFFFFFFFFu);
}

float3 random_in_unit_sphere(thread uint& seed) {
    // Rejection sampling for a point inside unit sphere
    for (int i = 0; i < 32; ++i) {
        float x = 2.0f * random_float(seed) - 1.0f;
        float y = 2.0f * random_float(seed) - 1.0f;
        float z = 2.0f * random_float(seed) - 1.0f;
        float3 p = float3(x, y, z);
        if (dot(p, p) < 1.0f) {
            return p;
        }
    }
    // Fallback: return a normalized random direction
    float3 p = float3(2.0f * random_float(seed) - 1.0f,
                       2.0f * random_float(seed) - 1.0f,
                       2.0f * random_float(seed) - 1.0f);
    return normalize(p) * 0.5f;
}

// ---------------------------------------------------------------------------
// Reflection and refraction helpers
// ---------------------------------------------------------------------------

float3 reflect_vec(float3 v, float3 n) {
    return v - 2.0f * dot(v, n) * n;
}

float3 refract_vec(float3 uv, float3 n, float etai_over_etat) {
    float cos_theta = min(dot(-uv, n), 1.0f);
    float3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    float k = 1.0f - dot(r_out_perp, r_out_perp);
    // Guard against negative sqrt due to floating point
    float r_out_parallel_len = sqrt(max(k, 0.0f));
    float3 r_out_parallel = -r_out_parallel_len * n;
    return r_out_perp + r_out_parallel;
}

float schlick_reflectance(float cosine, float ref_idx) {
    float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

// ---------------------------------------------------------------------------
// Sphere UV computation from outward normal
// ---------------------------------------------------------------------------
void sphere_uv(float3 outward_normal, thread float& u, thread float& v) {
    float theta = acos(-outward_normal.y);
    float phi = atan2(-outward_normal.z, outward_normal.x) + M_PI_F;
    u = phi / (2.0f * M_PI_F);
    v = 1.0f - theta / M_PI_F;
}

// ---------------------------------------------------------------------------
// Texture sampling from raw RGBA byte buffer
// ---------------------------------------------------------------------------
float3 texel_at(constant uchar* texture_data, int texture_offset,
                int w, int px, int py) {
    int idx = texture_offset + (py * w + px) * 4;
    return float3(float(texture_data[idx])     / 255.0f,
                  float(texture_data[idx + 1]) / 255.0f,
                  float(texture_data[idx + 2]) / 255.0f);
}

float3 sample_texture(constant uchar* texture_data, int texture_offset,
                      float tex_width, float tex_height, float u, float v) {
    int w = int(tex_width);
    int h = int(tex_height);

    // Wrap U to [0,1), clamp V to [0,1)
    u = u - floor(u);
    v = clamp(v, 0.0f, 1.0f);

    // Pole-aware averaging: near the poles (v close to 0 or 1), the atan2
    // singularity produces unstable u values. Average across the row to
    // produce a smooth, stable color at the poles.
    float pole_epsilon = 0.02f;
    if (v < pole_epsilon || v > (1.0f - pole_epsilon)) {
        // Determine the row(s) near the pole
        float fy_pole = v * float(h) - 0.5f;
        int y_pole = clamp(int(floor(fy_pole + 0.5f)), 0, h - 1);
        // Average across the full row (every 4th texel for performance)
        float3 row_avg = float3(0.0f);
        int step = max(1, w / 64);  // sample up to 64 points across the row
        int count = 0;
        for (int px = 0; px < w; px += step) {
            row_avg += texel_at(texture_data, texture_offset, w, px, y_pole);
            count++;
        }
        return row_avg / float(count);
    }

    // Continuous texel coordinates (half-texel offset for center-of-pixel)
    float fx = u * float(w) - 0.5f;
    float fy = v * float(h) - 0.5f;

    int x0 = int(floor(fx));
    int y0 = int(floor(fy));
    float frac_x = fx - floor(fx);
    float frac_y = fy - floor(fy);

    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // U wraps around (seamless horizontal tiling for equirectangular maps)
    x0 = ((x0 % w) + w) % w;
    x1 = ((x1 % w) + w) % w;

    // V clamps at poles (no vertical wrap)
    y0 = clamp(y0, 0, h - 1);
    y1 = clamp(y1, 0, h - 1);

    // Bilinear interpolation of 4 surrounding texels
    float3 c00 = texel_at(texture_data, texture_offset, w, x0, y0);
    float3 c10 = texel_at(texture_data, texture_offset, w, x1, y0);
    float3 c01 = texel_at(texture_data, texture_offset, w, x0, y1);
    float3 c11 = texel_at(texture_data, texture_offset, w, x1, y1);

    float3 top    = mix(c00, c10, frac_x);
    float3 bottom = mix(c01, c11, frac_x);
    return mix(top, bottom, frac_y);
}

// Clamped texture sampling for triplanar use: clamps UV instead of wrapping,
// avoiding the 1px seam that occurs when u=1.0 wraps to u=0.0 at mirror boundaries.
float3 sample_texture_clamped(constant uchar* texture_data, int texture_offset,
                              float tex_width, float tex_height, float u, float v) {
    int w = int(tex_width);
    int h = int(tex_height);
    u = clamp(u, 0.0f, 1.0f - 1.0f / float(w));
    v = clamp(v, 0.0f, 1.0f - 1.0f / float(h));
    float fx = u * float(w) - 0.5f;
    float fy = v * float(h) - 0.5f;
    int x0 = int(floor(fx));
    int y0 = int(floor(fy));
    float frac_x = fx - floor(fx);
    float frac_y = fy - floor(fy);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    x0 = clamp(x0, 0, w - 1);
    x1 = clamp(x1, 0, w - 1);
    y0 = clamp(y0, 0, h - 1);
    y1 = clamp(y1, 0, h - 1);
    float3 c00 = texel_at(texture_data, texture_offset, w, x0, y0);
    float3 c10 = texel_at(texture_data, texture_offset, w, x1, y0);
    float3 c01 = texel_at(texture_data, texture_offset, w, x0, y1);
    float3 c11 = texel_at(texture_data, texture_offset, w, x1, y1);
    float3 top    = mix(c00, c10, frac_x);
    float3 bottom = mix(c01, c11, frac_x);
    return mix(top, bottom, frac_y);
}

// Mirror-repeat: maps any float to [0,1] with seamless mirroring at tile boundaries.
// Unlike fract(), the texture flips at each boundary so edges always match.
float mirror_repeat(float t) {
    t = fract(t * 0.5f) * 2.0f;         // map to [0, 2)
    return 1.0f - abs(t - 1.0f);         // mirror: 0→0, 0.5→0.5, 1→1, 1.5→0.5
}

// Triplanar texture mapping: projects texture from 3 orthogonal axes and blends
// based on the surface normal. Uses mirror-repeat for seamless tiling of any texture.
float3 sample_texture_triplanar(constant uchar* texture_data, int texture_offset,
                                float tex_width, float tex_height,
                                float3 point, float3 normal, float tile_scale) {
    // Blending weights from absolute normal components (sharper = less blending)
    float3 blend = abs(normal);
    // Sharpen the blending to reduce muddy transitions (power 4)
    blend = blend * blend;
    blend = blend * blend;  // power 4 for sharper transitions
    // Normalize so weights sum to 1
    blend = blend / (blend.x + blend.y + blend.z);

    // Scale world-space coordinates for tiling
    float3 p = point * tile_scale;

    // Project from each axis using mirror-repeat (seamless for any texture)
    // Use sample_texture_clamped to avoid wrap-around seam at mirror boundaries
    float3 x_proj = sample_texture_clamped(texture_data, texture_offset,
                                   tex_width, tex_height,
                                   mirror_repeat(p.y),
                                   mirror_repeat(p.z));
    float3 y_proj = sample_texture_clamped(texture_data, texture_offset,
                                   tex_width, tex_height,
                                   mirror_repeat(p.x),
                                   mirror_repeat(p.z));
    float3 z_proj = sample_texture_clamped(texture_data, texture_offset,
                                   tex_width, tex_height,
                                   mirror_repeat(p.x),
                                   mirror_repeat(p.y));

    return x_proj * blend.x + y_proj * blend.y + z_proj * blend.z;
}

// Dual-hemisphere blending for seamless sphere texture mapping:
// Maps the texture from two projections rotated 90 degrees apart around the Y axis,
// then blends by distance from each projection's seam. This eliminates the hard seam
// line at the atan2 wraparound that appears with standard spherical UV mapping.
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

// Cube map texture sampling for spheres: projects texture from 6 cube faces,
// eliminating polar distortion and atan2 seam artifacts entirely.
// Uses coordinate warping at face edges to prevent sampling discontinuities.
float3 sample_sphere_cube_map(constant uchar* texture_data, int texture_offset,
                               float tex_width, float tex_height, float3 n) {
    float3 abs_n = abs(n);

    float face_u, face_v;
    if (abs_n.x >= abs_n.y && abs_n.x >= abs_n.z) {
        // X dominant: +X or -X face
        float inv = 1.0f / abs_n.x;
        face_u = (n.x > 0) ? -n.z * inv : n.z * inv;
        face_v = -n.y * inv;
    } else if (abs_n.y >= abs_n.x && abs_n.y >= abs_n.z) {
        // Y dominant: +Y or -Y face
        float inv = 1.0f / abs_n.y;
        face_u = n.x * inv;
        face_v = (n.y > 0) ? n.z * inv : -n.z * inv;
    } else {
        // Z dominant: +Z or -Z face
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

// ---------------------------------------------------------------------------
// Ray-shape intersection functions
// ---------------------------------------------------------------------------

constant float T_MIN = 0.001f;
constant float T_MAX = 1e20f;

// Sphere intersection: quadratic formula
// params[0-2]=center, params[3]=radius
// Returns outward normal (always pointing away from center)
bool intersect_sphere(Ray ray, constant float* params, float t_min, float t_max,
                      thread float& t_hit, thread float3& outward_normal) {
    float3 center = float3(params[0], params[1], params[2]);
    float radius = params[3];

    float3 oc = ray.origin - center;
    float a = dot(ray.direction, ray.direction);
    float half_b = dot(oc, ray.direction);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = half_b * half_b - a * c;

    if (discriminant < 0.0f) return false;

    float sqrt_d = sqrt(discriminant);
    float root = (-half_b - sqrt_d) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrt_d) / a;
        if (root < t_min || root > t_max) {
            return false;
        }
    }

    t_hit = root;
    float3 hit_point = ray.origin + root * ray.direction;
    outward_normal = (hit_point - center) / radius;
    return true;
}

// Plane intersection: dot product
// params[0-2]=point, params[4-6]=normal
// Returns the geometric plane normal (not flipped)
bool intersect_plane(Ray ray, constant float* params, float t_min, float t_max,
                     thread float& t_hit, thread float3& outward_normal) {
    float3 plane_point = float3(params[0], params[1], params[2]);
    float3 plane_normal = float3(params[4], params[5], params[6]);

    float denom = dot(plane_normal, ray.direction);
    if (abs(denom) < 1e-8f) return false;

    float t = dot(plane_point - ray.origin, plane_normal) / denom;
    if (t < t_min || t > t_max) return false;

    t_hit = t;
    // Return the geometric normal (outward). The caller will handle front_face.
    outward_normal = plane_normal;
    return true;
}

// Box intersection: slab method (AABB)
// params[0-2]=bmin, params[4-6]=bmax
bool intersect_box(Ray ray, constant float* params, float t_min, float t_max,
                   thread float& t_hit, thread float3& outward_normal) {
    float3 bmin = float3(params[0], params[1], params[2]);
    float3 bmax = float3(params[4], params[5], params[6]);

    float3 inv_dir = 1.0f / ray.direction;

    float3 t0s = (bmin - ray.origin) * inv_dir;
    float3 t1s = (bmax - ray.origin) * inv_dir;

    float3 tsmaller = min(t0s, t1s);
    float3 tbigger  = max(t0s, t1s);

    float tmin_slab = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    float tmax_slab = min(min(tbigger.x, tbigger.y), tbigger.z);

    if (tmin_slab > tmax_slab) return false;

    float t = tmin_slab;
    if (t < t_min) {
        t = tmax_slab;
        if (t < t_min || t > t_max) return false;
    }
    if (t > t_max) return false;

    t_hit = t;

    // Compute outward-facing normal based on which face was hit
    float3 hit_point = ray.origin + t * ray.direction;
    float3 center = 0.5f * (bmin + bmax);
    float3 half_size = 0.5f * (bmax - bmin);
    float3 d = (hit_point - center) / half_size;

    // Find the dominant axis
    float3 abs_d = abs(d);
    if (abs_d.x > abs_d.y && abs_d.x > abs_d.z) {
        outward_normal = float3(sign(d.x), 0.0f, 0.0f);
    } else if (abs_d.y > abs_d.z) {
        outward_normal = float3(0.0f, sign(d.y), 0.0f);
    } else {
        outward_normal = float3(0.0f, 0.0f, sign(d.z));
    }

    return true;
}

// Cylinder intersection: Y-axis aligned with caps
// params[0-2]=center, params[3]=radius, params[4-6]=axis(unused, Y-aligned), params[7]=half_height
bool intersect_cylinder(Ray ray, constant float* params, float t_min, float t_max,
                        thread float& t_hit, thread float3& outward_normal) {
    float3 center = float3(params[0], params[1], params[2]);
    float radius = params[3];
    float half_height = params[7];
    float y_min = center.y;
    float y_max = center.y + 2.0f * half_height;

    bool hit_anything = false;
    float closest = t_max;

    // Body intersection: quadratic in XZ plane
    float ox = ray.origin.x - center.x;
    float oz = ray.origin.z - center.z;
    float dx = ray.direction.x;
    float dz = ray.direction.z;

    float a = dx * dx + dz * dz;
    if (a > 1e-8f) {
        float b = 2.0f * (ox * dx + oz * dz);
        float c = ox * ox + oz * oz - radius * radius;
        float discriminant = b * b - 4.0f * a * c;

        if (discriminant >= 0.0f) {
            float sqrt_d = sqrt(discriminant);
            float inv_2a = 1.0f / (2.0f * a);

            for (int i = 0; i < 2; ++i) {
                float t = (-b + (i == 0 ? -sqrt_d : sqrt_d)) * inv_2a;
                if (t >= t_min && t < closest) {
                    float y = ray.origin.y + t * ray.direction.y;
                    if (y >= y_min && y <= y_max) {
                        closest = t;
                        hit_anything = true;
                        float3 hp = ray.origin + t * ray.direction;
                        outward_normal = float3(hp.x - center.x, 0.0f, hp.z - center.z) / radius;
                    }
                }
            }
        }
    }

    // Cap intersection: bottom (y_min) and top (y_max)
    float dy = ray.direction.y;
    if (abs(dy) > 1e-8f) {
        for (int cap = 0; cap < 2; ++cap) {
            float cap_y = (cap == 0) ? y_min : y_max;
            float t = (cap_y - ray.origin.y) / dy;
            if (t >= t_min && t < closest) {
                float3 hp = ray.origin + t * ray.direction;
                float cx = hp.x - center.x;
                float cz = hp.z - center.z;
                if (cx * cx + cz * cz <= radius * radius) {
                    closest = t;
                    hit_anything = true;
                    outward_normal = (cap_y > center.y + half_height)
                           ? float3(0.0f, 1.0f, 0.0f)
                           : float3(0.0f, -1.0f, 0.0f);
                }
            }
        }
    }

    if (hit_anything) {
        t_hit = closest;
    }
    return hit_anything;
}

// Triangle intersection: Moller-Trumbore algorithm
// params[0-2]=v0, params[4-6]=v1, params[8-10]=v2
bool intersect_triangle(Ray ray, constant float* params, float t_min, float t_max,
                        thread float& t_hit, thread float3& outward_normal) {
    float3 v0 = float3(params[0], params[1], params[2]);
    float3 v1 = float3(params[4], params[5], params[6]);
    float3 v2 = float3(params[8], params[9], params[10]);

    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);

    if (abs(a) < 1e-8f) return false;

    float f = 1.0f / a;
    float3 s = ray.origin - v0;
    float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    float3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    float t = f * dot(edge2, q);
    if (t < t_min || t > t_max) return false;

    t_hit = t;
    outward_normal = normalize(cross(edge1, edge2));
    return true;
}

// ---------------------------------------------------------------------------
// Transform support: transform ray by inverse matrix
// ---------------------------------------------------------------------------
Ray transform_ray(Ray ray, float4x4 inv_transform) {
    Ray transformed;
    float4 o = inv_transform * float4(ray.origin, 1.0f);
    float4 d = inv_transform * float4(ray.direction, 0.0f);
    transformed.origin = o.xyz;
    transformed.direction = d.xyz;
    return transformed;
}

float3 transform_normal(float3 normal, float4x4 inv_transform) {
    // Normal transforms by transpose of inverse
    float3 n;
    n.x = inv_transform[0][0] * normal.x + inv_transform[1][0] * normal.y + inv_transform[2][0] * normal.z;
    n.y = inv_transform[0][1] * normal.x + inv_transform[1][1] * normal.y + inv_transform[2][1] * normal.z;
    n.z = inv_transform[0][2] * normal.x + inv_transform[1][2] * normal.y + inv_transform[2][2] * normal.z;
    return normalize(n);
}

// ---------------------------------------------------------------------------
// Linear BVH node struct -- must match C++ LinearBVHNode layout (32 bytes)
// ---------------------------------------------------------------------------
struct LinearBVHNode {
    packed_float3 aabb_min;  //  0: bounding box minimum
    uint   offset;           // 12: second child (interior) or first prim (leaf)
    packed_float3 aabb_max;  // 16: bounding box maximum
    uint   count;            // 28: 0 for interior, >0 for leaf (prim count)
};

// ---------------------------------------------------------------------------
// Ray-AABB intersection (slab method)
// ---------------------------------------------------------------------------
bool intersect_aabb(Ray ray, float3 aabb_min, float3 aabb_max, float t_max) {
    float3 inv_dir = 1.0f / ray.direction;
    float3 t0s = (aabb_min - ray.origin) * inv_dir;
    float3 t1s = (aabb_max - ray.origin) * inv_dir;
    float3 tsmaller = min(t0s, t1s);
    float3 tbigger  = max(t0s, t1s);
    float tmin = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    float tmax_aabb = min(min(tbigger.x, tbigger.y), tbigger.z);
    return tmax_aabb >= max(tmin, T_MIN) && tmin < t_max;
}

// ---------------------------------------------------------------------------
// Scene intersection: brute-force over all shapes, find closest
// Now returns outward_normal and computes front_face for dielectric support
// ---------------------------------------------------------------------------
bool intersect_scene(Ray ray,
                     constant uchar* shapes,
                     uint shape_count,
                     float t_min_val,
                     float t_max_val,
                     thread HitRecord& rec) {
    bool hit_anything = false;
    float closest = t_max_val;

    for (uint i = 0; i < shape_count; ++i) {
        constant GPUShape& shape = *(constant GPUShape*)(shapes + i * 128);
        constant float* params = shape.params;

        float t_hit = 0.0f;
        float3 outward_normal = float3(0.0f);
        bool did_hit = false;

        Ray test_ray = ray;
        if (shape.has_transform) {
            test_ray = transform_ray(ray, shape.inverse_transform);
        }

        if (shape.shape_type == SHAPE_SPHERE) {
            did_hit = intersect_sphere(test_ray, params, t_min_val, closest, t_hit, outward_normal);
        } else if (shape.shape_type == SHAPE_PLANE) {
            did_hit = intersect_plane(test_ray, params, t_min_val, closest, t_hit, outward_normal);
        } else if (shape.shape_type == SHAPE_BOX) {
            did_hit = intersect_box(test_ray, params, t_min_val, closest, t_hit, outward_normal);
        } else if (shape.shape_type == SHAPE_CYLINDER) {
            did_hit = intersect_cylinder(test_ray, params, t_min_val, closest, t_hit, outward_normal);
        } else if (shape.shape_type == SHAPE_TRIANGLE) {
            did_hit = intersect_triangle(test_ray, params, t_min_val, closest, t_hit, outward_normal);
        }

        if (did_hit) {
            hit_anything = true;
            closest = t_hit;

            // Save object-space normal before world-space transform
            // (needed for sphere UV so texture rotates with the sphere)
            float3 object_space_normal = outward_normal;

            if (shape.has_transform) {
                rec.point = ray.origin + t_hit * ray.direction;
                outward_normal = transform_normal(outward_normal, shape.inverse_transform);
            } else {
                rec.point = ray.origin + t_hit * ray.direction;
            }

            // Determine front_face and set normal to always face against the ray
            bool ff = dot(ray.direction, outward_normal) < 0.0f;
            rec.front_face = ff;
            rec.normal = ff ? outward_normal : -outward_normal;

            rec.t = t_hit;
            rec.material_index = shape.material_index;
            rec.shape_type = shape.shape_type;

            // Compute UV for spheres using object-space normal
            if (shape.shape_type == SHAPE_SPHERE) {
                sphere_uv(object_space_normal, rec.u, rec.v);
            } else {
                rec.u = 0.0f;
                rec.v = 0.0f;
            }
        }
    }

    return hit_anything;
}

// ---------------------------------------------------------------------------
// Scene intersection: BVH stack-based traversal (64-entry fixed stack)
// ---------------------------------------------------------------------------
bool intersect_scene_bvh(Ray ray,
                         constant uchar* bvh_nodes,
                         uint bvh_count,
                         constant uchar* shapes,
                         uint shape_count,
                         float t_min_val,
                         float t_max_val,
                         thread HitRecord& rec) {
    uint stack[128];
    int stack_ptr = 0;
    stack[stack_ptr++] = 0;  // start at root node

    bool hit_anything = false;
    float closest = t_max_val;

    while (stack_ptr > 0) {
        uint node_idx = stack[--stack_ptr];

        if (node_idx >= bvh_count) continue;

        constant LinearBVHNode& node = *(constant LinearBVHNode*)(bvh_nodes + node_idx * 32);

        if (!intersect_aabb(ray, node.aabb_min, node.aabb_max, closest))
            continue;

        if (node.count > 0) {
            // Leaf: test primitives
            for (uint i = 0; i < node.count; ++i) {
                uint shape_idx = node.offset + i;
                if (shape_idx >= shape_count) continue;

                constant GPUShape& shape = *(constant GPUShape*)(shapes + shape_idx * 128);
                constant float* params = shape.params;

                float t_hit = 0.0f;
                float3 outward_normal = float3(0.0f);
                bool did_hit = false;

                Ray test_ray = ray;
                if (shape.has_transform) {
                    test_ray = transform_ray(ray, shape.inverse_transform);
                }

                if (shape.shape_type == SHAPE_SPHERE) {
                    did_hit = intersect_sphere(test_ray, params, t_min_val, closest, t_hit, outward_normal);
                } else if (shape.shape_type == SHAPE_PLANE) {
                    did_hit = intersect_plane(test_ray, params, t_min_val, closest, t_hit, outward_normal);
                } else if (shape.shape_type == SHAPE_BOX) {
                    did_hit = intersect_box(test_ray, params, t_min_val, closest, t_hit, outward_normal);
                } else if (shape.shape_type == SHAPE_CYLINDER) {
                    did_hit = intersect_cylinder(test_ray, params, t_min_val, closest, t_hit, outward_normal);
                } else if (shape.shape_type == SHAPE_TRIANGLE) {
                    did_hit = intersect_triangle(test_ray, params, t_min_val, closest, t_hit, outward_normal);
                }

                if (did_hit) {
                    hit_anything = true;
                    closest = t_hit;

                    // Save object-space normal before world-space transform
                    float3 object_space_normal = outward_normal;

                    if (shape.has_transform) {
                        rec.point = ray.origin + t_hit * ray.direction;
                        outward_normal = transform_normal(outward_normal, shape.inverse_transform);
                    } else {
                        rec.point = ray.origin + t_hit * ray.direction;
                    }

                    bool ff = dot(ray.direction, outward_normal) < 0.0f;
                    rec.front_face = ff;
                    rec.normal = ff ? outward_normal : -outward_normal;

                    rec.t = t_hit;
                    rec.material_index = shape.material_index;
                    rec.shape_type = shape.shape_type;

                    // Compute UV for spheres using object-space normal
                    if (shape.shape_type == SHAPE_SPHERE) {
                        sphere_uv(object_space_normal, rec.u, rec.v);
                    } else {
                        rec.u = 0.0f;
                        rec.v = 0.0f;
                    }
                }
            }
        } else {
            // Interior: push children (left = node_idx+1, right = node.offset)
            if (stack_ptr + 1 < 128) {
                stack[stack_ptr++] = node.offset;     // right child (far)
                stack[stack_ptr++] = node_idx + 1;    // left child (near, process first)
            }
        }
    }
    return hit_anything;
}

// ---------------------------------------------------------------------------
// Direct lighting computation (shadow rays with glass transparency)
// ---------------------------------------------------------------------------
float3 compute_direct_lighting(HitRecord rec,
                               float3 attenuation,
                               constant uchar* lights,
                               uint light_count,
                               constant uchar* shapes,
                               uint shape_count,
                               constant uchar* materials,
                               uint material_count,
                               constant uchar* bvh_nodes,
                               uint bvh_count) {
    float3 color = float3(0.0f);

    for (uint i = 0; i < light_count; ++i) {
        constant GPULight& light = *(constant GPULight*)(lights + i * 64);

        float3 light_dir;
        float light_dist;
        float3 light_intensity;

        if (light.light_type == LIGHT_POINT) {
            float3 to_light = light.position - rec.point;
            light_dist = length(to_light);
            light_dir = to_light / light_dist;
            float attenuation = 1.0f / (1.0f + light_dist * light_dist);
            light_intensity = light.color * light.intensity * attenuation;
        } else {
            // Directional light: position stores direction toward light
            light_dir = normalize(light.position);
            light_dist = T_MAX;
            light_intensity = light.color * light.intensity;
        }

        // Shadow ray with transparency: trace through dielectric surfaces,
        // accumulating tint attenuation. Opaque surfaces block completely.
        float3 shadow_origin = rec.point + T_MIN * rec.normal;
        float3 shadow_transparency = float3(1.0f);
        float remaining_dist = light_dist;
        bool fully_blocked = false;

        for (int bounce = 0; bounce < 8; ++bounce) {
            Ray shadow_ray;
            shadow_ray.origin = shadow_origin;
            shadow_ray.direction = light_dir;

            HitRecord shadow_rec;
            bool hit;
            if (bvh_count > 0) {
                hit = intersect_scene_bvh(shadow_ray, bvh_nodes, bvh_count,
                                          shapes, shape_count,
                                          T_MIN, remaining_dist, shadow_rec);
            } else {
                hit = intersect_scene(shadow_ray, shapes, shape_count,
                                      T_MIN, remaining_dist, shadow_rec);
            }

            if (!hit) break; // clear path to light

            // Check if the hit surface is dielectric (transparent)
            if (shadow_rec.material_index < material_count) {
                constant GPUMaterial& shadow_mat = *(constant GPUMaterial*)(materials + shadow_rec.material_index * 48);
                if (shadow_mat.material_type == MAT_DIELECTRIC) {
                    // Attenuate by tint color and continue through
                    shadow_transparency *= shadow_mat.tint;
                    shadow_origin = shadow_rec.point + T_MIN * light_dir;
                    remaining_dist -= shadow_rec.t;
                    if (remaining_dist < T_MIN) { break; }
                    continue;
                }
            }

            // Opaque surface: fully blocked
            fully_blocked = true;
            break;
        }

        if (!fully_blocked) {
            float diffuse_factor = max(0.0f, dot(rec.normal, light_dir));
            color += (attenuation / M_PI_F) * light_intensity * shadow_transparency * diffuse_factor;
        }
    }

    return color;
}

// ---------------------------------------------------------------------------
// Main kernel: iterative multi-bounce ray tracing
// ---------------------------------------------------------------------------
kernel void ray_trace_kernel(
    constant GPUCamera& camera     [[buffer(0)]],
    device float4* output          [[buffer(1)]],
    constant uchar* shapes         [[buffer(2)]],
    constant uchar* materials      [[buffer(3)]],
    constant uchar* lights         [[buffer(4)]],
    constant uint* scene_counts    [[buffer(5)]],
    constant uchar* bvh_nodes      [[buffer(6)]],
    constant uchar* texture_data   [[buffer(7)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= camera.image_width || gid.y >= camera.image_height) return;

    uint idx = gid.y * camera.image_width + gid.x;

    uint shape_count    = scene_counts[0];
    uint material_count = scene_counts[1];
    uint light_count    = scene_counts[2];
    uint bvh_count      = scene_counts[3];
    uint max_depth      = camera.max_depth;
    uint spp            = camera.samples_per_pixel;

    // Initialize RNG seed from pixel coordinates and batch index
    // to ensure each batch produces unique random sequences
    uint rng_seed = pcg_hash(idx * 1099u + 7919u + camera.batch_index * 277363u);

    float3 accumulated_color = float3(0.0f);

    for (uint sample = 0; sample < spp; ++sample) {
        // Compute ray for this pixel with sub-pixel jitter for multi-SPP
        float jitter_u = 0.0f;
        float jitter_v = 0.0f;
        if (spp > 1) {
            jitter_u = random_float(rng_seed) - 0.5f;
            jitter_v = random_float(rng_seed) - 0.5f;
        }

        float3 pixel_center = camera.pixel00_loc
                            + (float(gid.x) + jitter_u) * camera.pixel_delta_u
                            + (float(gid.y) + jitter_v) * camera.pixel_delta_v;
        float3 ray_direction = pixel_center - camera.lookfrom;

        Ray current_ray;
        current_ray.origin = camera.lookfrom;
        current_ray.direction = ray_direction;

        float3 throughput = float3(1.0f);
        float3 color = float3(0.0f);
        bool debug_hit_jelly_triangle = false;

        for (uint bounce = 0; bounce < max_depth; ++bounce) {
            HitRecord rec;
            bool scene_hit;
            if (bvh_count > 0) {
                scene_hit = intersect_scene_bvh(current_ray, bvh_nodes, bvh_count,
                                                shapes, shape_count, T_MIN, T_MAX, rec);
            } else {
                scene_hit = intersect_scene(current_ray, shapes, shape_count, T_MIN, T_MAX, rec);
            }
            if (!scene_hit) {
                // Sky gradient only on primary ray (bounce 0).
                // Indirect misses are already accounted for by explicit
                // direct lighting (NEE) — adding sky here would double-count.
                if (bounce == 0) {
                    float3 unit_dir = normalize(current_ray.direction);
                    float a = 0.5f * (unit_dir.y + 1.0f);
                    color += throughput * ((1.0f - a) * camera.background_bottom + a * camera.background_top);
                }
                break;
            }

            // Fetch material
            uint mat_type = MAT_LAMBERTIAN;
            float3 albedo = float3(0.5f);
            float param1 = 0.0f;
            float3 tint = float3(1.0f);

            if (rec.material_index < material_count) {
                constant GPUMaterial& mat = *(constant GPUMaterial*)(materials + rec.material_index * 48);
                mat_type = mat.material_type;
                albedo = mat.albedo;
                param1 = mat.param1;
                tint = mat.tint;

                // Sample texture if material has one
                if (mat.texture_offset >= 0) {
                    bool is_equirectangular = (mat.texture_width > mat.texture_height * 1.8f);
                    if (mat.texture_scale < 0.0f && rec.shape_type == SHAPE_SPHERE) {
                        // Cube map mode: seam-free texture mapping for spheres
                        // Reconstruct object-space normal from sphere UV so texture
                        // rotates with the sphere (consistent with equirectangular path)
                        float theta_os = (1.0f - rec.v) * M_PI_F;
                        float phi_os = rec.u * 2.0f * M_PI_F - M_PI_F;
                        float3 obj_n = float3(cos(phi_os) * sin(theta_os),
                                              -cos(theta_os),
                                              -sin(phi_os) * sin(theta_os));
                        albedo = sample_sphere_cube_map(texture_data, mat.texture_offset,
                                                         mat.texture_width, mat.texture_height,
                                                         obj_n);
                    } else if (rec.shape_type == SHAPE_SPHERE && is_equirectangular) {
                        // Equirectangular maps (Earth, Moon): standard sphere UV with wrapping
                        albedo = sample_texture(texture_data, mat.texture_offset,
                                                mat.texture_width, mat.texture_height,
                                                rec.u, rec.v);
                    } else if (rec.shape_type == SHAPE_SPHERE && mat.texture_scale > 0.0f) {
                        // Scaled texture on sphere: small texture centered, white background
                        float scale = mat.texture_scale;
                        float su = (rec.u - 0.5f) / scale + 0.5f;
                        float sv = (rec.v - 0.5f) / scale + 0.5f;
                        if (su >= 0.0f && su <= 1.0f && sv >= 0.0f && sv <= 1.0f) {
                            albedo = sample_texture_clamped(texture_data, mat.texture_offset,
                                                            mat.texture_width, mat.texture_height,
                                                            su, sv);
                        } else {
                            albedo = float3(0.95f, 0.95f, 0.95f); // white sclera
                        }
                    } else if (rec.shape_type == SHAPE_SPHERE) {
                        // Non-equirectangular on sphere: dual-hemisphere blending (seam-free)
                        // Reconstruct object-space normal from sphere UV so texture
                        // rotates with the sphere (rec.normal is world-space)
                        float theta_dh = (1.0f - rec.v) * M_PI_F;
                        float phi_dh = rec.u * 2.0f * M_PI_F - M_PI_F;
                        float3 obj_n_dh = float3(cos(phi_dh) * sin(theta_dh),
                                                  -cos(theta_dh),
                                                  -sin(phi_dh) * sin(theta_dh));
                        albedo = sample_sphere_dual_hemisphere(texture_data, mat.texture_offset,
                                                               mat.texture_width, mat.texture_height,
                                                               obj_n_dh);
                    } else {
                        // Non-sphere shapes: triplanar mapping
                        albedo = sample_texture_triplanar(texture_data, mat.texture_offset,
                                                          mat.texture_width, mat.texture_height,
                                                          rec.point, rec.normal, 2.0f);
                    }
                }
            }

            // --- Emissive material ---
            if (mat_type == MAT_EMISSIVE) {
                // Emit light: intensity * color
                float3 emission = param1 * albedo;
                color += throughput * emission;
                // Emissive materials do not scatter -- absorbed
                break;
            }

            // --- Lambertian material ---
            if (mat_type == MAT_LAMBERTIAN) {
                // Ambient term — primary hit only to avoid per-bounce accumulation
                if (bounce == 0) {
                    color += throughput * camera.ambient_factor * albedo;
                }

                // Direct lighting
                color += throughput * compute_direct_lighting(
                    rec, albedo, lights, light_count, shapes, shape_count,
                    materials, material_count, bvh_nodes, bvh_count);

                // Scatter: random direction on hemisphere (normal + random in unit sphere)
                float3 scatter_dir = rec.normal + random_in_unit_sphere(rng_seed);
                // Catch degenerate scatter direction
                if (dot(scatter_dir, scatter_dir) < 1e-8f) {
                    scatter_dir = rec.normal;
                }
                scatter_dir = normalize(scatter_dir);

                throughput *= albedo;
                current_ray.origin = rec.point + T_MIN * rec.normal;
                current_ray.direction = scatter_dir;
                continue;
            }

            // --- Metal material ---
            if (mat_type == MAT_METAL) {
                float fuzziness = param1;
                float3 unit_dir = normalize(current_ray.direction);
                float3 reflected = reflect_vec(unit_dir, rec.normal);
                float3 scatter_dir = reflected + fuzziness * random_in_unit_sphere(rng_seed);

                // Check that scattered ray is in the same hemisphere as normal
                if (dot(scatter_dir, rec.normal) <= 0.0f) {
                    // Absorbed (fuzziness caused ray to scatter below surface)
                    break;
                }

                // Ambient term for metal
                color += throughput * camera.ambient_factor * albedo;

                // Direct lighting for metal
                color += throughput * compute_direct_lighting(
                    rec, albedo, lights, light_count, shapes, shape_count,
                    materials, material_count, bvh_nodes, bvh_count);

                throughput *= albedo;
                current_ray.origin = rec.point + T_MIN * rec.normal;
                current_ray.direction = normalize(scatter_dir);
                continue;
            }

            // --- Checker Metal material ---
            if (mat_type == MAT_CHECKER_METAL) {
                // 3D checker pattern: albedo = color_a, tint = color_b, texture_scale = checker scale
                constant GPUMaterial& cmat = *(constant GPUMaterial*)(materials + rec.material_index * 48);
                float checker_scale = cmat.texture_scale;
                if (checker_scale <= 0.0f) checker_scale = 1.0f;
                // 2D checker (X+Z) to avoid float-precision artifacts on
                // surfaces aligned to axis boundaries (e.g. floor at y=0).
                int ix = int(floor(rec.point.x * checker_scale));
                int iz = int(floor(rec.point.z * checker_scale));
                bool is_even = ((ix + iz) & 1) == 0;
                float3 checker_albedo = is_even ? albedo : tint;

                float fuzziness = param1;
                float3 unit_dir = normalize(current_ray.direction);
                float3 reflected = reflect_vec(unit_dir, rec.normal);
                float3 scatter_dir = reflected + fuzziness * random_in_unit_sphere(rng_seed);

                if (dot(scatter_dir, rec.normal) <= 0.0f) {
                    break;
                }

                color += throughput * camera.ambient_factor * checker_albedo;
                color += throughput * compute_direct_lighting(
                    rec, checker_albedo, lights, light_count, shapes, shape_count,
                    materials, material_count, bvh_nodes, bvh_count);

                throughput *= checker_albedo;
                current_ray.origin = rec.point + T_MIN * rec.normal;
                current_ray.direction = normalize(scatter_dir);
                continue;
            }

            // --- Dielectric material ---
            if (mat_type == MAT_DIELECTRIC) {
                float ior = param1;
                float3 attenuation_color = tint;

                // Full dielectric behavior (transparent glass/jelly)
                float eta = rec.front_face ? (1.0f / ior) : ior;
                float3 unit_dir = normalize(current_ray.direction);
                float cos_theta = min(dot(-unit_dir, rec.normal), 1.0f);
                float sin_theta = sqrt(1.0f - cos_theta * cos_theta);

                bool cannot_refract = (eta * sin_theta > 1.0f);
                float3 scatter_dir;

                if (cannot_refract || schlick_reflectance(cos_theta, eta) > random_float(rng_seed)) {
                    // Total internal reflection or Fresnel reflection
                    scatter_dir = reflect_vec(unit_dir, rec.normal);
                } else {
                    // Refract
                    scatter_dir = refract_vec(unit_dir, rec.normal, eta);
                }

                throughput *= attenuation_color;
                current_ray.origin = rec.point + T_MIN * scatter_dir;
                current_ray.direction = normalize(scatter_dir);
                continue;
            }

            // Unknown material type: treat as absorbed
            break;
        }

        // Replace NaN/inf per-sample color with zero to prevent poisoning
        if (any(isnan(color)) || any(isinf(color))) color = float3(0.0f);

        accumulated_color += color;
    }

    // Average over samples in this batch
    float3 batch_avg = accumulated_color / float(spp);

    // Replace NaN/inf with zero
    if (isnan(batch_avg.x) || isinf(batch_avg.x)) batch_avg.x = 0.0f;
    if (isnan(batch_avg.y) || isinf(batch_avg.y)) batch_avg.y = 0.0f;
    if (isnan(batch_avg.z) || isinf(batch_avg.z)) batch_avg.z = 0.0f;

    // Weight this batch's contribution by 1/num_batches
    float3 weighted = batch_avg / float(camera.num_batches);

    if (camera.batch_index == 0) {
        output[idx] = float4(weighted, 1.0f);
    } else {
        // Accumulate weighted batch contribution
        output[idx] = float4(output[idx].xyz + weighted, 1.0f);
    }

    // Apply gamma correction only on the final batch
    if (camera.batch_index == camera.num_batches - 1) {
        float3 final_color = clamp(output[idx].xyz, 0.0f, 1.0f);
        output[idx] = float4(sqrt(final_color), 1.0f);
    }
}
