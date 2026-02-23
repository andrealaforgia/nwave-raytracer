# Glass Sweeper Prism -- Requirements

## Problem Statement

A scene designer working on the nWave bowling demo wants to add a dramatic "clearing" moment to the animation: after the bowling ball scatters the green glass "W" letter blocks, a tall blue glass prism sweeps across the entire chessboard from back to front, pushing every remaining dynamic element (scattered W blocks, bowling ball, purple "e" letter blocks, pink jelly cube) off the board edge. Today, the scattered debris just sits on the board after the bowling impact, leaving the scene cluttered. The sweeper prism creates a clean, cinematic "wipe" effect that resets the board.

## Stakeholders

- **Scene Designer (primary)**: Andrea Laforgia, defines scene YAML and evaluates visual result
- **Ray Tracer Engine**: must parse, simulate, and render the new object correctly
- **Viewer/Audience**: sees the final rendered video; expects visually coherent glass refraction, smooth motion, and satisfying physics interactions

## Business Context

The nWave bowling demo is a showcase animation demonstrating the ray tracer's capabilities: physics simulation, dielectric (glass) materials, soft bodies, and dynamic interactions. Adding the sweeper prism demonstrates a new capability -- kinematic body motion -- and creates a more polished, complete animation sequence.

## Functional Requirements

### FR-1: Blue Glass Material Definition

The scene must include a new dielectric material with a blue tint for the sweeper prism, analogous to the existing `green_glass` material but with a blue color channel.

**Domain Examples:**
- Material `blue_glass` with `type: dielectric`, `ior: 1.5`, `tint: [0.4, 0.4, 0.95]` renders as a transparent blue prism that refracts light and shows blue-tinted caustics on the chessboard surface.
- When the point light at [-4, 10, 2] hits the blue glass prism, the viewer sees blue-tinted refraction patterns on the white and black chessboard tiles beneath it.
- The blue glass prism is visually distinct from the existing `green_glass` W-letter blocks, so the audience clearly sees two different glass colors in the scene.

### FR-2: Sweeper Prism Geometry

The prism is a box shape spanning the full width of the chessboard (8 units in X, from x=-4 to x=4), with narrow depth (~0.3 units in Z) and sufficient height to contact and push all objects on the board (~1.5 units in Y, from board surface y=0 to y=1.5).

**Domain Examples:**
- The prism box is defined as `min: [-4, 0, z_start]` to `max: [4, 1.5, z_start+0.3]` where z_start is the initial Z position at the back edge of the board (z ~ 4.0).
- The prism is tall enough (1.5 units) to contact the tallest letter block (W at y=0.84) and the bowling ball (center y=0.3, radius 0.3, so top at y=0.6).
- The prism is narrow in depth (0.3 units) so it reads as a thin wall or blade sweeping across, not a large block obscuring the scene.

### FR-3: Kinematic Sweep Motion

The prism moves as a kinematic physics body from the back edge of the chessboard (z ~ 4.3, just off-board) to the front edge (z ~ -4.3, just off-board) at a constant speed over a defined time window.

**Domain Examples:**
- The sweeper starts at z=4.3 (just behind the back row of the chessboard). At 30fps over the sweep window, it moves at constant velocity in the -Z direction, reaching z=-4.3 by the end of its sweep period.
- The sweep begins at approximately frame 60 (2.0 seconds into a 5-second animation), giving the bowling ball impact ~1.3 seconds to play out after wake_frame=20 (0.67s). The sweep takes approximately 2.5 seconds (75 frames) to cross the 8.6-unit distance.
- During the sweep, the prism's Z position changes linearly: z(t) = 4.3 - speed * (t - t_start). The prism does not accelerate, decelerate, bounce, or respond to collisions. It moves on a predetermined path.

### FR-4: Physics Interaction -- Pushing Dynamic Bodies

The kinematic prism collides with all dynamic bodies (W glass blocks, bowling ball, e letter blocks) and the soft body (pink jelly cube), pushing them in the -Z direction and off the board edge.

**Domain Examples:**
- When the sweeper reaches the scattered W_0 block at approximately z=0.5, it contacts the block and pushes it forward. The block slides, tumbles, and eventually falls off the front edge of the board (z < -4).
- The bowling ball (mass 6.0 kg, much heavier than the letter blocks at 0.1 kg) is pushed by the sweeper but moves more slowly due to its mass. The kinematic body exerts effectively infinite force, so even the heavy ball is displaced.
- The pink jelly cube (soft body) deforms on contact with the sweeper's flat face, then is pushed and squeezed off the board edge.

### FR-5: YAML Scene Definition

The sweeper prism and its motion must be fully definable in the scene YAML file without code changes to the YAML parser beyond supporting kinematic motion parameters.

**Domain Examples:**
- The scene YAML includes a new object entry:
  ```yaml
  - name: sweeper_prism
    type: box
    min: [-4, 0, 4.0]
    max: [4, 1.5, 4.3]
    material: blue_glass
    physics:
      body_type: kinematic
      initial_velocity: [0, 0, -3.44]
  ```
  where the velocity is calculated as distance/time = 8.6/2.5 = -3.44 units/sec.
- Alternatively, a new `motion` block could specify start/end positions and timing, but the simplest approach uses `initial_velocity` on a kinematic body that begins moving at a configured frame.

### FR-6: Timing Coordination

The sweeper must begin its motion after the bowling ball impact has played out sufficiently for the audience to see the scatter effect, but early enough to complete its sweep before the animation ends.

**Domain Examples:**
- With a 5-second animation at 30fps (150 frames), wake_frame=20 (0.67s), the bowling ball reaches the W blocks around frame 30-40 (~1.0-1.3s). The sweeper starting at frame 60 (2.0s) gives ~0.7 seconds of post-impact settling before the sweep begins.
- The sweep takes ~2.5 seconds (75 frames), so starting at frame 60 and ending at frame 135 leaves 15 frames (0.5s) of the board being clear before the animation ends.
- If the animation duration is extended (e.g., to 7 seconds), the sweep timing should be adjustable via YAML parameters without code changes.

## Non-Functional Requirements

### NFR-1: Rendering Performance
The addition of one large dielectric box should not significantly degrade per-frame render time. The existing scene already handles dielectric materials (green_glass W blocks). One additional large box adds minimal ray intersection overhead.

### NFR-2: Physics Stability
The kinematic body must not cause physics instability (tunneling, explosion, jitter) when pushing multiple dynamic bodies simultaneously. The sweep speed (~3.44 units/sec) at the physics timestep of 0.01667s means ~0.057 units per step, well within collision detection tolerances for the 0.12-unit letter blocks.

### NFR-3: Visual Coherence
The blue glass prism must refract and reflect consistently across frames as it moves. The dielectric material already handles this for static objects (green_glass W). Moving dielectric objects must produce the same visual quality.

## Constraints and Dependencies

### Existing Infrastructure
- **YAML parser** already supports `body_type: kinematic` (maps to `BodyType::KINEMATIC` in `parse_body_type()`).
- **Jolt integration** already maps `KINEMATIC` to `JPH::EMotionType::Kinematic` in `map_body_type_to_motion()`.
- **Animation renderer** already treats kinematic bodies as movable (`is_movable_body()` returns true for `KINEMATIC`).

### Gap: Kinematic Body Velocity/Motion Driver
- **Critical gap**: The `PhysicsSimulator` interface has no method to set linear velocity on a kinematic body per-frame. Jolt requires `BodyInterface::SetLinearVelocity()` or `MoveKinematic()` for kinematic bodies.
- `initial_velocity` in `PhysicsProperties` is only applied to `DYNAMIC` bodies (line 225-229 of `jolt_physics_simulator.cpp`). Kinematic bodies currently get created but never move.
- **Resolution needed**: Either (a) extend `PhysicsSimulator` with a `set_linear_velocity(body_id, velocity)` method and apply `initial_velocity` for kinematic bodies during creation, or (b) introduce a frame-based motion callback.

### Gap: Delayed Activation / Start Frame
- The sweeper should not start moving at frame 0. It needs a mechanism to begin motion at a specific frame (e.g., frame 60).
- **Possible approaches**: (a) `wake_frame` per-body in physics properties, (b) `start_frame` field in physics properties, (c) the sweeper starts asleep and a new per-object wake mechanism triggers it.

### Existing Scene Layout
- Chessboard: x in [-4, 4], z in [-4, 4], y surface at 0.0.
- Letters positioned around z=0.5, x from -1.74 to 1.62.
- Bowling ball starts at [-2.85, 0.3, 2.7] with velocity [3.54, 0, -3.54].
- Camera orbits from [2, 3, 6] counterclockwise; the sweep from z=4 to z=-4 moves toward the camera, creating a visually dramatic "approaching wall" effect.
