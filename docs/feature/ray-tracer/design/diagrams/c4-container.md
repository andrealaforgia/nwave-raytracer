# C4 Container Diagram: nwave-raytracer

## Diagram

```mermaid
graph TB
    user["<b>Developer / Artist</b>"]

    subgraph nwave ["nwave executable"]
        direction TB

        cli["<b>CLI Module</b><br/><i>C++17</i><br/>Parses arguments,<br/>dispatches commands"]

        subgraph infra ["Infrastructure (Ring 4)"]
            loader["<b>Scene Loader</b><br/><i>C++17 + yaml-cpp</i><br/>Parses YAML into<br/>domain objects"]
            validator["<b>Validator</b><br/><i>C++17</i><br/>Validates scene<br/>integrity"]
            writer["<b>Image Writer</b><br/><i>C++17 + stb</i><br/>Writes PPM/PNG<br/>image files"]
            progress["<b>Progress Reporter</b><br/><i>C++17</i><br/>Terminal progress bar"]
        end

        subgraph app ["Application (Ring 3)"]
            renderer["<b>Renderer</b><br/><i>C++17</i><br/>Orchestrates ray tracing:<br/>ray gen, intersect, shade"]
            bvh["<b>BVH Engine</b><br/><i>C++17</i><br/>Acceleration structure<br/>build and traversal"]
            sampler["<b>Sampler</b><br/><i>C++17</i><br/>Anti-aliasing<br/>sample generation"]
        end

        subgraph domain ["Domain (Ring 2)"]
            shapes["<b>Shapes</b><br/>Sphere, Plane, Triangle,<br/>Mesh, Box"]
            materials["<b>Materials</b><br/>Lambertian, Metal,<br/>Dielectric, Emissive"]
            lights["<b>Lights</b><br/>Point, Directional,<br/>Area"]
            camera["<b>Camera</b><br/>Pinhole + Thin Lens"]
            scene["<b>Scene</b><br/>Aggregate of shapes,<br/>lights, camera"]
        end

        subgraph core ["Core / Math (Ring 1)"]
            math["<b>Math Primitives</b><br/>Vec3, Ray, AABB,<br/>Matrix4, MathUtils"]
        end
    end

    fs["<b>File System</b>"]

    user -->|"nwave render scene.yaml"| cli
    cli --> loader
    cli --> validator
    cli --> renderer
    loader --> scene
    validator --> scene
    renderer --> bvh
    renderer --> sampler
    renderer --> shapes
    renderer --> materials
    renderer --> lights
    renderer --> camera
    renderer -->|"pixel buffer"| writer
    renderer -->|"progress callback"| progress
    bvh --> shapes
    shapes --> math
    materials --> math
    lights --> math
    camera --> math
    scene --> shapes
    scene --> lights
    scene --> camera
    loader -->|"reads YAML"| fs
    writer -->|"writes PPM/PNG"| fs

    classDef person fill:#08427B,stroke:#073B6F,color:#fff
    classDef ring1 fill:#438DD5,stroke:#3C7FC0,color:#fff
    classDef ring2 fill:#1168BD,stroke:#0E5AA0,color:#fff
    classDef ring3 fill:#0B4884,stroke:#093D6F,color:#fff
    classDef ring4 fill:#2D882D,stroke:#267326,color:#fff
    classDef external fill:#999999,stroke:#6B6B6B,color:#fff

    class user person
    class math ring1
    class shapes,materials,lights,camera,scene ring2
    class renderer,bvh,sampler ring3
    class cli,loader,validator,writer,progress ring4
    class fs external
```

## Container Descriptions

### Infrastructure (Ring 4) -- Outermost

| Container | Technology | Responsibility |
|---|---|---|
| CLI Module | C++17 | Parses `argc/argv`, dispatches to validate or render sub-commands, merges CLI overrides |
| Scene Loader | C++17 + yaml-cpp | Reads YAML files, constructs domain objects (Scene, Shapes, Materials, Lights, Camera) |
| Validator | C++17 | Checks scene integrity: material references, parameter ranges, structural completeness |
| Image Writer | C++17 + stb_image_write | Writes pixel buffer to PPM (P3/P6) or PNG files |
| Progress Reporter | C++17 | Renders terminal progress bar with row count, elapsed time, ETA |

### Application (Ring 3)

| Container | Technology | Responsibility |
|---|---|---|
| Renderer | C++17 | Main rendering loop: pixel iteration, sample accumulation, trace_ray recursion, gamma correction |
| BVH Engine | C++17 | Builds bounding volume hierarchy from shape list; implements Shape interface for transparent traversal |
| Sampler | C++17 | Generates sub-pixel sample offsets (RandomSampler, StratifiedSampler) |

### Domain (Ring 2)

| Container | Technology | Responsibility |
|---|---|---|
| Shapes | C++17 | Abstract Shape with concrete Sphere, Plane, Triangle, TriangleMesh, Box |
| Materials | C++17 | Abstract Material with concrete Lambertian, Metal, Dielectric, Emissive |
| Lights | C++17 | Abstract Light with concrete PointLight, DirectionalLight, AreaLight |
| Camera | C++17 | Pinhole and thin-lens ray generation |
| Scene | C++17 | Aggregate container for shapes, lights, camera, settings |

### Core / Math (Ring 1) -- Innermost

| Container | Technology | Responsibility |
|---|---|---|
| Math Primitives | C++17 | Vec3, Point3, Color3, Ray, AABB, Matrix4x4, math constants and utilities |

## Dependency Flow

```
Infrastructure --> Application --> Domain --> Core
     |                                          ^
     +------------------------------------------+
                  (also depends on Core directly)
```

Each ring depends only on inner rings. No outward dependencies.
