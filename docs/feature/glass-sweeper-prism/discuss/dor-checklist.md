# Glass Sweeper Prism -- Definition of Ready Checklist

Validated per-story against all 8 DoR items. Every item must PASS with evidence for handoff to DESIGN wave.

---

## US-001: Blue Glass Material in Scene YAML

| # | DoR Item | Status | Evidence |
|---|----------|--------|----------|
| 1 | Problem statement clear, in domain language | PASS | "Scene designer needs a second glass material with blue tint so the sweeper prism is visually distinct from the green W-letter blocks." No technical jargon, stated from designer's perspective. |
| 2 | User/persona identified with specific characteristics | PASS | Andrea Laforgia, scene designer who edits YAML directly and evaluates rendered output visually. |
| 3 | At least 3 domain examples with real data | PASS | Three examples: (1) blue_glass parses with tint [0.4, 0.4, 0.95], (2) visual distinction from green_glass side by side, (3) same-IOR different-tint scenario. All use real material names and values. |
| 4 | UAT scenarios in Given/When/Then (3-7) | PASS | 3 scenarios covering: material parsing, object reference, and rendering with blue tint. |
| 5 | Acceptance criteria derived from UAT | PASS | 3 checkable items directly mapping to the 3 scenarios. |
| 6 | Story right-sized (1-3 days, 3-7 scenarios) | PASS | Zero code changes -- YAML content addition only. Estimated < 0.5 days. 3 scenarios. |
| 7 | Technical notes identify constraints/dependencies | PASS | Notes confirm zero code changes needed; existing `create_dielectric()` handles all fields. |
| 8 | Dependencies resolved or tracked | PASS | No dependencies. Uses existing parser and material system. |

**DoR Verdict: PASS** -- Ready for DESIGN wave handoff.

---

## US-002: Sweeper Prism Box Appears in Scene

| # | DoR Item | Status | Evidence |
|---|----------|--------|----------|
| 1 | Problem statement clear, in domain language | PASS | "Scene designer wants to place a tall, board-wide rectangular prism on the chessboard that will serve as the sweeper." Domain-level description of the visual element. |
| 2 | User/persona identified with specific characteristics | PASS | Andrea Laforgia, scene designer positioning objects via min/max coordinates in YAML. |
| 3 | At least 3 domain examples with real data | PASS | Three examples with real coordinates: (1) min [-4,0,4.0] max [4,1.5,4.3], (2) height 1.5 vs tallest object 0.84, (3) camera position [2,3,6] viewing the back edge. |
| 4 | UAT scenarios in Given/When/Then (3-7) | PASS | 3 scenarios covering: correct rendering position, board-width match, height vs dynamic objects. |
| 5 | Acceptance criteria derived from UAT | PASS | 4 checkable items covering position, width, height, and depth. |
| 6 | Story right-sized (1-3 days, 3-7 scenarios) | PASS | YAML content addition only. Estimated < 0.5 days. 3 scenarios. |
| 7 | Technical notes identify constraints/dependencies | PASS | Notes confirm zero code changes; prism is initially static, kinematic motion added in US-003. |
| 8 | Dependencies resolved or tracked | PASS | Depends on US-001 (blue_glass material). Tracked in story sequence. |

**DoR Verdict: PASS** -- Ready for DESIGN wave handoff.

---

## US-003: Kinematic Velocity for Sweeper Motion

| # | DoR Item | Status | Evidence |
|---|----------|--------|----------|
| 1 | Problem statement clear, in domain language | PASS | "The prism just sits there. He needs it to move steadily across the board, acting as an unstoppable wall that pushes everything in its path." Pain point clearly stated in scene-design terms. |
| 2 | User/persona identified with specific characteristics | PASS | Andrea Laforgia, scene designer who expects kinematic bodies to move and needs speed controllable via YAML. |
| 3 | At least 3 domain examples with real data | PASS | Three examples: (1) velocity [0,0,-3.44] traverses 8.6 units in 2.5s, (2) kinematic not affected by 6.0kg bowling ball collision, (3) zero velocity keeps body stationary. All with real values. |
| 4 | UAT scenarios in Given/When/Then (3-7) | PASS | 5 scenarios covering: velocity at creation, constant motion, pushing dynamic body, gravity immunity, backward compatibility with dynamic bodies. |
| 5 | Acceptance criteria derived from UAT | PASS | 5 checkable items mapping to each scenario. |
| 6 | Story right-sized (1-3 days, 3-7 scenarios) | PASS | One code change location (`add_body()` in Jolt simulator). Estimated 1 day. 5 scenarios. |
| 7 | Technical notes identify constraints/dependencies | PASS | Identifies exact code location: `JoltPhysicsSimulator::add_body()` line 225-229. Notes that Jolt kinematic bodies ignore gravity by default. |
| 8 | Dependencies resolved or tracked | PASS | No external dependencies. Change is self-contained in Jolt simulator. |

**DoR Verdict: PASS** -- Ready for DESIGN wave handoff.

---

## US-004: Delayed Sweep Start via Per-Body Wake Frame

| # | DoR Item | Status | Evidence |
|---|----------|--------|----------|
| 1 | Problem statement clear, in domain language | PASS | "Sweeper prism moving from frame 0, but needs to wait until bowling impact has played out (~2 seconds). Global wake_frame wakes ALL bodies at once, but sweeper needs independent timing." |
| 2 | User/persona identified with specific characteristics | PASS | Andrea Laforgia, scene designer choreographing multi-phase animation sequences via YAML. |
| 3 | At least 3 domain examples with real data | PASS | Three examples: (1) sweeper wakes at frame 60 (2.0s), (2) W/e blocks still wake at global wake_frame=20, (3) bowling ball unaffected (no start_asleep). All with real frame numbers and timing. |
| 4 | UAT scenarios in Given/When/Then (3-7) | PASS | 5 scenarios covering: YAML parsing, pre-wake stationarity, activation at wake frame, global wake_frame compatibility, default behavior. |
| 5 | Acceptance criteria derived from UAT | PASS | 5 checkable items directly mapping to scenarios. |
| 6 | Story right-sized (1-3 days, 3-7 scenarios) | PASS | Changes in 3 files (PhysicsProperties, YAML parser, animation renderer). Estimated 1-2 days. 5 scenarios. |
| 7 | Technical notes identify constraints/dependencies | PASS | Identifies: `std::optional<int> wake_frame` in PhysicsProperties, YAML parser addition, animation renderer per-frame check, potential `wake_body()` method. Notes backward compatibility requirement. |
| 8 | Dependencies resolved or tracked | PASS | Depends on US-003 (kinematic velocity). PhysicsSimulator may need `wake_body()` -- tracked as design decision. |

**DoR Verdict: PASS** -- Ready for DESIGN wave handoff.

---

## US-005: Sweeper Pushes All Dynamic Objects Off the Board

| # | DoR Item | Status | Evidence |
|---|----------|--------|----------|
| 1 | Problem statement clear, in domain language | PASS | "Verify the sweeper actually clears every dynamic object off the chessboard edge. If any object gets stuck, passes through, or clips, the cinematic effect is ruined." |
| 2 | User/persona identified with specific characteristics | PASS | Andrea Laforgia, scene designer evaluating the final rendered video for visual correctness. |
| 3 | At least 3 domain examples with real data | PASS | Three examples: (1) W blocks (0.1kg each) pushed off front edge by frame 150, (2) bowling ball (6.0kg) pushed off despite weight, (3) jelly cube deforms against glass face then pushed off. |
| 4 | UAT scenarios in Given/When/Then (3-7) | PASS | 7 scenarios covering: W blocks, bowling ball, jelly cube, e blocks, static chessboard, static letters, physics stability. |
| 5 | Acceptance criteria derived from UAT | PASS | 7 checkable items mapping to each scenario. |
| 6 | Story right-sized (1-3 days, 3-7 scenarios) | PASS | Integration/verification story, no new code. Estimated 1 day for YAML tuning and visual verification. 7 scenarios. |
| 7 | Technical notes identify constraints/dependencies | PASS | Notes: integration story validating US-001 through US-004, may require parameter tuning, visual verification on frame subset. Explains static-vs-kinematic collision filtering via ObjectLayerPairFilter. |
| 8 | Dependencies resolved or tracked | PASS | Depends on US-001, US-002, US-003, US-004. All tracked in story sequence. |

**DoR Verdict: PASS** -- Ready for DESIGN wave handoff.

---

## Summary

| Story | Title | DoR Status | Estimated Effort |
|-------|-------|------------|------------------|
| US-001 | Blue Glass Material in Scene YAML | PASS | < 0.5 days |
| US-002 | Sweeper Prism Box Appears in Scene | PASS | < 0.5 days |
| US-003 | Kinematic Velocity for Sweeper Motion | PASS | 1 day |
| US-004 | Delayed Sweep Start via Per-Body Wake Frame | PASS | 1-2 days |
| US-005 | Sweeper Pushes All Dynamic Objects Off the Board | PASS | 1 day |
| **Total** | | **All PASS** | **3-4 days** |

## Dependency Graph

```
US-001 (blue_glass material)
  |
  v
US-002 (sweeper box geometry) --+
                                |
US-003 (kinematic velocity) ----+
  |                             |
  v                             |
US-004 (per-body wake_frame) ---+
                                |
                                v
                         US-005 (integration verification)
```

## Identified Risks

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Kinematic body tunnels through small blocks at high speed | Low | Medium | Sweep speed (3.44 u/s) at physics dt (0.01667s) = 0.057 units/step, well below block size (0.12 units). CCD not needed. |
| Jelly cube gets stuck against sweeper instead of being pushed off | Medium | Low | Tunable via friction and pressure parameters. If needed, reduce jelly friction or increase sweeper speed. |
| Static letters collide with sweeper (unintended) | Low | High | Static objects are on LAYER_STATIC; kinematic sweeper is on LAYER_DYNAMIC. ObjectLayerPairFilter prevents static-vs-static collisions, but kinematic is on LAYER_DYNAMIC, so kinematic-vs-static IS allowed. The static letters have no physics body (default STATIC body type). Need to verify the collision filtering handles this correctly. |
| Per-body wake_frame interacts with global wake_frame unexpectedly | Medium | Medium | Design must ensure per-body wake_frame takes precedence when set; global wake_frame only applies to bodies without a per-body value. |

## Handoff Notes for DESIGN Wave

1. **Key design decision**: How to drive kinematic body velocity -- `SetLinearVelocity` at creation time (simplest) vs. `MoveKinematic` per-frame (more flexible). Recommend starting with creation-time velocity.
2. **Key design decision**: Per-body `wake_frame` storage -- `std::optional<int>` in `PhysicsProperties` vs. separate motion timeline. Recommend the simple `optional<int>` approach.
3. **Collision layer concern**: Verify that static objects WITHOUT an explicit physics block are assigned `LAYER_STATIC` and that kinematic-on-LAYER_DYNAMIC does not collide with LAYER_STATIC. Current filter: `LAYER_STATIC` only collides with `LAYER_DYNAMIC`, and kinematic is on `LAYER_DYNAMIC`, so kinematic WILL collide with static. This means the sweeper WILL collide with chessboard tiles (static). Since the sweeper sits on y=0 (same as chessboard top surface), it may clip or stutter. Design should evaluate whether the sweeper should start at y > 0 (e.g., y=0.01) to float above the board, or whether the physics handles this gracefully.
4. **Animation duration**: Current 5-second animation may need extension if the sweep feels rushed. The YAML `duration` field controls this without code changes.
