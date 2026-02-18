#include <metal_stdlib>
using namespace metal;

// ---------------------------------------------------------------------------
// GPU struct definitions -- must match C++ gpu_types.h layout exactly
// ---------------------------------------------------------------------------

struct GPUCamera {
    float3 lookfrom;        //  0
    float  _pad0;           // 12
    float3 pixel00_loc;     // 16
    float  _pad1;           // 28
    float3 pixel_delta_u;   // 32
    float  _pad2;           // 44
    float3 pixel_delta_v;   // 48
    float  _pad3;           // 60
    float3 background_top;  // 64
    float  _pad4;           // 76
    float3 background_bottom; // 80
    float  _pad5;           // 92
    uint   image_width;     // 96
    uint   image_height;    //100
    uint   samples_per_pixel; //104
    uint   max_depth;       //108
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
    float  _pad0;           //  4
    float  _pad1;           //  8
    float  _pad2;           // 12
    float3 albedo;          // 16
    float  param1;          // 28
    float3 tint;            // 32
    float  _pad3;           // 44
};                          // 48 total

// GPULight: 64 bytes
struct GPULight {
    uint   light_type;      //  0
    float  _pad0;           //  4
    float  _pad1;           //  8
    float  _pad2;           // 12
    float3 position;        // 16
    float  intensity;       // 28
    float3 color;           // 32
    float  _pad3;           // 44
    float4 _reserved;       // 48
};                          // 64 total

// Shape type tags
constant uint SHAPE_SPHERE   = 0;
constant uint SHAPE_PLANE    = 1;
constant uint SHAPE_BOX      = 2;

// Light type tags
constant uint LIGHT_POINT       = 0;
constant uint LIGHT_DIRECTIONAL = 1;

// ---------------------------------------------------------------------------
// Ray struct
// ---------------------------------------------------------------------------
struct Ray {
    float3 origin;
    float3 direction;
};

// Hit record
struct HitRecord {
    float  t;
    float3 point;
    float3 normal;
    uint   material_index;
};

// ---------------------------------------------------------------------------
// Ray-shape intersection functions
// ---------------------------------------------------------------------------

constant float T_MIN = 0.001f;
constant float T_MAX = 1e20f;

// Sphere intersection: quadratic formula
// params[0-2]=center, params[3]=radius
bool intersect_sphere(Ray ray, constant float* params, float t_min, float t_max,
                      thread float& t_hit, thread float3& normal) {
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
    normal = (hit_point - center) / radius;
    return true;
}

// Plane intersection: dot product
// params[0-2]=point, params[4-6]=normal
bool intersect_plane(Ray ray, constant float* params, float t_min, float t_max,
                     thread float& t_hit, thread float3& normal) {
    float3 plane_point = float3(params[0], params[1], params[2]);
    float3 plane_normal = float3(params[4], params[5], params[6]);

    float denom = dot(plane_normal, ray.direction);
    if (abs(denom) < 1e-8f) return false;

    float t = dot(plane_point - ray.origin, plane_normal) / denom;
    if (t < t_min || t > t_max) return false;

    t_hit = t;
    // Normal always faces toward the ray
    normal = (denom < 0.0f) ? plane_normal : -plane_normal;
    return true;
}

// Box intersection: slab method (AABB)
// params[0-2]=bmin, params[4-6]=bmax
bool intersect_box(Ray ray, constant float* params, float t_min, float t_max,
                   thread float& t_hit, thread float3& normal) {
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
        normal = float3(sign(d.x), 0.0f, 0.0f);
    } else if (abs_d.y > abs_d.z) {
        normal = float3(0.0f, sign(d.y), 0.0f);
    } else {
        normal = float3(0.0f, 0.0f, sign(d.z));
    }

    // Flip normal to face the ray
    if (dot(normal, ray.direction) > 0.0f) {
        normal = -normal;
    }

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
// Scene intersection: brute-force over all shapes, find closest
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
        float3 normal = float3(0.0f);
        bool did_hit = false;

        Ray test_ray = ray;
        if (shape.has_transform) {
            test_ray = transform_ray(ray, shape.inverse_transform);
        }

        if (shape.shape_type == SHAPE_SPHERE) {
            did_hit = intersect_sphere(test_ray, params, t_min_val, closest, t_hit, normal);
        } else if (shape.shape_type == SHAPE_PLANE) {
            did_hit = intersect_plane(test_ray, params, t_min_val, closest, t_hit, normal);
        } else if (shape.shape_type == SHAPE_BOX) {
            did_hit = intersect_box(test_ray, params, t_min_val, closest, t_hit, normal);
        }

        if (did_hit) {
            hit_anything = true;
            closest = t_hit;

            if (shape.has_transform) {
                // Compute hit point in world space using original ray
                rec.point = ray.origin + t_hit * ray.direction;
                rec.normal = transform_normal(normal, shape.inverse_transform);
            } else {
                rec.point = ray.origin + t_hit * ray.direction;
                rec.normal = normal;
            }

            rec.t = t_hit;
            rec.material_index = shape.material_index;
        }
    }

    return hit_anything;
}

// ---------------------------------------------------------------------------
// Lambertian shading with direct lighting and shadow rays
// ---------------------------------------------------------------------------
float3 shade(HitRecord rec,
             Ray ray,
             constant uchar* materials,
             uint material_count,
             constant uchar* lights,
             uint light_count,
             constant uchar* shapes,
             uint shape_count) {

    // Fetch material
    float3 albedo = float3(0.5f);
    if (rec.material_index < material_count) {
        constant GPUMaterial& mat = *(constant GPUMaterial*)(materials + rec.material_index * 48);
        albedo = mat.albedo;
    }

    // Ambient term
    float3 color = 0.05f * albedo;

    // Direct illumination from each light
    for (uint i = 0; i < light_count; ++i) {
        constant GPULight& light = *(constant GPULight*)(lights + i * 64);

        float3 light_dir;
        float light_dist;
        float3 light_intensity;

        if (light.light_type == LIGHT_POINT) {
            float3 to_light = light.position - rec.point;
            light_dist = length(to_light);
            light_dir = to_light / light_dist;
            light_intensity = light.color * light.intensity;
        } else {
            // Directional light: position stores direction toward light
            light_dir = normalize(light.position);
            light_dist = T_MAX;
            light_intensity = light.color * light.intensity;
        }

        // Shadow ray
        float3 shadow_origin = rec.point + T_MIN * rec.normal;
        Ray shadow_ray;
        shadow_ray.origin = shadow_origin;
        shadow_ray.direction = light_dir;

        HitRecord shadow_rec;
        bool in_shadow = intersect_scene(shadow_ray, shapes, shape_count,
                                         T_MIN, light_dist - T_MIN, shadow_rec);

        if (!in_shadow) {
            float diffuse_factor = max(0.0f, dot(rec.normal, light_dir));
            color += albedo * light_intensity * diffuse_factor;
        }
    }

    return color;
}

// ---------------------------------------------------------------------------
// Main kernel
// ---------------------------------------------------------------------------
kernel void ray_trace_kernel(
    constant GPUCamera& camera     [[buffer(0)]],
    device float4* output          [[buffer(1)]],
    constant uchar* shapes         [[buffer(2)]],
    constant uchar* materials      [[buffer(3)]],
    constant uchar* lights         [[buffer(4)]],
    constant uint* scene_counts    [[buffer(5)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= camera.image_width || gid.y >= camera.image_height) return;

    uint idx = gid.y * camera.image_width + gid.x;

    uint shape_count    = scene_counts[0];
    uint material_count = scene_counts[1];
    uint light_count    = scene_counts[2];

    // Compute ray for this pixel (center sample, no jitter)
    float3 pixel_center = camera.pixel00_loc
                        + float(gid.x) * camera.pixel_delta_u
                        + float(gid.y) * camera.pixel_delta_v;
    float3 ray_direction = pixel_center - camera.lookfrom;

    Ray ray;
    ray.origin = camera.lookfrom;
    ray.direction = ray_direction;

    float3 color;

    HitRecord rec;
    if (intersect_scene(ray, shapes, shape_count, T_MIN, T_MAX, rec)) {
        color = shade(rec, ray, materials, material_count,
                      lights, light_count, shapes, shape_count);
    } else {
        // Sky gradient: matches CPU formula exactly
        float3 unit_dir = normalize(ray_direction);
        float a = 0.5f * (unit_dir.y + 1.0f);
        color = (1.0f - a) * camera.background_bottom + a * camera.background_top;
    }

    // Gamma correction (gamma 2.0) to match CPU renderer
    color = sqrt(clamp(color, 0.0f, 1.0f));

    output[idx] = float4(color, 1.0f);
}
