# Glass Sweeper Prism -- User Stories

Stories are sliced by user outcome using Elephant Carpaccio. Each story is independently demonstrable, delivers incremental value, and targets 1-3 days of effort.

---

## US-001: Blue Glass Material in Scene YAML

### Problem (The Pain)
Andrea Laforgia is a scene designer building the nWave bowling demo. He wants to add a blue glass object to the scene but the scene YAML only defines `green_glass` as a dielectric material. He needs a second glass material with a blue tint so the sweeper prism is visually distinct from the green W-letter blocks.

### Who (The User)
- Scene designer who edits `nwave_bowling.yaml` directly
- Evaluates rendered output visually for correct glass appearance
- Needs the material to work with existing dielectric rendering pipeline

### Solution (What We Build)
A new `blue_glass` material entry in the scene YAML that the existing YAML parser and dielectric material system can load and render without code changes.

### Domain Examples

**Example 1: Blue glass material parses correctly**
Andrea adds `blue_glass` to the materials section of `nwave_bowling.yaml` with `type: dielectric`, `ior: 1.5`, `tint: [0.4, 0.4, 0.95]`. The YAML loader creates a `Dielectric` material with IOR 1.5 and tint (0.4, 0.4, 0.95). No code changes needed -- the existing parser handles this.

**Example 2: Blue glass renders distinct from green glass**
A test scene with a green glass box and a blue glass box side by side, lit by the same point light, renders two clearly different-colored transparent boxes. The green box tints refracted light greenish; the blue box tints refracted light bluish.

**Example 3: Blue glass with same IOR as green glass**
Both `green_glass` (IOR 1.5) and `blue_glass` (IOR 1.5) refract at the same angles but produce different color tints. If Andrea accidentally sets the tint to [0.4, 0.95, 0.4] (same as green), the two materials would look identical -- the tint is what distinguishes them.

### UAT Scenarios (BDD)

**Scenario 1: Blue glass material loads from YAML**
```
Given the scene YAML contains a material "blue_glass" with type "dielectric", ior 1.5, tint [0.4, 0.4, 0.95]
When the YAML scene loader parses the materials section
Then a Dielectric material named "blue_glass" exists with IOR 1.5 and tint (0.4, 0.4, 0.95)
```

**Scenario 2: Object references blue glass material**
```
Given the scene YAML contains a box object with material "blue_glass"
And the materials section defines "blue_glass" as a dielectric
When the YAML scene loader parses the objects section
Then the box shape is created with the blue_glass Dielectric material pointer
```

**Scenario 3: Blue glass renders with blue tint**
```
Given a scene with a blue_glass box at the origin and a point light above it
When a ray passes through the blue_glass box
Then the transmitted ray color is attenuated by the blue tint (0.4, 0.4, 0.95), making it appear blue
```

### Acceptance Criteria
- [ ] `blue_glass` material with `type: dielectric`, `ior: 1.5`, `tint: [0.4, 0.4, 0.95]` parses without error
- [ ] A box referencing `blue_glass` renders as transparent with blue-tinted refraction
- [ ] `blue_glass` is visually distinguishable from `green_glass` in the same scene

### Technical Notes
- Zero code changes expected. This is purely a YAML content addition.
- The existing `create_dielectric()` function in `yaml_scene_loader.cpp` already handles `ior` and `tint` fields.

---

## US-002: Sweeper Prism Box Appears in Scene

### Problem (The Pain)
Andrea Laforgia wants to place a tall, board-wide rectangular prism on the chessboard that will serve as the sweeper. Currently, there is no such object in the scene. Without adding the prism geometry, there is nothing to sweep the debris.

### Who (The User)
- Scene designer positioning objects via min/max coordinates in YAML
- Needs the prism to span the full board width (8 units) and be tall enough to contact all objects
- Evaluates correct placement by rendering frame 0

### Solution (What We Build)
A new box object in the scene YAML with dimensions spanning the chessboard width, positioned at the back edge of the board, using the `blue_glass` material.

### Domain Examples

**Example 1: Prism spans full board width**
Andrea defines the sweeper as `min: [-4, 0, 4.0], max: [4, 1.5, 4.3]`. The box stretches from x=-4 to x=4 (8 units, matching the chessboard), sits on the board surface (y=0 to y=1.5), and is 0.3 units deep in Z. At frame 0, it appears as a thin blue glass wall at the back edge of the board.

**Example 2: Prism is tall enough to contact all objects**
The tallest dynamic object is the W letter at y=0.84. The bowling ball top is at y=0.6. The jelly cube falls from y=3.0 but settles around y=0.6. With the prism top at y=1.5, it clears all objects with margin.

**Example 3: Prism positioned just behind the board**
With `min_z: 4.0` and `max_z: 4.3`, the prism's front face is at z=4.0 (aligned with the back edge of the last chessboard row) and its back face extends slightly beyond the board. At frame 0, the camera at [2, 3, 6] looks toward [0, 0.3, 0.5], so the prism appears behind the letters but in the camera's field of view.

### UAT Scenarios (BDD)

**Scenario 1: Sweeper prism renders at correct position**
```
Given the scene YAML contains a box "sweeper_prism" with min [-4, 0, 4.0] and max [4, 1.5, 4.3] using material "blue_glass"
When frame 0 is rendered with the camera at [2, 3, 6] looking at [0, 0.3, 0.5]
Then a blue-tinted transparent rectangular wall is visible at the back of the chessboard
```

**Scenario 2: Sweeper prism spans full board width**
```
Given the sweeper_prism box has min_x=-4 and max_x=4
When viewed from above
Then the prism extends from the left edge to the right edge of the 8x8 chessboard
```

**Scenario 3: Sweeper prism is taller than all dynamic objects**
```
Given the sweeper_prism has max_y=1.5
And the tallest dynamic object (W_0 block) has max_y=0.84
And the bowling ball top is at y=0.6
When the sweeper moves through the scene
Then its face contacts all dynamic objects because its height exceeds their maximum Y positions
```

### Acceptance Criteria
- [ ] Sweeper prism renders as a blue glass box at the back edge of the chessboard in frame 0
- [ ] Prism spans full board width (x=-4 to x=4, 8 units)
- [ ] Prism height (1.5 units) exceeds the tallest dynamic object (W block at y=0.84)
- [ ] Prism is narrow in depth (0.3 units) to read as a wall, not a block

### Technical Notes
- Zero code changes expected. This is a YAML content addition.
- The prism will initially be static (no physics). Kinematic motion is added in US-003.

---

## US-003: Kinematic Velocity for Sweeper Motion

### Problem (The Pain)
Andrea Laforgia has placed the blue glass prism at the back of the chessboard, but it just sits there. He needs the prism to move steadily across the board from back (z=4.3) to front (z=-4.3), acting as an unstoppable wall that pushes everything in its path. The existing physics system supports `body_type: kinematic` in the YAML, but kinematic bodies do not actually move because `initial_velocity` is only applied to dynamic bodies.

### Who (The User)
- Scene designer who expects kinematic bodies to move at their specified velocity
- Needs the motion to be physics-driven so the prism collides with and pushes other objects
- Must be able to control speed via YAML without recompiling

### Solution (What We Build)
Extend the physics body creation so that kinematic bodies also receive their `initial_velocity`. When the Jolt physics simulator creates a kinematic body, it applies `SetLinearVelocity` so the body moves at constant velocity through the physics world, colliding with dynamic and soft bodies.

### Domain Examples

**Example 1: Sweeper moves at constant velocity**
Andrea sets `initial_velocity: [0, 0, -3.44]` on the sweeper_prism with `body_type: kinematic`. The physics simulator creates a kinematic body and sets its linear velocity to (0, 0, -3.44). Each physics step, the prism advances ~0.057 units in -Z. After 2.5 seconds (75 frames at 30fps), it has traveled 8.6 units from z=4.3 to z=-4.3.

**Example 2: Kinematic body is not affected by collisions**
When the sweeper_prism contacts the 6.0 kg bowling ball, the prism does not slow down or deflect. It continues at constant velocity. The bowling ball, however, is pushed in the -Z direction because kinematic bodies in Jolt act as immovable movers.

**Example 3: Kinematic body with zero velocity stays put**
If Andrea sets `initial_velocity: [0, 0, 0]` on a kinematic body, it does not move. This is the default behavior and serves as a static obstacle that can later be activated.

### UAT Scenarios (BDD)

**Scenario 1: Kinematic body receives initial velocity**
```
Given a box with body_type "kinematic" and initial_velocity [0, 0, -3.44]
When the physics simulator creates the body
Then the Jolt body has linear velocity (0, 0, -3.44)
And the body's motion type is Kinematic
```

**Scenario 2: Kinematic body moves at constant velocity through physics steps**
```
Given a kinematic box at position (0, 0.75, 4.15) with velocity [0, 0, -3.44]
When the physics simulator steps 150 times at dt=0.01667
Then the box position Z decreases by approximately 8.6 units (from 4.15 to approximately -4.45)
And the box position X and Y remain unchanged
```

**Scenario 3: Kinematic body pushes dynamic body on contact**
```
Given a kinematic box moving at velocity [0, 0, -3.44]
And a dynamic box (mass 0.1) at rest in the kinematic body's path
When the kinematic box reaches the dynamic box
Then the dynamic box is displaced in the -Z direction
And the kinematic box continues at velocity [0, 0, -3.44] without slowing
```

**Scenario 4: Kinematic body with zero velocity does not move**
```
Given a kinematic box at position (0, 0, 0) with initial_velocity [0, 0, 0]
When the physics simulator steps 100 times at dt=0.01667
Then the box remains at position (0, 0, 0)
```

### Acceptance Criteria
- [ ] Kinematic bodies created with non-zero `initial_velocity` move at that velocity
- [ ] Kinematic body position changes linearly with time (constant velocity, no gravity)
- [ ] Kinematic body collides with and displaces dynamic bodies
- [ ] Kinematic body is not displaced or slowed by collisions with dynamic bodies
- [ ] Kinematic body with zero initial velocity remains stationary

### Technical Notes
- Requires code change in `JoltPhysicsSimulator::add_body()`: apply `SetLinearVelocity` for kinematic bodies (currently only done for dynamic).
- Jolt's kinematic bodies do not respond to gravity by default, which is the desired behavior.
- Dependency: `PhysicsSimulator` interface may need a `set_linear_velocity(int body_id, Vec3 velocity)` method if per-frame velocity control is needed later, but for this story, setting velocity at creation time suffices.

---

## US-004: Delayed Sweep Start via Per-Body Wake Frame

### Problem (The Pain)
Andrea Laforgia has the sweeper prism moving from frame 0, but he needs it to wait until the bowling ball impact has played out (~2 seconds) before starting its sweep. The existing `wake_frame` in `AnimationConfig` wakes ALL sleeping bodies at once, but Andrea needs the sweeper to wake independently at a different time than the letter blocks.

### Who (The User)
- Scene designer who choreographs multi-phase animation sequences
- Needs fine-grained control over when individual objects begin moving
- Must coordinate sweeper timing with bowling impact timing via YAML

### Solution (What We Build)
Add a `wake_frame` field to `PhysicsProperties` (per-object, in the YAML `physics` block) so individual objects can be set to `start_asleep: true` and then wake at their own designated frame. The animation renderer checks each body's per-object `wake_frame` during the render loop.

### Domain Examples

**Example 1: Sweeper wakes at frame 60**
Andrea sets `start_asleep: true` and `wake_frame: 60` on the sweeper_prism. For frames 0-59, the prism sits motionless at z=4.15. At frame 60 (2.0 seconds), the animation renderer wakes the prism. It begins moving at its initial_velocity [0, 0, -3.44] and sweeps across the board.

**Example 2: Letter blocks still wake via global wake_frame**
The existing W and e letter blocks have `start_asleep: true` but no per-object `wake_frame`. They continue to wake at the global `wake_frame: 20` from `AnimationConfig`, preserving existing behavior.

**Example 3: Object with no wake_frame is unaffected**
The bowling ball has no `start_asleep` or `wake_frame` settings. It begins moving immediately with its initial_velocity at frame 0. The per-object wake_frame feature does not change any existing object behavior.

### UAT Scenarios (BDD)

**Scenario 1: Per-object wake_frame parsed from YAML**
```
Given the scene YAML contains a kinematic box with physics wake_frame 60 and start_asleep true
When the YAML scene loader parses the physics properties
Then the PhysicsProperties for that object has wake_frame=60 and start_asleep=true
```

**Scenario 2: Sleeping kinematic body does not move before wake frame**
```
Given a kinematic sweeper_prism at z=4.15 with start_asleep=true, wake_frame=60, velocity [0, 0, -3.44]
When the animation renders frames 0 through 59
Then the sweeper_prism remains at z=4.15 (not moving)
```

**Scenario 3: Sleeping kinematic body begins moving at wake frame**
```
Given a kinematic sweeper_prism at z=4.15 with start_asleep=true, wake_frame=60, velocity [0, 0, -3.44]
When the animation reaches frame 60
Then the sweeper_prism is activated
And by frame 61 the sweeper_prism has moved to approximately z=4.09 (one frame of motion)
```

**Scenario 4: Global wake_frame still works for objects without per-object wake_frame**
```
Given W_0 block has start_asleep=true but no per-object wake_frame
And animation config has global wake_frame=20
When the animation reaches frame 20
Then W_0 is activated (existing behavior unchanged)
```

**Scenario 5: Object without start_asleep ignores wake_frame**
```
Given the bowling_ball has no start_asleep field and no wake_frame
When the animation starts
Then the bowling_ball moves immediately from frame 0 with its initial_velocity
```

### Acceptance Criteria
- [ ] `wake_frame` field in YAML `physics` block is parsed into `PhysicsProperties`
- [ ] Kinematic body with `start_asleep: true` and `wake_frame: 60` does not move before frame 60
- [ ] Kinematic body begins moving at its `initial_velocity` starting at frame 60
- [ ] Objects without per-object `wake_frame` continue to use global `wake_frame` behavior
- [ ] Objects without `start_asleep` are unaffected by the feature

### Technical Notes
- Requires adding `std::optional<int> wake_frame` to `PhysicsProperties`.
- Requires YAML parser change to read `wake_frame` from the `physics` block.
- Requires animation renderer change: in the per-frame loop, check each body's per-object `wake_frame` and activate if frame matches.
- The `PhysicsSimulator` interface may need a `wake_body(int body_id)` method (or reuse activation logic).
- Must not break existing global `wake_frame` behavior.

---

## US-005: Sweeper Pushes All Dynamic Objects Off the Board

### Problem (The Pain)
Andrea Laforgia has the sweeper moving and the physics system handling collisions, but he needs to verify the end-to-end visual result: the sweeper actually clears every dynamic object (scattered W blocks, bowling ball, e blocks, jelly cube) off the chessboard edge. If any object gets stuck, passes through, or clips, the cinematic effect is ruined.

### Who (The User)
- Scene designer evaluating the final rendered animation
- Expects all dynamic debris to be visibly pushed off all four edges (primarily -Z, but also +X/-X from deflection)
- Judges success by watching the rendered video

### Solution (What We Build)
Integrate the sweeper prism into the full `nwave_bowling.yaml` scene with correct timing, velocity, and positioning so that the rendered animation shows the complete choreography: bowling impact, debris scatter, sweeper entrance, board cleared.

### Domain Examples

**Example 1: Green glass W blocks pushed off front edge**
The scattered W blocks (W_0 through W_17, each 0.1 kg) are spread around z=-1 to z=1 after the bowling impact. When the sweeper reaches them around frame 80-90, it pushes all blocks toward z=-4. The light blocks accelerate quickly and tumble off the front edge of the board.

**Example 2: Heavy bowling ball pushed off board**
The bowling ball (6.0 kg) has rolled to approximately z=-2 after its initial trajectory. When the sweeper contacts it around frame 85, the ball is pushed in -Z. Despite its weight, the kinematic sweeper moves it steadily. The ball rolls off the front edge by approximately frame 110.

**Example 3: Jelly cube deforms and is pushed off**
The pink jelly cube (soft body) has settled on or near the "e" letter area around z=0.5. When the sweeper contacts it, the jelly deforms against the flat glass face, then is pushed and eventually squeezed off the board. The deformation is visible in the rendered frames due to the soft body mesh updating each frame.

### UAT Scenarios (BDD)

**Scenario 1: W blocks cleared from board**
```
Given the nwave_bowling scene with sweeper_prism starting sweep at frame 60
And W blocks scattered by bowling impact between frames 30-50
When the animation completes at frame 150
Then all W blocks (W_0 through W_17) have Z positions less than -4.0 (off the front edge)
```

**Scenario 2: Bowling ball cleared from board**
```
Given the bowling ball at rest around z=-2 after its initial trajectory
And the sweeper_prism reaches z=-2 at approximately frame 95
When the animation reaches frame 130
Then the bowling ball center Z position is less than -4.3 (fully off the board)
```

**Scenario 3: Jelly cube cleared from board**
```
Given the pink jelly cube settled near z=0.5 on the "e" letters
And the sweeper reaches z=0.5 at approximately frame 80
When the animation reaches frame 120
Then the jelly cube centroid Z position is less than -4.0 (off the board)
```

**Scenario 4: Chessboard tiles remain in place**
```
Given the chessboard tiles are static bodies (body_type: static)
When the sweeper_prism passes over them
Then all 64 chessboard tile positions are unchanged
And the ground_floor position is unchanged
```

**Scenario 5: Static letters remain in place**
```
Given the "n", "a", and "v" letters are static bodies (no physics block)
When the sweeper_prism passes through their Z positions
Then the static letter blocks remain at their original positions
And the sweeper passes through them (no collision with static-on-static)
```

### Acceptance Criteria
- [ ] All dynamic W blocks are pushed beyond z=-4 by the end of the animation
- [ ] The bowling ball is pushed beyond z=-4 by the end of the animation
- [ ] The jelly cube is pushed beyond z=-4 by the end of the animation
- [ ] All purple "e" blocks are pushed beyond z=-4 by the end of the animation
- [ ] Static objects (chessboard, ground, static letters) remain in their original positions
- [ ] The sweeper prism is visible as a blue glass wall in the rendered frames
- [ ] No physics instability (exploding objects, tunneling, jitter) during the sweep

### Technical Notes
- This is an integration/verification story -- no new code, but validates US-001 through US-004 working together.
- May require tuning: sweeper speed, start frame, prism height, or friction values.
- Rendering should be verified visually on a subset of frames (e.g., frames 0, 30, 60, 90, 120, 150).
- Static letters (n, a, v) have no `physics` block and default to `STATIC`, so the sweeper (placed on `LAYER_DYNAMIC`) will not collide with them per the `ObjectLayerPairFilter` (static-vs-static collisions are filtered out). This is the desired behavior -- only dynamic objects get pushed.
- Dependency: US-001, US-002, US-003, US-004 must all be complete.
