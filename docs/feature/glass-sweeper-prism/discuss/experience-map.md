# Experience Map: Glass Sweeper Prism

## Overview

Two distinct user experiences exist for this feature: the **scene author** who
configures the prism in YAML, and the **viewer** who watches the rendered video.
This map covers both, with emphasis on where they intersect (the author's
configuration choices directly shape the viewer's emotional experience).

---

## Experience 1: Scene Author

### Persona

A developer who already knows the nWave YAML scene format. They have configured
the existing bowling scene and want to add the sweeper prism as a finishing touch.

### Journey Map

```
Phase       | Action                  | Thinking              | Feeling
------------|-------------------------|-----------------------|------------------
ORIENT      | Open nwave_bowling.yaml | "Where do materials   | Confident --
            | Review existing scene   |  and objects go?"     | familiar format
            |                         |                       |
MATERIAL    | Add blue_glass material | "Same as green_glass  | Easy -- copy
            | type: dielectric        |  but with blue tint"  | and modify
            | ior: 1.5               |                       |
            | tint: [0.3, 0.5, 0.95] |                       |
            |                         |                       |
PLACEMENT   | Add glass_sweeper box   | "How big? Where to    | Slightly
            | Decide dimensions       |  start? How fast?"    | uncertain --
            | min: [-4, 0, -5]        |                       | needs mental
            | max: [4, 2, -4.5]       |                       | model of coords
            |                         |                       |
PHYSICS     | Set body_type: kinematic| "Kinematic means it   | Confident if
            | Set initial_velocity    |  moves but can't be   | they know the
            |   [0, 0, 1.8]           |  pushed back"         | physics model
            |                         |                       |
TIMING      | Check velocity vs       | "Will it traverse the | Needs mental
            | animation duration      |  board in time? Let   | arithmetic:
            |                         |  me calculate..."     | distance / vel
            |                         |                       |
RENDER      | Run nwave render        | "Did it work? Let me  | Anticipation,
            | --physics-animate       |  watch the video"     | then delight
            |                         |                       | or frustration
```

### Key Decision Points

```
DECISION 1: Prism dimensions
  +--------------------------------------------------+
  | Trade-off: Height                                 |
  |                                                   |
  |  Too short (y < 0.84):  Blocks fly over the prism |
  |  Too tall (y > 3.0):    Looks disproportionate    |
  |  Sweet spot (y = 2.0):  Clears everything, looks  |
  |                         like a deliberate wall     |
  +--------------------------------------------------+

DECISION 2: Sweep velocity
  +--------------------------------------------------+
  | Trade-off: Speed                                  |
  |                                                   |
  |  Too slow (< 1.0):  Boring, video too long        |
  |  Too fast (> 3.0):  Objects fly violently,         |
  |                     glass refraction hard to see   |
  |  Sweet spot (1.5-2.0): Stately sweep, objects     |
  |                     tumble satisfyingly, glass     |
  |                     refraction visible             |
  +--------------------------------------------------+

DECISION 3: Prism depth (z-thickness)
  +--------------------------------------------------+
  | Trade-off: Depth                                  |
  |                                                   |
  |  Too thin (0.1):   Fragile look, less refraction  |
  |  Too thick (2.0):  Hides too much of the scene    |
  |  Sweet spot (0.5): Visible glass slab, good       |
  |                    refraction depth, does not      |
  |                    dominate the frame              |
  +--------------------------------------------------+
```

### Error Recovery

```
PROBLEM: "The prism doesn't move"
  CAUSE:  Kinematic velocity not applied (Gap 1 from ux-journey.md)
  SYMPTOM: Prism renders at start position every frame
  AUTHOR EXPERIENCE: Confusion -> check YAML -> looks correct -> frustration
  FIX: Code fix in jolt_physics_simulator.cpp (apply velocity for kinematic)

PROBLEM: "Objects don't get pushed"
  CAUSE:  Prism is static instead of kinematic (typo in body_type)
  SYMPTOM: Prism sits still, objects pass through or pile up
  AUTHOR EXPERIENCE: Confusion -> re-read YAML -> spot typo -> fix

PROBLEM: "Prism doesn't clear the board in time"
  CAUSE:  Velocity too slow for animation duration
  SYMPTOM: Video ends with prism still on the board
  AUTHOR EXPERIENCE: Mild frustration -> recalculate -> adjust velocity or duration

PROBLEM: "Objects fly too violently"
  CAUSE:  Mass too high or velocity too fast
  SYMPTOM: Blocks launch into the air instead of sliding off
  AUTHOR EXPERIENCE: Amusing but not desired -> reduce mass or velocity
```

---

## Experience 2: Video Viewer

### Emotional Arc

```
Emotion
  ^
  |                                    ***
  |                                  **   **
  |                  ***           **       *
  |                **   *        **         *
  |              **      *     **           *
  |   ***      **        *   **             *
  |  *   *   **           ***               *
  | *     ***                                *
  |*                                          **
  +---------------------------------------------->
  0s     1s     2s     3s     4s     5s     Time

  Legend:
  0.0-0.7s  Gentle anticipation (familiar scene setup)
  0.7-1.5s  Excitement (bowling hit) + curiosity (blue glint at back)
  1.5-3.0s  Rising awe (glass wall advances, refraction visible)
  3.0-4.3s  Peak satisfaction (everything swept clean)
  4.3-5.0s  Calm resolution (empty board, orbit continues)
```

### Visual Moments (Keyframes)

```
FRAME 0 (t=0.0s): THE SETUP
+-----------------------------------------------+
|                                      [camera]  |
|     ____________________________________       |
|    |  . . . . . . . .  |  chessboard   |       |
|    |  n W a v e         |              |       |
|    |        *ball -->   |              |       |
|    |____________________|______________|       |
|                                                |
|    [prism hidden behind board at z=-5]         |
+-----------------------------------------------+
Viewer: "Ah, the bowling scene."


FRAME 45 (t=1.5s): THE REVEAL
+-----------------------------------------------+
|                                                |
|     ____________________________________       |
|    |####|  <- BLUE GLASS PRISM           |     |
|    |####|                                |     |
|    |####| n [W debris]  a  v  e  *ball*  |     |
|    |____|________________________________|     |
|     ^^^^                                       |
|     Prism entering from back edge              |
+-----------------------------------------------+
Viewer: "What is that blue wall?"
Visual: Light refracts through glass, scene behind
        is tinted blue and slightly distorted.


FRAME 75 (t=2.5s): THE PUSH
+-----------------------------------------------+
|                                                |
|     ____________________________________       |
|    |         |####|                      |     |
|    |  stuff  |####| <-- prism mid-board  |     |
|    |  piling |####|                      |     |
|    |_________|____|______________________|     |
|                                                |
+-----------------------------------------------+
Viewer: "It's pushing everything!"
Visual: Letters and debris piling up against the
        front face of the prism, then spilling over
        the board edges. Refraction shows the
        empty board behind the glass.


FRAME 120 (t=4.0s): THE CLEAN
+-----------------------------------------------+
|                                                |
|     ____________________________________       |
|    |                              |####| |     |
|    |   empty chessboard           |####| |     |
|    |                              |####| |     |
|    |______________________________|____| |     |
|                                   ^^^^         |
|                         Prism near front edge  |
+-----------------------------------------------+
Viewer: "The board is clean!"
Visual: Pristine chessboard visible through the
        glass prism. Objects falling off the
        front edge.


FRAME 150 (t=5.0s): THE EXIT
+-----------------------------------------------+
|                                                |
|     ____________________________________       |
|    |                                    |      |
|    |   empty chessboard                 |      |
|    |                                    |      |
|    |____________________________________|      |
|                                                |
|    [prism exited past z=+4.5]                  |
+-----------------------------------------------+
Viewer: "That was satisfying."
```

### Optical Effects Breakdown

```
The glass prism creates three distinct visual effects:

1. REFRACTION
   Rays passing THROUGH the prism bend at entry and exit surfaces.
   Objects seen through the prism appear slightly shifted.
   The dielectric material with ior=1.5 creates visible distortion.

2. BLUE TINTING
   tint: [0.3, 0.5, 0.95] means transmitted light is filtered blue.
   The chessboard seen through the prism has a blue color cast.
   This makes the prism visually distinct from the green glass W.

3. FRESNEL REFLECTIONS
   At glancing angles, the glass surface reflects the environment.
   The prism's top and side faces reflect the light source and sky.
   This gives the prism a "solid glass" appearance rather than
   appearing as a transparent cutout.

Together: The prism looks like a physical glass wall sliding across
the board, with the scene visible (but blue-tinted and distorted)
through it, and reflections on its surfaces.
```

---

## Integration Checkpoints

These are the points where the author's YAML choices directly affect
the viewer's experience:

```
CHECKPOINT 1: Material -> Visual Quality
  YAML: tint: [0.3, 0.5, 0.95], ior: 1.5
  VIEWER SEES: Blue-tinted refraction
  VALIDATE: Render single frame with prism at mid-board. Is the blue
            visible? Is refraction noticeable? Is it too dark/opaque?

CHECKPOINT 2: Velocity -> Dramatic Timing
  YAML: initial_velocity: [0.0, 0.0, 1.8]
  VIEWER SEES: Pace of the sweep
  VALIDATE: Does the prism reach the letters before the bowling
            debris settles? (Timing overlap creates visual interest.)
            Does the sweep feel deliberate, not rushed?

CHECKPOINT 3: Height -> Physics Interactions
  YAML: max y: 2.0
  VIEWER SEES: Objects pushed along vs flying over
  VALIDATE: Does the jelly cube get pushed or does it bounce over?
            Do letter blocks slide or get launched?

CHECKPOINT 4: Start Position -> Reveal Timing
  YAML: min z: -5.0 (0.5 units behind board back edge)
  VIEWER SEES: When the prism first becomes visible
  VALIDATE: At what frame does the prism first appear? It should be
            after the bowling hit begins (frame ~25-30) to avoid
            stealing focus from the initial action.

CHECKPOINT 5: Mass -> Push Behavior
  YAML: mass: 100.0 (kinematic ignores mass for its own motion,
        but mass affects collision response of dynamic objects)
  VIEWER SEES: How violently objects react to being pushed
  VALIDATE: Objects should tumble and slide, not explode. Adjust
            mass and prism velocity if reactions are too violent.
```

---

## Summary: Complete YAML Addition

For reference, the complete set of additions to `scenes/nwave_bowling.yaml`:

```yaml
# In materials section, add:
  - name: blue_glass
    type: dielectric
    ior: 1.5
    tint: [0.3, 0.5, 0.95]

# In objects section, add:
  - name: glass_sweeper
    type: box
    min: [-4.0, 0.0, -5.0]
    max: [4.0, 2.0, -4.5]
    material: blue_glass
    physics:
      body_type: kinematic
      mass: 100.0
      friction: 0.5
      restitution: 0.1
      initial_velocity: [0.0, 0.0, 1.8]

# In animation section, consider adjusting:
  duration: 6.0   # was 5.0 -- gives prism time to fully exit
```

### Code Change Required

One code fix is needed before this YAML configuration will work:

File: `src/infrastructure/jolt_physics_simulator.cpp`, lines 225-229

Change:
```cpp
if (desc.properties.body_type == BodyType::DYNAMIC) {
    body_settings.mLinearVelocity = JPH::Vec3(
```

To:
```cpp
if (desc.properties.body_type == BodyType::DYNAMIC ||
    desc.properties.body_type == BodyType::KINEMATIC) {
    body_settings.mLinearVelocity = JPH::Vec3(
```

This allows kinematic bodies to have an initial velocity, which Jolt will
maintain indefinitely (kinematic bodies are not affected by gravity or
collision forces on themselves -- they push other objects but are not
pushed back).
