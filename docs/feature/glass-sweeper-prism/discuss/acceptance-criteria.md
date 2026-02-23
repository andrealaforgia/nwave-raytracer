# Glass Sweeper Prism -- Acceptance Criteria (BDD)

All scenarios use Given/When/Then format. Organized by user story for traceability.

---

## US-001: Blue Glass Material in Scene YAML

### Scenario 1: Blue glass material loads from YAML
```gherkin
Given the scene YAML materials section contains:
  | name       | type       | ior | tint              |
  | blue_glass | dielectric | 1.5 | [0.4, 0.4, 0.95] |
When the YAML scene loader parses the materials section
Then a Dielectric material named "blue_glass" is created
And the material has IOR 1.5
And the material has tint (0.4, 0.4, 0.95)
```

### Scenario 2: Object references blue glass material
```gherkin
Given the scene YAML materials section defines "blue_glass" as a dielectric
And the objects section contains a box "sweeper_prism" referencing material "blue_glass"
When the YAML scene loader parses the full scene
Then the sweeper_prism shape has a pointer to the blue_glass Dielectric material
```

### Scenario 3: Unknown material reference rejected
```gherkin
Given the scene YAML objects section contains a box referencing material "nonexistent_glass"
And no material named "nonexistent_glass" is defined
When the YAML scene loader parses the objects section
Then a runtime error is thrown with message containing "Unknown material reference: nonexistent_glass"
```

---

## US-002: Sweeper Prism Box Appears in Scene

### Scenario 1: Sweeper prism parsed as box shape
```gherkin
Given the scene YAML objects section contains:
  | name           | type | min             | max            | material   |
  | sweeper_prism  | box  | [-4, 0, 4.0]   | [4, 1.5, 4.3] | blue_glass |
When the YAML scene loader parses the scene
Then a Box shape is created with min (-4, 0, 4.0) and max (4, 1.5, 4.3)
And the box uses the blue_glass material
```

### Scenario 2: Prism dimensions match chessboard width
```gherkin
Given the sweeper_prism has min_x=-4 and max_x=4
And the chessboard extends from x=-4 to x=4
When the prism width is calculated as (max_x - min_x)
Then the prism width is 8.0 units, equal to the chessboard width
```

### Scenario 3: Prism height exceeds tallest dynamic object
```gherkin
Given the sweeper_prism has max_y=1.5
And the tallest W block (W_0) has max_y=0.84
And the bowling ball apex is at y=0.6
And the jelly cube settles at approximately y=0.6
When compared to the prism height
Then the prism top (1.5) exceeds the tallest object (0.84) by 0.66 units
```

---

## US-003: Kinematic Velocity for Sweeper Motion

### Scenario 1: Kinematic body receives initial velocity at creation
```gherkin
Given a physics body descriptor with body_type KINEMATIC and initial_velocity (0, 0, -3.44)
When the JoltPhysicsSimulator creates the body via add_body()
Then the Jolt body has motion type Kinematic
And the Jolt body has linear velocity (0, 0, -3.44)
```

### Scenario 2: Kinematic body moves at constant velocity
```gherkin
Given a kinematic body at initial position (0, 0.75, 4.15) with velocity (0, 0, -3.44)
When the physics simulator executes 60 steps at dt=0.01667 (1.0 second of simulation)
Then the body position is approximately (0, 0.75, 0.71)
And the velocity remains (0, 0, -3.44)
```

### Scenario 3: Kinematic body pushes dynamic body
```gherkin
Given a kinematic body moving at velocity (0, 0, -3.44) starting at z=2.0
And a dynamic body (mass 0.1) at rest at position (0, 0.06, 0.5)
When the kinematic body reaches z=0.5 after physics simulation
Then the dynamic body has been displaced to a Z position less than 0.5
And the kinematic body velocity remains (0, 0, -3.44)
```

### Scenario 4: Kinematic body ignores gravity
```gherkin
Given a kinematic body at position (0, 0.75, 4.15) with velocity (0, 0, -3.44)
And gravity is set to (0, -9.81, 0)
When the physics simulator executes 60 steps at dt=0.01667
Then the body Y position remains 0.75 (gravity has no effect on kinematic bodies)
```

### Scenario 5: Dynamic body velocity still works as before
```gherkin
Given a dynamic body (bowling_ball) with initial_velocity (3.54, 0, -3.54)
When the physics simulator creates the body
Then the Jolt body has linear velocity (3.54, 0, -3.54)
And motion type is Dynamic
```

---

## US-004: Delayed Sweep Start via Per-Body Wake Frame

### Scenario 1: Per-object wake_frame parsed from YAML
```gherkin
Given the scene YAML contains a physics block:
  """yaml
  physics:
    body_type: kinematic
    initial_velocity: [0, 0, -3.44]
    start_asleep: true
    wake_frame: 60
  """
When the YAML scene loader parses the physics properties
Then PhysicsProperties has body_type=KINEMATIC, start_asleep=true, wake_frame=60
And initial_velocity is (0, 0, -3.44)
```

### Scenario 2: Sleeping kinematic body is stationary before wake frame
```gherkin
Given the sweeper_prism at z=4.15 with start_asleep=true, wake_frame=60, velocity (0, 0, -3.44)
And the animation renderer processes frame 59
When the sweeper_prism transform is queried
Then the Z position is 4.15 (unchanged from initial position)
```

### Scenario 3: Sleeping kinematic body activates at wake frame
```gherkin
Given the sweeper_prism at z=4.15 with start_asleep=true, wake_frame=60, velocity (0, 0, -3.44)
When the animation renderer processes frame 60
Then the physics simulator activates the sweeper_prism body
And after the physics steps for frame 60, the Z position has decreased
```

### Scenario 4: Global wake_frame unaffected by per-object wake_frame
```gherkin
Given W_0 block has start_asleep=true and no per-object wake_frame
And animation config has global wake_frame=20
And the sweeper_prism has per-object wake_frame=60
When the animation reaches frame 20
Then W_0 is activated by the global wake_frame mechanism
And the sweeper_prism remains asleep (per-object wake_frame=60 not yet reached)
```

### Scenario 5: Object without wake_frame defaults to no per-object wake
```gherkin
Given the bowling_ball has body_type=dynamic and no wake_frame field in YAML
When the YAML scene loader parses its physics properties
Then PhysicsProperties has wake_frame as empty/nullopt
And the bowling_ball is activated immediately (not sleeping)
```

---

## US-005: Sweeper Pushes All Dynamic Objects Off the Board

### Scenario 1: Scattered W blocks cleared by sweeper
```gherkin
Given the full nwave_bowling scene with sweeper_prism (wake_frame=60, velocity [0,0,-3.44])
And the bowling ball impacts W blocks between frames 30-50
When the animation completes all 150 frames
Then all 18 W blocks (W_0 through W_17) have final Z position less than -4.0
```

### Scenario 2: Bowling ball cleared by sweeper
```gherkin
Given the bowling ball (mass 6.0) at rest around z=-2 after its trajectory
And the sweeper_prism reaches z=-2 at approximately frame 95
When the animation completes all 150 frames
Then the bowling ball center Z position is less than -4.3
```

### Scenario 3: Jelly cube cleared by sweeper
```gherkin
Given the pink jelly cube settled near z=0.5 on the "e" letter area
And the sweeper_prism reaches z=0.5 at approximately frame 80
When the animation completes all 150 frames
Then the jelly cube centroid Z position is less than -4.0
```

### Scenario 4: Purple e blocks cleared by sweeper
```gherkin
Given the purple "e" letter blocks (e_0 through e_11) with body_type=dynamic
And the sweeper_prism sweeps through z=0.5 where the "e" is positioned
When the animation completes all 150 frames
Then all 12 "e" blocks have final Z position less than -4.0
```

### Scenario 5: Static chessboard unaffected
```gherkin
Given all 64 chessboard tiles have body_type=static (default, no physics block)
And the sweeper_prism is a kinematic body on LAYER_DYNAMIC
When the sweeper_prism passes over the chessboard tiles during its sweep
Then all chessboard tiles remain at their original Y=0 surface positions
```

### Scenario 6: Static letters unaffected
```gherkin
Given the "n" letter blocks (n_0 through n_9) have no physics block (static by default)
And the "a" letter blocks (a_0 through a_11) have no physics block
And the "v" letter blocks (v_0 through v_8) have no physics block
When the sweeper_prism passes through their Z positions during its sweep
Then all static letter blocks remain at their original positions
```

### Scenario 7: No physics instability during sweep
```gherkin
Given the sweeper_prism moves at velocity [0, 0, -3.44] through multiple dynamic bodies
When the animation renders all 150 frames
Then no dynamic body position exceeds 100 units from origin (no explosion)
And no dynamic body velocity exceeds 100 units/sec (no jitter)
And the sweeper_prism maintains constant velocity throughout
```
