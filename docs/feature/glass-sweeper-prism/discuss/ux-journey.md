# UX Journey: Glass Sweeper Prism

## Feature Summary

A blue glass rectangular prism, spanning the full width of the chessboard (8 units),
sweeps slowly from the back edge to the front edge, pushing all scene elements
(letter blocks, jelly cube, bowling ball) off the board. The prism is a dielectric
material, refracting and tinting light blue as it moves.

---

## Journey 1: Viewer's Visual Storytelling Arc (watching the rendered video)

### Act Structure

```
TIME (5s video, 150 frames @ 30fps)
=================================================================

Act 1: "The Calm"          frames 0-29   (0.0s - 1.0s)
Act 2: "The Bowling Hit"   frames 20-60  (0.67s - 2.0s)
Act 3: "The Sweep Begins"  frames 45-90  (1.5s - 3.0s)
Act 4: "The Sweep Clears"  frames 90-130 (3.0s - 4.3s)
Act 5: "Clean Board"       frames 130-150 (4.3s - 5.0s)
```

### Step-by-Step

```
STEP 1: "The Calm" (frames 0-20)
  Viewer sees: Chessboard with "nWave" letters. Jelly cube falling.
               Bowling ball approaching. Prism NOT YET VISIBLE --
               it sits just behind the back edge at z = -4.5.
  Emotion:     Familiar anticipation -- the bowling scene they know.

STEP 2: "The Bowling Hit" (frames 20-60)
  Viewer sees: Ball smashes into W, glass blocks scatter. Jelly
               cube bounces on 'e'. Camera rotates. Meanwhile...
               the prism begins its slow entry from the back edge.
               First visible glint of blue glass at z ~ -4.0.
  Emotion:     Excitement from the familiar bowling action, then
               curiosity -- "what is that blue shape at the back?"

STEP 3: "The Sweep Begins" (frames 45-90)
  Viewer sees: The prism is now clearly visible -- a tall, wide
               blue glass wall moving steadily forward. It reaches
               the 'n' letter blocks and pushes them. Refraction
               through the glass distorts the scene behind it.
               Static letters (n, a, v) get shoved off the board.
               Dynamic letters (W debris, e blocks) are pushed too.
  Emotion:     Awe at the visual effect (glass refraction + physics)
               combined with satisfaction of the "clean sweep."

STEP 4: "The Sweep Clears" (frames 90-130)
  Viewer sees: Prism reaches the front half of the board. Remaining
               debris, jelly cube, and bowling ball are all pushed
               forward off the front edge. Objects fall off z = +4.
               The glass prism itself refracts the emptying board.
  Emotion:     Satisfying inevitability -- everything gets cleared.

STEP 5: "Clean Board" (frames 130-150)
  Viewer sees: Prism exits the front edge. Chessboard is empty and
               pristine. Camera continues its orbit over a clean board.
  Emotion:     Calm resolution. "That was cool."
```

### Top-Down Motion Diagram

```
  z = -4.5 (behind board)          z = +4.5 (in front of board)
     |                                    |
     V  PRISM START                       V  PRISM END

     +------------------------------------+
     |         8x8 CHESSBOARD             |
     |                                    |
     |  n  W  a  v  e    (at z ~ 0.5)    |
     |         *ball*     (approaching)   |
     |                                    |
     +------------------------------------+
   z=-4                                 z=+4

     PRISM moves in +Z direction ======>

     Prism dimensions (seen from above):
     x: [-4, +4]   (full board width, 8 units)
     z: [pos, pos+0.5]  (0.5 units deep)

     Prism dimensions (seen from side):
     y: [0, 2.0]   (2 units tall, well above letters ~0.84 high)
```

---

## Journey 2: Author's YAML Configuration Experience

### Step-by-Step

```
STEP 1: Define the blue glass material
  Author adds: A new material entry in the materials section.
  Feels:       Straightforward -- same pattern as existing green_glass.

  YAML pattern:
    - name: blue_glass
      type: dielectric
      ior: 1.5
      tint: [0.3, 0.5, 0.95]

STEP 2: Define the prism object
  Author adds: A box with kinematic physics and a linear velocity.
  Feels:       Natural extension -- kinematic body type already exists
               in the schema, and initial_velocity already works.

  YAML pattern:
    - name: glass_sweeper
      type: box
      min: [-4.0, 0.0, -5.0]
      max: [4.0, 2.0, -4.5]
      material: blue_glass
      physics:
        body_type: kinematic
        initial_velocity: [0.0, 0.0, 1.8]
        mass: 100.0
        friction: 0.5
        restitution: 0.1

  Key decisions:
    - Start position: z from -5.0 to -4.5 (just behind the board)
    - Height: y from 0.0 to 2.0 (tall enough to push everything)
    - Width: x from -4.0 to +4.0 (full board width)
    - Velocity: +Z at 1.8 units/sec traverses 9.5 units in ~5.3s
    - Mass: 100kg makes it an unstoppable force
    - body_type: kinematic means physics does not affect it --
      it moves at constant velocity and pushes dynamic objects

STEP 3: Verify the scene renders
  Author runs: nwave render --scene scenes/nwave_bowling.yaml --physics-animate
  Feels:       Confident -- no new YAML syntax to learn, just a new
               object with existing property types.
```

### YAML Authoring Mental Model

```
  What the author thinks:                What the system does:

  "I want a glass wall"         -->  dielectric material + box shape
  "that moves steadily"         -->  kinematic body + initial_velocity
  "and pushes things"           -->  Jolt physics handles collisions
  "from back to front"          -->  positive Z velocity, start behind board
  "and can't be stopped"        -->  kinematic body ignores forces from others
```

---

## Journey 3: Rendering Pipeline (how the system processes the sweep)

### Data Flow

```
YAML Scene File
    |
    v
YamlSceneLoader::load()
    |-- Parses "blue_glass" material -> Dielectric(1.5, [0.3,0.5,0.95])
    |-- Parses "glass_sweeper" box -> Box([-4,0,-5], [4,2,-4.5])
    |-- Parses physics: kinematic + velocity -> PhysicsProperties
    |
    v
AnimationRenderer::render()
    |-- map_shape_to_body_desc() creates PhysicsBodyDesc
    |       shape_type: BOX, dimensions: [4, 1, 0.25]
    |       position: [0, 1, -4.75] (center of box)
    |       body_type: KINEMATIC
    |
    |-- physics_->add_body(desc) -> JoltPhysicsSimulator
    |       Creates JPH body with EMotionType::Kinematic
    |       ** PROBLEM: initial_velocity is only set for DYNAMIC bodies **
    |       ** FIX NEEDED: also set velocity for KINEMATIC bodies **
    |
    |-- Wraps in TransformedShape (is_movable_body returns true for KINEMATIC)
    |
    v
Per-Frame Loop (150 frames):
    |
    |-- get_transform(sweeper_body_id)
    |       Jolt returns updated position (kinematic moves at set velocity)
    |       Position advances: z = -4.75 + frame * dt * 1.8
    |
    |-- TransformedShape gets relative transform matrix
    |       Prism box renders at new position
    |
    |-- physics_->step(dt)
    |       Jolt computes collisions between kinematic prism and dynamic objects
    |       Dynamic objects (W blocks, e blocks, ball, jelly) get pushed
    |
    |-- Ray tracer renders the frame
    |       Rays hitting the prism: refraction through blue dielectric
    |       Rays hitting pushed objects: at their new physics positions
    |
    v
Output: 150 PPM frames -> ffmpeg -> mp4
```

### Technical Gap Analysis

```
GAP 1: Kinematic velocity not set in add_body()
  WHERE:  jolt_physics_simulator.cpp, line 225-229
  WHAT:   initial_velocity is only applied when body_type == DYNAMIC
  FIX:    Also apply for KINEMATIC bodies:
            if (desc.properties.body_type == BodyType::DYNAMIC ||
                desc.properties.body_type == BodyType::KINEMATIC)

  WHY IT MATTERS: Without this fix, the kinematic body sits still.
  Jolt kinematic bodies move only when given a velocity -- they do
  not respond to gravity or forces, but they DO respect their set
  linear velocity.

  ALTERNATIVE: Use Jolt's MoveKinematic() API each frame to set
  target position directly. This would require:
    - Adding set_kinematic_target(int body_id, Point3 pos, double dt)
      to PhysicsSimulator interface
    - Computing target position in AnimationRenderer per frame
    - More complex but more controllable (allows non-linear paths)

  RECOMMENDATION: Start with the simple velocity fix. If non-linear
  motion is needed later (e.g., prism accelerates), upgrade to
  MoveKinematic() then.

GAP 2: Kinematic activation
  WHERE:  jolt_physics_simulator.cpp, line 236
  WHAT:   Kinematic bodies without start_asleep should be activated
  STATUS: Already handled correctly -- activation is DontActivate only
          for STATIC or start_asleep. Kinematic without start_asleep
          gets Activate. OK.

GAP 3: No gaps in YAML parsing
  WHERE:  yaml_scene_loader.cpp
  WHAT:   body_type: kinematic already parsed correctly via
          parse_body_type(). initial_velocity already parsed.
  STATUS: No changes needed.

GAP 4: Animation duration may need extending
  WHERE:  scenes/nwave_bowling.yaml, animation.duration
  WHAT:   At velocity 1.8 units/sec, traversing from z=-5.0 to z=+5.0
          takes 10/1.8 = 5.56s. Current duration is 5.0s.
  FIX:    Either increase duration to 6.0s or increase velocity to
          2.0 units/sec (traverse in 5.0s exactly).
  RECOMMENDATION: Increase velocity to 2.0 for clean 5s timing, or
          extend duration to 6s for a more leisurely sweep.
```

---

## Shared Artifacts Registry

| Artifact | Source | Consumed By | Notes |
|---|---|---|---|
| `blue_glass` material name | YAML materials section | objects[].material | Must match exactly |
| `glass_sweeper` object name | YAML objects section | (debugging/logging only) | Optional but helpful |
| Prism x-range `[-4, 4]` | Prism box min/max x | Must match chessboard x-range | If board size changes, prism must too |
| Prism start z `-5.0` | Prism box min z | Must be <= board back edge (-4) | Off-screen at frame 0 |
| Prism velocity z `1.8` | physics.initial_velocity[2] | Determines sweep timing | Must traverse board within animation.duration |
| Prism height `2.0` | Prism box max y | Must exceed tallest scene object | Letters are ~0.84 high, jelly lands ~0.6 high |
| `animation.duration` | YAML animation section | Prism must exit board within this time | Velocity * duration > total_z_travel |
| `body_type: kinematic` | YAML physics section | JoltPhysicsSimulator | Requires velocity fix (Gap 1) |

---

## Quality Checklist

- [x] Happy path: prism defined, moves, pushes objects, exits board
- [x] Emotional arc: anticipation -> curiosity -> awe -> satisfaction -> calm
- [x] Shared artifacts: all cross-references documented
- [x] Technical gaps: identified with specific file/line references
- [x] YAML authoring: no new syntax needed, uses existing primitives
- [x] Error paths acknowledged: velocity fix needed, timing coordination needed
