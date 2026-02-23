# Glass Sweeper Prism -- C4 Component Diagrams

## 1. Component Diagram: Data Flow

Shows how the sweeper prism data flows from YAML through the architecture layers to rendered output. Modified components are marked with `(MODIFIED)`. New data paths are marked with `(NEW)`.

```mermaid
graph TB
    subgraph "Content Layer"
        YAML["nwave_bowling.yaml<br/>(NEW: blue_glass material,<br/>sweeper_prism object,<br/>wake_frame field)"]
    end

    subgraph "Infrastructure Layer"
        YSL["YamlSceneLoader<br/>(MODIFIED: parse wake_frame)"]
        JPS["JoltPhysicsSimulator<br/>(MODIFIED: kinematic velocity,<br/>NEW: wake_body method)"]
        MRB["MetalRenderBackend<br/>(unchanged)"]
    end

    subgraph "Application Layer"
        AR["AnimationRenderer<br/>(MODIFIED: per-body wake loop)"]
        PS["PhysicsSimulator port<br/>(MODIFIED: wake_body method)"]
    end

    subgraph "Domain Layer"
        PP["PhysicsProperties<br/>(MODIFIED: wake_frame field)"]
        AC["AnimationConfig<br/>(unchanged)"]
        BOX["Box shape<br/>(unchanged)"]
        DIEL["Dielectric material<br/>(unchanged)"]
    end

    subgraph "Core Layer"
        V3["Vec3, Point3<br/>(unchanged)"]
        Q["Quaternion<br/>(unchanged)"]
    end

    YAML -->|"parse"| YSL
    YSL -->|"PhysicsProperties<br/>with wake_frame"| PP
    YSL -->|"Box + Dielectric"| BOX
    YSL -->|"Box + Dielectric"| DIEL
    YSL -->|"AnimationConfig"| AC

    PP -->|"body desc"| AR
    AC -->|"global wake_frame"| AR
    BOX -->|"shape"| AR
    DIEL -->|"material"| AR

    AR -->|"add_body(kinematic)"| PS
    AR -->|"wake_body(id) at frame N"| PS
    AR -->|"get_transform(id)"| PS
    AR -->|"step(dt)"| PS

    PS -->|"implements"| JPS

    AR -->|"render frame"| MRB

    PP -.->|"uses"| V3
    PP -.->|"uses"| Q
```

## 2. Sequence Diagram: Sweep Animation Lifecycle

Shows the frame-by-frame lifecycle from scene load through sweep completion. Key phases: setup, pre-sweep (bowling impact), sweep start, sweep in progress, sweep complete.

```mermaid
sequenceDiagram
    participant YAML as nwave_bowling.yaml
    participant YSL as YamlSceneLoader
    participant AR as AnimationRenderer
    participant JPS as JoltPhysicsSimulator
    participant GPU as MetalRenderBackend

    note over YAML,GPU: PHASE 1: Scene Load
    YAML->>YSL: load(yaml_content)
    YSL->>YSL: parse blue_glass material (dielectric, ior=1.5, tint=[0.4,0.4,0.95])
    YSL->>YSL: parse sweeper_prism box (min=[-4,0.01,4.0], max=[4,1.5,4.3])
    YSL->>YSL: parse physics (kinematic, velocity=[0,0,-3.44], start_asleep=true, wake_frame=60)
    YSL-->>AR: SceneLoadResult (shapes, physics_props, animation_config)

    note over YAML,GPU: PHASE 2: Physics Setup
    AR->>JPS: add_body(sweeper_prism, kinematic, velocity=[0,0,-3.44])
    JPS->>JPS: create BoxShape, EMotionType::Kinematic
    JPS->>JPS: set mLinearVelocity = (0, 0, -3.44)
    JPS->>JPS: DontActivate (start_asleep=true)
    JPS-->>AR: body_id = N

    note over YAML,GPU: PHASE 3: Pre-Sweep (frames 0-19)
    loop frame 0 to 19
        AR->>JPS: get_transform(sweeper_id)
        JPS-->>AR: position unchanged (z=4.15, body sleeping)
        AR->>GPU: render frame (sweeper stationary at back of board)
        AR->>JPS: step(dt) x steps_per_frame
        note right of JPS: bowling ball moving, W blocks stationary
    end

    note over YAML,GPU: PHASE 4: Global Wake (frame 20)
    AR->>JPS: wake_all() [global wake_frame=20]
    note right of JPS: W blocks and e blocks activated<br/>Sweeper NOT affected (has per-body wake_frame=60)

    note over YAML,GPU: PHASE 5: Bowling Impact (frames 20-59)
    loop frame 20 to 59
        AR->>JPS: get_transform(sweeper_id)
        JPS-->>AR: position unchanged (z=4.15, still sleeping)
        AR->>GPU: render frame (bowling scatters W blocks, sweeper stationary)
        AR->>JPS: step(dt) x steps_per_frame
        note right of JPS: W blocks scattering, jelly cube bouncing
    end

    note over YAML,GPU: PHASE 6: Sweep Start (frame 60)
    AR->>AR: check per-body wake_frames
    AR->>JPS: wake_body(sweeper_id) [per-body wake_frame=60]
    JPS->>JPS: ActivateBody(sweeper_jolt_id)
    note right of JPS: sweeper begins moving at stored velocity (0, 0, -3.44)

    note over YAML,GPU: PHASE 7: Sweep In Progress (frames 60-149)
    loop frame 60 to 149
        AR->>JPS: get_transform(sweeper_id)
        JPS-->>AR: position.z decreasing (~0.057 units/frame)
        AR->>GPU: render frame (sweeper advancing, pushing debris)
        AR->>JPS: step(dt) x steps_per_frame
        note right of JPS: kinematic sweeper collides with dynamic bodies<br/>W blocks, bowling ball, e blocks, jelly cube pushed in -Z
    end

    note over YAML,GPU: PHASE 8: Animation Complete
    AR->>AR: frame 150 reached (5.0s at 30fps)
    note right of AR: All dynamic objects pushed beyond z=-4<br/>Chessboard and static letters unchanged
```

## 3. Component Boundary Diagram: What Changes vs What Does Not

```mermaid
graph LR
    subgraph "CHANGED (4 files)"
        direction TB
        C1["physics_properties.h<br/>+ wake_frame field"]
        C2["yaml_scene_loader.cpp<br/>+ parse wake_frame"]
        C3["jolt_physics_simulator.cpp<br/>+ kinematic velocity<br/>+ wake_body()"]
        C4["animation_renderer.cpp<br/>+ per-body wake check"]
    end

    subgraph "PORT CHANGED (1 file)"
        direction TB
        P1["physics_simulator.h<br/>+ wake_body() method"]
    end

    subgraph "UNCHANGED"
        direction TB
        U1["Core layer<br/>(vec3, matrix, quaternion)"]
        U2["Box shape"]
        U3["Dielectric material"]
        U4["ray_trace.metal shader"]
        U5["scene_flattener.cpp"]
        U6["bvh_flattener.cpp"]
        U7["MetalRenderBackend"]
        U8["AnimationConfig"]
    end

    C1 -->|"read by"| C2
    C1 -->|"read by"| C4
    C4 -->|"calls"| P1
    P1 -->|"implemented by"| C3
```
