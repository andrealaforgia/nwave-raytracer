# Rendering Pipeline Sequence Diagram

## Main Render Flow

```mermaid
sequenceDiagram
    participant User
    participant CLI
    participant Loader as SceneLoader
    participant Val as Validator
    participant Rend as Renderer
    participant BVH
    participant Samp as Sampler
    participant Cam as Camera
    participant Shape
    participant Mat as Material
    participant Light
    participant Writer as ImageWriter

    User->>CLI: nwave render scene.yaml --samples 100
    CLI->>Loader: load_scene("scene.yaml")
    Loader->>Loader: Parse YAML (yaml-cpp)
    Loader->>Loader: Construct Shapes, Materials, Lights, Camera
    Loader-->>CLI: Scene object

    CLI->>CLI: Apply CLI overrides (samples=100)
    CLI->>Val: validate(scene)
    Val->>Val: Check references, ranges, structure
    Val-->>CLI: ValidationResult (valid)

    CLI->>BVH: build(scene.shapes)
    BVH->>Shape: bounding_box() for each shape
    BVH->>BVH: Recursive split (longest axis midpoint)
    BVH-->>CLI: BVH root node

    CLI->>Rend: render(scene, settings, progress_callback)

    loop For each pixel (x, y)
        Rend->>Samp: generate_samples(spp)
        Samp-->>Rend: sample offsets [(u0,v0), (u1,v1), ...]

        loop For each sample offset (u, v)
            Rend->>Cam: generate_ray(u, v)
            Cam-->>Rend: Ray

            Rend->>Rend: trace_ray(ray, scene, depth=0)
            Note over Rend: See trace_ray detail below
        end

        Rend->>Rend: Average samples, gamma correct, clamp
        Rend->>Rend: Store in pixel_buffer[y * width + x]
    end

    Rend->>CLI: progress_callback(row, total_rows)
    Rend-->>CLI: RenderResult (pixel_buffer, stats)

    CLI->>Writer: write("output.ppm", pixels, width, height)
    Writer-->>CLI: File written

    CLI-->>User: Render complete. Output: output.ppm
```

## trace_ray Detail

```mermaid
sequenceDiagram
    participant Rend as trace_ray
    participant BVH
    participant Shape
    participant Mat as Material
    participant Light
    participant Scene

    Note over Rend: trace_ray(ray, scene, depth)

    alt depth >= max_depth
        Rend-->>Rend: return Color3(0, 0, 0)
    end

    Rend->>BVH: hit(ray, 0.001, infinity, hit_record)
    BVH->>BVH: Test AABB, traverse children
    BVH->>Shape: hit(ray, t_min, t_max, rec)
    Shape-->>BVH: HitRecord (or miss)
    BVH-->>Rend: closest HitRecord (or miss)

    alt No hit (ray misses all geometry)
        Rend-->>Rend: return background_color(ray)
    end

    Note over Rend: Hit found at hit_record

    Rend->>Mat: emit(u, v, point)
    Mat-->>Rend: emission_color

    Rend->>Mat: scatter(ray, hit_record, attenuation, scattered)

    alt Scatter returns false (absorbed)
        Rend-->>Rend: return emission_color
    end

    Note over Rend: Compute direct illumination

    loop For each light in scene
        Rend->>Light: illuminate(hit_point)
        Light-->>Rend: light_color, direction, distance

        loop For each shadow sample (1 for point/dir, N for area)
            Rend->>Scene: any_hit(shadow_ray, 0, distance)
            Scene-->>Rend: occluded (true/false)
        end

        alt Not occluded
            Rend->>Rend: direct += albedo * light_color * max(0, dot(N, L))
        end
    end

    Rend->>Rend: trace_ray(scattered_ray, scene, depth + 1)
    Note over Rend: Recursive call

    Rend-->>Rend: return emission + attenuation * recursive_color + direct
```

## Key Timing Characteristics

| Phase | Frequency | Performance Notes |
|---|---|---|
| Scene loading | Once per render | Negligible (small YAML files) |
| BVH construction | Once per render | O(N log N) where N = number of primitives |
| Pixel iteration | width * height times | Outer loop; parallelizable per row |
| Sample iteration | SPP times per pixel | Inner loop; sequential within pixel |
| trace_ray | SPP * width * height times (minimum) | Recursion multiplies this by avg depth |
| Shape::hit | O(log N) per ray (with BVH) | Hottest path; dominates render time |
| Material::scatter | Once per ray bounce | Relatively cheap; random number generation |
| Shadow rays | lights * shadow_samples per hit | "Any hit" early exit for efficiency |
