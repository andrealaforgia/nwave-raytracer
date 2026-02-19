# Physics Integration Data Flow

## Overview

This diagram shows how data flows from the YAML scene file through physics simulation to rendered frames, crossing architectural ring boundaries at each step.

```mermaid
flowchart TD
    subgraph External["External (User's File System)"]
        YAML["scene.yaml<br/>Materials + Objects + Physics + Animation"]
        FRAMES["frames/<br/>frame_0000.ppm ... frame_0149.ppm"]
        VIDEO["output.mp4<br/>(user runs ffmpeg)"]
    end

    subgraph R4["Ring 4: Infrastructure"]
        LOADER["YamlSceneLoader<br/>yaml-cpp parsing"]
        VALIDATOR["Validator<br/>Integrity checks"]
        JOLT["JoltPhysicsSimulator<br/>Jolt Physics adapter"]
        PPM["PPMWriter<br/>PPM file output"]
        CLID["CLI Dispatcher<br/>Subcommand routing"]
    end

    subgraph R3["Ring 3: Application"]
        PHYS_IF["PhysicsSimulator<br/>(abstract interface)"]
        ANIM["AnimationRenderer<br/>Orchestrator"]
        REND["Renderer<br/>Per-frame ray tracing"]
    end

    subgraph R2["Ring 2: Domain"]
        SCENE["Scene<br/>shapes + lights"]
        TSHAPE["TransformedShape<br/>Shape + Matrix4x4"]
        PPROPS["PhysicsProperties<br/>mass, velocity, friction"]
        ACFG["AnimationConfig<br/>duration, fps, timestep"]
        SHAPES["Original Shapes<br/>Sphere, Box, ..."]
        CAM["Camera"]
    end

    subgraph R1["Ring 1: Core / Math"]
        MAT4["Matrix4x4<br/>4x4 transform"]
        QUAT["Quaternion<br/>rotation"]
        VEC3["Vec3, Ray, Point3"]
    end

    %% Data flow arrows
    YAML -->|"parse"| LOADER
    LOADER -->|"constructs"| SCENE
    LOADER -->|"constructs"| PPROPS
    LOADER -->|"constructs"| ACFG
    LOADER -->|"constructs"| CAM

    CLID -->|"invokes"| LOADER
    CLID -->|"invokes"| VALIDATOR
    CLID -->|"invokes"| ANIM

    VALIDATOR -->|"inspects"| SCENE
    VALIDATOR -->|"inspects"| PPROPS
    VALIDATOR -->|"inspects"| ACFG

    ANIM -->|"wraps shapes in"| TSHAPE
    ANIM -->|"registers bodies"| PHYS_IF
    JOLT -->|"implements"| PHYS_IF

    ANIM -->|"step(dt)"| PHYS_IF
    PHYS_IF -->|"position + rotation"| ANIM
    ANIM -->|"Matrix4x4::from_translation_rotation"| MAT4
    MAT4 -->|"constructed from"| QUAT
    ANIM -->|"set_transform(matrix)"| TSHAPE

    TSHAPE -->|"wraps"| SHAPES
    TSHAPE -->|"uses for ray transform"| MAT4

    ANIM -->|"render(camera, scene, settings)"| REND
    REND -->|"hit test via Shape::hit()"| TSHAPE
    REND -->|"pixel buffer"| ANIM

    ANIM -->|"write frame"| PPM
    PPM -->|"frame_NNNN.ppm"| FRAMES
    FRAMES -->|"ffmpeg"| VIDEO

    SHAPES --> VEC3
    CAM --> VEC3
```

## Ring Boundary Crossings

| # | Crossing | From | To | Data | Direction |
|---|---|---|---|---|---|
| 1 | YAML to Domain | Ring 4 (Loader) | Ring 2 (Scene, PhysicsProperties) | Parsed scene objects | Inward (R4 -> R2) |
| 2 | Domain to Physics | Ring 3 (AnimationRenderer) | Ring 3/4 (PhysicsSimulator/Jolt) | PhysicsBodyDesc (shape type + properties) | Outward via interface (R3 -> R4 impl) |
| 3 | Physics to Math | Ring 4 (Jolt) | Ring 1 (Matrix4x4, Quaternion) | Position + rotation -> transform matrix | Inward (R4 -> R1) |
| 4 | Math to Domain | Ring 1 (Matrix4x4) | Ring 2 (TransformedShape) | Transform matrix applied to shape | Inward (R1 -> R2) |
| 5 | Domain to Renderer | Ring 2 (TransformedShape) | Ring 3 (Renderer) | Shape::hit() interface | Inward (R2 -> R3) |
| 6 | Renderer to Writer | Ring 3 (Renderer) | Ring 4 (PPMWriter) | Pixel buffer | Outward via callback (R3 -> R4) |

All crossings respect the dependency rule: code dependencies always point inward. Data flows both ways, but only through interfaces defined in inner rings (PhysicsSimulator in R3) or callbacks (write_frame, progress).

## Transform Pipeline Detail

```mermaid
flowchart LR
    subgraph Jolt["Jolt Physics (Ring 4)"]
        JP["JPH::BodyInterface"]
    end

    subgraph Adapter["JoltPhysicsSimulator (Ring 4)"]
        GT["get_transform()"]
    end

    subgraph Math["Core Math (Ring 1)"]
        FTR["Matrix4x4::from_translation_rotation()"]
    end

    subgraph Domain["TransformedShape (Ring 2)"]
        ST["set_transform(matrix)"]
        HIT["hit(ray, t_min, t_max, rec)"]
    end

    subgraph Render["Renderer (Ring 3)"]
        TR["trace_ray()"]
    end

    JP -->|"JPH::RVec3 position<br/>JPH::Quat rotation"| GT
    GT -->|"PhysicsTransform<br/>{Point3, Quaternion}"| FTR
    FTR -->|"Matrix4x4"| ST
    ST -->|"stores inverse,<br/>inverse-transpose"| HIT

    TR -->|"Ray"| HIT
    HIT -->|"1. inverse_transform * ray<br/>2. inner_shape.hit(local_ray)<br/>3. transform * hit_point<br/>4. inv_transpose * normal"| TR
```
