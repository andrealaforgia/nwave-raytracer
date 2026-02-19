# Architecture Review: Soft Body Jelly Physics Feature
## Comprehensive Review Report

**Review Date**: 2026-02-19
**Reviewed By**: Atlas (Solution Architecture Reviewer)
**Status**: CONDITIONALLY_APPROVED
**Approval Gated On**: Resolution of 1 critical, 4 major, 2 minor issues

---

## Executive Summary

The soft-body-jelly-physics architecture design is **well-structured and architecturally sound**. The design demonstrates strong adherence to clean architecture principles, clear separation of concerns across rings, and comprehensive user story traceability. All 13 user stories are properly addressed.

**Key Strengths**:
- Domain types (SoftBodyDesc, SoftBodyMeshData, DeformableMesh) correctly isolated from infrastructure concerns
- Ring dependencies properly respected (Core ← Domain ← Application ← Infrastructure)
- Technology choices justified with explicit alternatives analysis (ADR-001, ADR-002)
- Data flow diagrams are correct and comprehensive
- Backward compatibility preserved for existing scenes and physics API

**Issues Identified**: 8 total
- 1 Critical (implementation detail requiring clarification)
- 4 Major (3 implementation details, 1 design quality)
- 2 Minor (documentation gaps)
- 1 Suggestion (risk mitigation)

All issues are **resolvable without architectural redesign**.

---

## Issue Details

### CRITICAL ISSUES

#### C-01: Incomplete PhysicsBodyDesc Extension for Compound Mesh

**Severity**: CRITICAL
**Location**: data-models.md §5, component-boundaries.md §IP-2
**Status**: OPEN - Requires clarification before code implementation

**Issue Description**:

The architecture specifies extending `PhysicsBodyDesc` to support letter mesh physics via convex decomposition. Specifically:

- data-models.md §5 (lines 105-122) states: "Add support for compound mesh collision shape (for letter physics)" with a new field `convex_hulls: std::vector<std::vector<Point3>>`
- component-boundaries.md §IP-2 (lines 334-344, KD-5) discusses using `COMPOUND_MESH` shape type
- component-boundaries.md §MODIFIED: YamlSceneLoader (lines 105-111) mentions "extend PhysicsBodyDesc with COMPOUND_MESH shape type"

**Problem**:

The current `PhysicsBodyDesc` struct (physics_simulator.h, lines 22-28) contains:
```cpp
struct PhysicsBodyDesc {
    PhysicsShapeType shape_type{PhysicsShapeType::BOX};
    Vec3 dimensions{1.0, 1.0, 1.0};
    Point3 position{0.0, 0.0, 0.0};
    Quaternion rotation;
    PhysicsProperties properties;
};
```

The design does NOT clarify:

1. **How convex_hulls integrates with the struct**: Is it a 6th field? What is its type exactly? How is it serialized or passed?
2. **How PhysicsBodyDesc changes impact add_body()**: The existing `PhysicsSimulator::add_body(const PhysicsBodyDesc& desc)` signature remains unchanged. How does JoltPhysicsSimulator differentiate between a BOX/SPHERE/COMPOUND_MESH when both use the same add_body() method?
3. **Jolt API for StaticCompoundShape**: The design mentions "JoltPhysicsSimulator creates ConvexHullShapes and combines them into StaticCompoundShape" but provides no pseudo-code showing:
   - How ConvexHull vertices convert to Jolt ConvexHullShapeSettings
   - How StaticCompoundShapeSettings::mShapes accumulates ConvexHullShape instances
   - Whether the compound shape is created before or after add_body() registration

**Impact**:

Without clarity, implementation will stall when the developer:
- Tries to modify PhysicsBodyDesc and discovers field conflicts
- Attempts to implement the COMPOUND_MESH case in JoltPhysicsSimulator::add_body()
- Struggles with Jolt's ConvexHullShapeSettings and StaticCompoundShapeSettings API

**Recommendation**:

Add a new section to data-models.md (after §5) titled "Modified: PhysicsBodyDesc Integration Details" that includes:

```cpp
// PROPOSED: Add to PhysicsBodyDesc
struct PhysicsBodyDesc {
    PhysicsShapeType shape_type{PhysicsShapeType::BOX};
    Vec3 dimensions{1.0, 1.0, 1.0};
    Point3 position{0.0, 0.0, 0.0};
    Quaternion rotation;
    PhysicsProperties properties;

    // NEW: Convex hulls for COMPOUND_MESH shape type
    // Only populated when shape_type == PhysicsShapeType::COMPOUND_MESH
    std::vector<std::vector<Point3>> convex_hulls;
};
```

Provide pseudo-code for JoltPhysicsSimulator::add_body() handling COMPOUND_MESH:

```
if (desc.shape_type == PhysicsShapeType::COMPOUND_MESH) {
    StaticCompoundShapeSettings compound_settings;
    for (const auto& hull : desc.convex_hulls) {
        ConvexHullShapeSettings hull_settings(hull);
        compound_settings.mShapes.push_back(hull_settings);
    }
    auto compound_shape = compound_settings.Create();
    // Create body with compound_shape...
}
```

---

### MAJOR ISSUES (Implementation Details)

#### M-01: Animation Renderer Integration Missing Key Details

**Severity**: MAJOR
**Location**: architecture-design.md §3 (IP-3), component-boundaries.md §MODIFIED: AnimationRenderer
**Status**: OPEN - Requires implementation strategy clarification

**Issue Description**:

The design correctly identifies that AnimationRenderer must:
1. Detect which shapes are soft bodies (via BodyType::SOFT in shape_physics)
2. Extract deformed mesh data per frame from PhysicsSimulator
3. Update DeformableMesh vertices before rendering

However, the design lacks critical implementation details:

**Missing Detail 1: Shape Tracking Strategy**

The AnimationRenderer receives:
- `const Scene& scene` - contains renderable shapes (Shape* pointers)
- `const std::vector<PhysicsProperties>& shape_physics` - parallel vector with physics properties

The design does NOT explain:
- How does AnimationRenderer map from shape_physics[i] to scene's shapes[i]?
- Is there a guarantee that order is preserved?
- How does the code identify which shapes are DeformableMesh vs. other types?

Current implementation (animation_renderer.cpp lines 15-53) uses `dynamic_cast<const Sphere*>(shape)` to extract shape parameters. Applying the same pattern to soft bodies suggests:

```cpp
// Pseudo-code from design, but UNVERIFIED in actual code
if (auto* deformable = dynamic_cast<DeformableMesh*>(scene_.shapes()[i])) {
    if (shape_physics[i].body_type == BodyType::SOFT) {
        auto mesh_data = physics_->get_soft_body_mesh(body_id);
        deformable->update_vertices(mesh_data.vertices);
    }
}
```

But the design does NOT explain:
- How is `body_id` known for shape index `i`?
- Is there a separate mapping structure: `std::vector<int> soft_body_ids_` indexed by shape?
- Or does the code iterate all soft bodies and match them to shapes by... what?

**Missing Detail 2: BodyType::SOFT in is_movable_body()**

The animation_renderer.cpp (line 61-63) has:

```cpp
bool is_movable_body(BodyType body_type) {
    return body_type == BodyType::DYNAMIC || body_type == BodyType::KINEMATIC;
}
```

The design does NOT explicitly state whether:
- SOFT bodies should return TRUE (they are movable, but via vertex update not rigid transform)
- SOFT bodies should return FALSE (they are movable but handled by separate code path)

The correct answer is: **SOFT bodies should return FALSE because they are not rigid transforms; they are updated via get_soft_body_mesh().**

But this is not stated in the architecture document, creating ambiguity for the implementer.

**Missing Detail 3: DeformableMesh Pointer Management**

The AnimationRenderer needs to call `deformable_mesh->update_vertices()` per frame. How does it access the DeformableMesh pointer?

Option A: Store in Scene alongside shapes
```cpp
// Pseudo-code - NOT in design
std::vector<DeformableMesh*> soft_body_shapes_;
```

Option B: Dynamic cast each shape per frame
```cpp
// Pseudo-code - NOT in design
for (auto* shape : scene_.shapes()) {
    if (auto* deformable = dynamic_cast<DeformableMesh*>(shape)) {
        // Update...
    }
}
```

The design does not specify which approach is preferred or implementable within the existing Scene/AnimationRenderer structure.

**Impact**:

The implementer will face ambiguity when coding AnimationRenderer::render() and will need to make design choices that may not be architecturally consistent:
- Choice of body ID tracking mechanism affects code maintainability
- is_movable_body() return value for SOFT affects control flow
- Shape access strategy affects performance and pointer lifetime management

**Recommendation**:

Add a new section to architecture-design.md (after §4, "Data Flow") titled "Per-Frame Update Loop: Detailed Implementation Strategy" that includes:

```
SHAPE TRACKING:
- AnimationRenderer maintains two parallel structures:
  * std::vector<PhysicsProperties> shape_physics_ (from constructor)
  * std::vector<int> physics_body_ids_ (indexed by scene shape index, -1 for static shapes)

- During scene construction in AnimationRenderer constructor:
  * Iterate shapes by index
  * For shapes with body_type != STATIC, call physics_->add_body() or physics_->add_soft_body()
  * Store returned body_id in physics_body_ids_[i]

PER-FRAME SOFT BODY UPDATE:
  for (int i = 0; i < scene_.shapes().size(); ++i) {
      if (shape_physics_[i].body_type == BodyType::SOFT) {
          int body_id = physics_body_ids_[i];
          auto mesh_data = physics_->get_soft_body_mesh(body_id);
          // Access shape via Scene and cast to DeformableMesh
          auto* deformable = dynamic_cast<DeformableMesh*>(scene_.shapes()[i]);
          deformable->update_vertices(mesh_data.vertices);
      }
  }

is_movable_body(BodyType::SOFT):
  - Returns FALSE; soft bodies are handled separately via get_soft_body_mesh()
```

Provide a concrete code flow diagram (ASCII or Mermaid) showing the per-frame loop order and shape filtering.

---

#### M-02: Data Model Inconsistency in SoftBodyMeshData

**Severity**: MAJOR
**Location**: data-models.md §2 (lines 31-44), architecture-design.md §3 (IP-3, lines 159-160)
**Status**: OPEN - Requires design decision and implementation consequence analysis

**Issue Description**:

The design states (data-models.md §2, line 39):

> `face_indices: std::vector<int>` | Flat list of triangle vertex indices (3 per face). Size = num_surface_faces * 3. **Constant across frames.**

This means face topology never changes (correct for a soft body with deformable vertices but constant faces).

However, the architecture describes the API (architecture-design.md §3, lines 159-160):

> `physics->get_soft_body_mesh(body_id)` per soft body per frame
>
> **Passes vertex positions to `DeformableMesh::update_vertices()`**

The question is: **Does get_soft_body_mesh() return face_indices every frame, or just vertices?**

**Current Design Implication**:

If SoftBodyMeshData always contains both vertices AND face_indices (returned every frame):

```cpp
struct SoftBodyMeshData {
    std::vector<Point3> vertices;         // 125 entries for 5x5x5
    std::vector<int> face_indices;        // 576 entries (192 triangles * 3)
};
```

Then per frame, the renderer extracts and copies **701 entries** even though face_indices never change.

For a 150-frame animation:
- 125 * 150 vertex entries = necessary
- 576 * 150 face index entries = **unnecessary repetition** (576 entries copied 150 times)

**Performance Impact**:

- JoltPhysicsSimulator::get_soft_body_mesh() copies 576 face indices unnecessarily every frame
- AnimationRenderer::render() receives redundant data
- Total wasted copies: 576 * 150 = 86,400 integer copies

For a 5-second 30fps animation with 5x5x5 cube, this is minor. But for longer animations or higher-resolution soft bodies, waste accumulates.

**Recommended Solution**:

**Option 1 (Recommended)**: Cache face_indices at creation time

```cpp
// In JoltPhysicsSimulator::add_soft_body()
soft_body_face_indices_[body_id] = extracted_face_indices;

// In get_soft_body_mesh()
SoftBodyMeshData data;
data.vertices = extracted_vertices;
data.face_indices = soft_body_face_indices_[body_id];  // Cached, not re-extracted
return data;
```

Slightly more complex but avoids redundant copies.

**Option 2 (Current Design)**: Accept redundant copies

```cpp
// get_soft_body_mesh() extracts both vertices AND face_indices every call
// Simpler implementation, minor performance cost
```

**Design Action**:

Clarify in data-models.md §2 whether:

1. SoftBodyMeshData face_indices are cached at add_soft_body() time and returned from cache during get_soft_body_mesh(), OR
2. Face indices are re-extracted every frame (simpler but wasteful)

Recommend caching strategy and document the JoltPhysicsSimulator internal structure:

```cpp
// In JoltPhysicsSimulator::Impl
std::map<int, std::vector<int>> soft_body_face_indices_;  // Cached per body
```

**Impact**:

Without clarity, the implementer may:
- Re-extract face indices every frame (inefficient)
- Cache them differently than other soft bodies, creating inconsistency
- Discover late in development that face_indices should be returned once, not 150 times

**Recommendation**:

Update data-models.md §2 to include:

> **Face Index Caching Strategy**:
> Face indices are constant across frames. To avoid redundant copies, JoltPhysicsSimulator caches face indices at add_soft_body() time:
>
> ```cpp
> // add_soft_body() extracts and caches face indices once
> soft_body_face_indices_[body_id] = jolt_soft_body->GetFaces();
>
> // get_soft_body_mesh() returns cached face indices
> result.face_indices = soft_body_face_indices_[body_id];
> ```

---

#### M-03: BodyType Enum Extension Does Not Cover All Switch Statements

**Severity**: MAJOR
**Location**: data-models.md (line 99-100), architecture-design.md §8 (lines 374-375)
**Status**: OPEN - Requires complete codebase audit and checklist

**Issue Description**:

The design correctly identifies that `map_body_type_to_motion()` in jolt_physics_simulator.cpp must handle `BodyType::SOFT` (data-models.md §4, line 99):

> `map_body_type_to_motion()` in `jolt_physics_simulator.cpp`: add SOFT case (soft bodies are on DYNAMIC layer with special creation path)

However, the design does NOT provide a **comprehensive checklist** of all switch statements on BodyType in the codebase.

**Identified Switch Statements** (from code review):

1. **animation_renderer.cpp line 61** (`is_movable_body`):
   ```cpp
   bool is_movable_body(BodyType body_type) {
       return body_type == BodyType::DYNAMIC || body_type == BodyType::KINEMATIC;
   }
   ```
   **Required Change**: Must return FALSE for SOFT (soft bodies are not rigid transforms)
   **NOT mentioned in design**

2. **yaml_scene_loader.cpp** (`parse_body_type`, location not provided but implied):
   **Required Change**: Add "soft" -> BodyType::SOFT mapping
   **Mentioned in component-boundaries.md line 110 but not in checklist**

3. **jolt_physics_simulator.cpp** (`map_body_type_to_motion`, location not provided):
   **Required Change**: Add SOFT case
   **Mentioned in data-models.md line 99**

4. **Unknown switch statements** in:
   - Validator (validation rules, requirements.md §6 lines 109-126)
   - SceneLoadResult handling
   - Scene construction logic
   - Any serialization code
   **NOT audited in design**

**Problem**:

The design identifies 1-2 switch statements by name but lacks:
- A complete audit of all BodyType comparisons (switch, if/else-if chains)
- Specification of expected behavior for each case
- Guidance on which statements must treat SOFT differently (e.g., is_movable_body returns false) vs. which can use a default case
- Backward compatibility implications (e.g., existing code that assumes 3 body types)

**Impact**:

The implementer will:
- Spend extra time searching for all BodyType usages
- Risk missing a switch statement, causing compilation errors or logic bugs
- Need to make educated guesses about SOFT behavior in each context

**Recommendation**:

Add a new section to architecture-design.md (after §8) titled "BodyType::SOFT Integration Checklist" that includes:

```
COMPLETE AUDIT: All BodyType Switch Statements

1. animation_renderer.cpp::is_movable_body(BodyType)
   - Current cases: DYNAMIC, KINEMATIC
   - Required: SOFT case returns FALSE
   - Rationale: Soft bodies use get_soft_body_mesh(), not rigid transforms

2. jolt_physics_simulator.cpp::map_body_type_to_motion(BodyType)
   - Current cases: STATIC, DYNAMIC, KINEMATIC
   - Required: SOFT case returns MotionType::Dynamic (soft body uses dynamic layer)
   - Rationale: Soft bodies participate in physics like dynamic rigid bodies

3. yaml_scene_loader.cpp::parse_body_type(const std::string&)
   - Current cases: "static", "dynamic", "kinematic"
   - Required: "soft" maps to BodyType::SOFT
   - Rationale: YAML schema extension for soft_body_cube objects

4. validator.cpp::validate() (if BodyType switch exists)
   - Current cases: STATIC, DYNAMIC, KINEMATIC validation
   - Required: Add SOFT case validation (grid_resolution, pressure, etc.)
   - Rationale: Soft body parameters have unique constraints

5. scene_loader.cpp::create_shape() (if BodyType affects shape selection)
   - Current cases: Shape type selected by object type (sphere, box, etc.)
   - Required: SOFT creates DeformableMesh instead of rigid shape
   - Rationale: soft_body_cube type requires DeformableMesh

COMPILATION CHECK:
- Run: grep -r "BodyType::" src/ | grep -E "(switch|case)" to find all locations
- Verify all locations have SOFT case handling or appropriate default case
```

---

### MAJOR ISSUES (Design Quality)

#### M-04: Normal Recomputation Algorithm Under-Specified

**Severity**: MAJOR
**Location**: data-models.md §3 (lines 67-75), user-stories.md US-06 (lines 507-510)
**Status**: OPEN - Requires pseudocode and edge case handling

**Issue Description**:

The design describes area-weighted normal recomputation (data-models.md §3, lines 73-75):

> 3. Smooth normals recomputed: for each face, accumulate `cross(v1-v0, v2-v0)` into each vertex's normal accumulator (area-weighted because cross product magnitude = 2x face area), then normalize all vertex normals

This is **mathematically correct** but **algorithmically under-specified**:

**Missing Pseudocode Details**:

1. **Initialization**: Are vertex normals initialized to zero before accumulation?
   - Implied: yes, but not stated

2. **Accumulation per vertex**: Does each face normal accumulate into all 3 vertices?
   ```
   // Pseudo-code NOT provided in design
   for (int i = 0; i < 3; ++i) {
       int vertex_index = face_indices[face_i * 3 + i];
       normals[vertex_index] += cross_product;  // Accumulate
   }
   ```
   - Implied: yes, but not stated

3. **Normalization timing**: Do you normalize after each face, or after all faces?
   - Correct: after all faces (single normalization per vertex)
   - Implied: correct, but not stated

4. **Edge case: Vertex shared by only 1 face**:
   - In a deformable mesh, is this possible?
   - Expected behavior: normal = face normal (after normalization)
   - Not addressed

5. **Edge case: Degenerate face (zero-area)**:
   - If cross product is zero, does it contribute to vertex normal?
   - Expected: skip or handle gracefully
   - Not addressed

6. **Winding order assumption**:
   - Does the algorithm assume outward-facing winding order?
   - Expected: yes, but not verified in design
   - Not addressed

**Current Design Statement** (user-stories.md US-06, line 509):

> Area-weighted normal: accumulate `cross(v1-v0, v2-v0)` per face (magnitude = 2x face area), sum into vertex normals, then normalize.

This is correct but lacks the pseudocode implementation and edge case handling needed for code review.

**Impact**:

The implementer will:
- Write pseudocode based on their interpretation (may differ from intent)
- Discover edge cases during testing (degenerate faces, single-face vertices)
- Need to revise code when edge case handling differs from reviewer expectations

**Recommendation**:

Add a detailed section to data-models.md §3 under "update_vertices() Contract" with pseudocode:

```
3. Smooth normals recomputed using area-weighted face normal averaging:

   Algorithm:
   a) Initialize vertex normals to zero:
      normals_.clear();
      normals_.resize(vertices_.size(), Vec3(0, 0, 0));

   b) For each triangle face, accumulate area-weighted normal into 3 vertices:
      for (int f = 0; f < face_count(); ++f) {
          int i0 = face_indices_[f * 3 + 0];
          int i1 = face_indices_[f * 3 + 1];
          int i2 = face_indices_[f * 3 + 2];

          Vec3 v0 = vertices_[i0];
          Vec3 v1 = vertices_[i1];
          Vec3 v2 = vertices_[i2];

          // Cross product magnitude = 2 * face area (area-weighted)
          Vec3 face_normal = cross(v1 - v0, v2 - v0);

          // Skip degenerate faces (zero-area)
          if (face_normal.length() < 1e-6) continue;

          // Accumulate into all 3 vertices (weighted by area)
          normals_[i0] += face_normal;
          normals_[i1] += face_normal;
          normals_[i2] += face_normal;
      }

   c) Normalize all vertex normals:
      for (auto& normal : normals_) {
          if (normal.length() > 1e-6) {
              normal = normal.normalized();
          } else {
              normal = Vec3(0, 0, 1);  // Default upward for degenerate case
          }
      }

   Postcondition: All normals are unit vectors (length = 1.0)
```

---

### MINOR ISSUES

#### m-01: Font Mesh Generator Output Type Inconsistency

**Severity**: MINOR
**Location**: data-models.md §7 (lines 138-148), component-boundaries.md §NEW: FontMeshGenerator (lines 127-135)
**Status**: OPEN - Requires scope clarification

**Issue Description**:

The design specifies FontMeshResult (data-models.md §7) with fields:

```cpp
struct FontMeshResult {
    std::vector<Point3> vertices;
    std::vector<Vec3> normals;
    std::vector<int> vertex_indices;        // Triangle vertex indices
    std::vector<int> normal_indices;        // Triangle normal indices (may differ)
};
```

This design follows TriangleMesh's pattern (triangle_mesh.h lines 27-28):

```cpp
std::vector<int> vertex_indices_;
std::vector<int> normal_indices_;
```

However, DeformableMesh (data-models.md §3, lines 54-61) uses a **different pattern**:

> face_indices: flat 3-per-triangle layout, **always 1:1 with vertices**

This creates an impedance mismatch:

- **FontMeshResult** is suitable for constructing TriangleMesh (separate vertex/normal indices)
- **DeformableMesh** expects flat face indices with 1:1 vertex-normal mapping

**Problem**:

The design does NOT clarify:
1. Is FontMeshResult used ONLY for TriangleMesh (letter rendering), never for DeformableMesh?
2. If a soft body were to use a font-generated mesh (future feature), would the mesh data need conversion?
3. Should FontMeshResult have a method to flatten indices for DeformableMesh, or should separate adapters exist?

The architecture document (component-boundaries.md §MODIFIED: YamlSceneLoader, line 111) says:

> Add `type: letter` branch: invokes FontMeshGenerator, creates TriangleMesh, optionally invokes ConvexDecomposer for physics.

This implies FontMeshResult is always fed to TriangleMesh, never DeformableMesh. But this is not explicitly stated.

**Impact**:

The implementer may:
- Assume FontMeshResult can be used for DeformableMesh and attempt conversion (wasted effort)
- Discover late that separate index arrays are incompatible with DeformableMesh's flat layout
- Need to clarify what "letter" objects can be (rendering shape only? soft physics body? rigid physics body?)

**Recommendation**:

Update data-models.md §7 to add a note:

> **Usage Scope**:
>
> FontMeshResult is designed for TriangleMesh rendering of letter objects only. It is NOT used for DeformableMesh, which only supports soft body cubes (not soft body letters) in v1. Separate vertex and normal indices enable smooth shading on letter surfaces; DeformableMesh flat face indices are suitable only for deforming cubes.
>
> If future versions support soft body letters, the mesh data would need conversion to a flat index layout before DeformableMesh construction.

---

#### m-02: GPU SceneFlattener Handling Mentioned But Not Specified

**Severity**: MINOR
**Location**: architecture-design.md §6 (KD-6, lines 346-354)
**Status**: OPEN - Requires implementation verification

**Issue Description**:

The design (KD-6, lines 348-353) states:

> DeformableMesh is CPU-rendered only. The GPU SceneFlattener skips it (same as TriangleMesh today).
>
> Rationale:
> - The GPU Metal pipeline uploads vertex buffers once; per-frame updates require significant shader and buffer management changes
> - **TriangleMesh is already CPU-only in the GPU path**

The statement "TriangleMesh is already CPU-only" suggests SceneFlattener has existing logic to skip TriangleMesh. However, the design does NOT specify:

1. **How does SceneFlattener skip TriangleMesh today?**
   - Explicit if/else for TriangleMesh type?
   - A catch-all "unsupported shape" handler?
   - Warning/error behavior?

2. **Will DeformableMesh be automatically skipped by the existing handler?**
   - If catch-all exists: yes, automatically handled
   - If explicit TriangleMesh case: no, new code needed

3. **What is the expected error/warning message?**
   - Should users see "DeformableMesh shapes are not supported on GPU; rendering on CPU"?
   - Or is it silent (shapes are skipped with no notification)?

**Impact**:

The implementer will need to:
- Inspect SceneFlattener code to understand skipping mechanism
- Determine if new code is needed or if existing logic covers DeformableMesh
- Possibly add a warning message for user-facing feedback

**Recommendation**:

Verify SceneFlattener implementation and update KD-6 with explicit statement:

> **GPU SceneFlattener Handling**:
>
> SceneFlattener currently skips TriangleMesh with a warning message (implementation detail: [specify location]). DeformableMesh will be handled identically -- skipped with the same warning pattern, requiring no new code.
>
> If SceneFlattener does NOT have catch-all handling, add a new dynamic_cast case:
> ```cpp
> if (dynamic_cast<const DeformableMesh*>(shape)) {
>     // Log warning and skip (CPU rendering only)
>     continue;
> }
> ```

---

### SUGGESTIONS

#### S-01: Add Risk Mitigation Plan for Dynamic Compound Collision

**Severity**: SUGGESTION
**Location**: technology-stack.md §2 (lines 56-73), ADR-002
**Status**: INFORMATIONAL - Recommended but not blocking

**Issue Description**:

The technology-stack.md correctly assesses V-HACD as LOW risk (line 66):

> **Risk**: LOW. The soft body module is part of the already-integrated Jolt build.

However, the **actual technical risk** is not the V-HACD library itself, but whether **Jolt correctly handles a StaticCompoundShape as a dynamic rigid body receiving collision impulses from soft bodies**.

The requirements (requirements.md, constraint line 183) states:

> No soft-to-soft body collision | Jolt limitation | Only one object in the scene can be a soft body if mutual collision is needed; the 'e' must be rigid

This suggests soft-to-soft collisions are unsupported, but **soft-to-rigid collision IS supported**. The user story US-03 (requirements-discussion.md line 265) includes:

> Scenario: Soft body interacts with dynamic rigid body
>
> Given a soft body cube at (0, 4, 0) and a dynamic rigid box at (0, 1, 0)
> When the soft body falls onto the rigid box
> Then the rigid box receives a downward impulse (its velocity changes)

This test case validates soft-to-rigid collision. However, the design does NOT include a specific test for **soft-body-to-compound-shape collision** (the 'e' letter is a dynamic compound shape, not a simple box).

**Potential Risk**:

Jolt may have undocumented limitations or bugs when:
- A soft body collides with a StaticCompoundShape
- The compound shape is created from V-HACD output (unusual vertex distribution)
- The compound shape is dynamic (not static)

**Recommendation** (Informational):

Add a new section to technology-stack.md (after ADR-002) titled "Risk Mitigation: Soft-to-Compound Collision Validation":

> **Risk**: Jolt may not correctly handle soft body collision with dynamic StaticCompoundShape (letter).
>
> **Validation Required** (in US-11 test cases):
> 1. Create a letter 'e' as dynamic compound shape from V-HACD
> 2. Drop a soft body cube onto the letter
> 3. Verify:
>    - Soft body deforms on contact
>    - Letter receives impulse and moves/rotates
>    - No solver divergence or NaN vertices
> 4. If collision fails, fallback: use multiple convex shapes (separate ConvexHullShape for each decomposed hull, not compound)

---

## Comprehensive Architecture Assessment

### Ring Architecture Compliance

**Ring 1 (Core)**: ✓ PASS
- No changes required. Point3, Vec3, AABB, Ray types sufficient.

**Ring 2 (Domain)**: ✓ PASS
- SoftBodyDesc: pure data, no Jolt dependencies ✓
- SoftBodyMeshData: pure data, no Jolt dependencies ✓
- DeformableMesh: Shape subclass with mutable vertices, clean interface ✓
- BodyType::SOFT: enum extension, no dependencies ✓

**Ring 3 (Application)**: ✓ PASS
- PhysicsSimulator methods (add_soft_body, is_soft_body, get_soft_body_mesh) are abstract contracts ✓
- AnimationRenderer orchestrates physics and rendering without infrastructure coupling ✓

**Ring 4 (Infrastructure)**: ✓ PASS
- JoltPhysicsSimulator: Jolt-specific implementation ✓
- YamlSceneLoader: YAML parsing and scene construction ✓
- FontMeshGenerator, ConvexDecomposer: external library adapters ✓

**Outward Dependency Check**: ✓ PASS
- Domain types do not depend on Application or Infrastructure ✓
- No implicit Jolt headers in domain code ✓

### User Story Traceability

| Story | Title | Addressed | Notes |
|---|---|---|---|
| US-01 | Soft Body Domain Types | ✓ | SoftBodyDesc, SoftBodyMeshData, BodyType::SOFT |
| US-02 | PhysicsSimulator Soft Body API | ✓ | add_soft_body, is_soft_body, get_soft_body_mesh |
| US-03 | JoltPhysicsSimulator Creation | ✓ | Jolt XPBD soft body grid + constraints |
| US-04 | JoltPhysicsSimulator Extraction | ✓ | get_soft_body_mesh implementation |
| US-05 | DeformableMesh Ray Intersection | ✓ | Moller-Trumbore + smooth normals |
| US-06 | DeformableMesh Vertex Update | ✓ | update_vertices + normal recomputation |
| US-07 | AnimationRenderer Soft Body Loop | ✓ | Per-frame mesh updates (M-01 needs clarification) |
| US-08 | YAML soft_body_cube | ✓ | YamlSceneLoader extension |
| US-09 | YAML letter Object | ✓ | FontMeshGenerator integration |
| US-10 | Font Glyph to 3D Mesh | ✓ | ttf2mesh adapter |
| US-11 | Letter Convex Decomposition | ✓ | V-HACD adapter |
| US-12 | Scene Validation | ✓ | Validator soft body parameter checks |
| US-13 | Demo Scene | ✓ | jelly_e.yaml specification |

**Overall**: All 13 stories covered ✓

### Technology Choice Justification

| Technology | Choice | Justification | Status |
|---|---|---|---|
| Jolt XPBD | Use existing v5.2.0 soft body module | Already integrated, proven | ✓ JUSTIFIED |
| ttf2mesh | Primary, FreeType+earcut fallback | Minimal footprint, ADR-001 documented | ✓ JUSTIFIED |
| V-HACD | Primary, CoACD alternative noted | Mature, widely used, ADR-002 documented | ✓ JUSTIFIED |
| Default Font | Open Sans / Roboto / Liberation Sans | Open-source, bundled | ✓ JUSTIFIED |

### Data Flow Correctness

| Flow | Diagram | Correctness | Notes |
|---|---|---|---|
| Per-frame physics-to-rendering | architecture-design.md §4 (lines 177-203) | ✓ CORRECT | Sequence order: step -> extract -> update -> render |
| Font mesh pipeline | data-models.md §10 (lines 211-226) | ✓ CORRECT | Scene load time only, no per-frame overhead |
| Soft body per-frame | data-models.md §11 (lines 229-244) | ✓ CORRECT | Proper separation of concerns |

### Implementation Feasibility

| Aspect | Assessment | Notes |
|---|---|---|
| Estimated Effort | 18-22 days (user-stories.md) | Reasonable for 13 stories |
| New Dependencies | ttf2mesh (2 files), V-HACD (FetchContent) | No heavy transitive deps |
| Jolt Integration | Extends existing v5.2.0 | No version upgrade needed |
| Backward Compatibility | ✓ Preserved | Existing scenes unaffected |
| Testing Strategy | 13 user story test cases | Comprehensive coverage |

**Overall**: Feasible within estimated timeline ✓

---

## Recommendation Summary

| Category | Status | Evidence |
|---|---|---|
| **Architectural Design** | ✓ SOUND | Clean rings, proper boundaries, no coupling |
| **Technical Completeness** | ⚠ CONDITIONAL | 8 issues identified; 1 critical, 4 major require clarification |
| **User Story Coverage** | ✓ COMPLETE | All 13 stories addressed |
| **Technology Choices** | ✓ JUSTIFIED | ADRs provided, alternatives documented |
| **Implementation Readiness** | ⚠ NEEDS CLARIFICATION | Critical and major issues must be resolved |

### Final Status: CONDITIONALLY_APPROVED

**Conditions**:

1. **CRITICAL (C-01)**: Resolve PhysicsBodyDesc convex_hulls integration with pseudo-code
2. **MAJOR (M-01)**: Add per-frame update loop diagram for mixed rigid/soft scenes
3. **MAJOR (M-02)**: Choose and document SoftBodyMeshData face_indices caching strategy
4. **MAJOR (M-03)**: Provide complete BodyType switch statement audit checklist
5. **MAJOR (M-04)**: Add normal recomputation pseudocode with edge case handling

**Timeline**: Resolve all conditions in 1-2 days, then gate user story implementation on approval.

---

## Approval Authority Sign-Off

**Reviewer**: Atlas (Solution Architecture Reviewer)
**Model**: Haiku 4.5
**Review Date**: 2026-02-19
**Approval Condition**: Pending resolution of critical and major issues

**Next Steps**:
1. Architecture author addresses all findings
2. Updated documents published
3. Code implementation gates on this approval
4. User story delivery begins post-approval

---

**Document Version**: 1.0
**Last Updated**: 2026-02-19 00:45 UTC
**Status**: OPEN for issue resolution
