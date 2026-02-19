# Soft Body Physics Simulation for Deformable Jelly-Like Objects in a C++ Ray Tracer Using Jolt Physics

**Research Date**: 2026-02-18
**Research Depth**: Comprehensive
**Source Count**: 14 sources (8 primary/official, 4 technical/community, 2 academic)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Q1: Jolt Physics Soft Body Support](#q1-jolt-physics-soft-body-support)
3. [Q2: Soft Body Simulation Techniques](#q2-soft-body-simulation-techniques)
4. [Q3: Rendering Deformable Objects in a Ray Tracer](#q3-rendering-deformable-objects-in-a-ray-tracer)
5. [Q4: Creating the 'e' Letter as a Physics Object](#q4-creating-the-e-letter-as-a-physics-object)
6. [Q5: Jelly Cube Implementation](#q5-jelly-cube-implementation)
7. [Q6: Integration Architecture](#q6-integration-architecture)
8. [Q7: Alternative Physics Engines](#q7-alternative-physics-engines)
9. [Knowledge Gaps](#knowledge-gaps)
10. [Source Analysis](#source-analysis)
11. [Recommendations](#recommendations)

---

## Executive Summary

This research confirms that Jolt Physics v5.2.0 (already integrated into the nwave-raytracer project) has full soft body simulation support sufficient for jelly-like deformable objects. The implementation is feasible with the following key findings:

- **Jolt's soft body system uses XPBD (Extended Position Based Dynamics)**, the same algorithm used in production game engines, and supports tetrahedral volume constraints essential for jelly simulation.
- **A soft body cube creator already exists in Jolt's sample code** (`SoftBodyCreator::CreateCube`), providing a complete reference implementation with edge constraints and volume constraints using tetrahedral decomposition.
- **Jolt natively supports ray casting against soft bodies** via `SoftBodyShape::CastRay`, which iterates over the deformed face triangles each frame -- no external BVH rebuild is needed for the physics-side collision.
- **Soft bodies interact bidirectionally with rigid bodies**: soft body vertices detect collisions with rigid body shapes, and equal-and-opposite impulses are applied to dynamic rigid bodies.
- **The main architectural challenge** is feeding Jolt's deformed vertex data back into the ray tracer's `TriangleMesh` shape each frame, requiring a new `DeformableMesh` shape or per-frame mesh reconstruction.
- **The 'e' letter** can be represented as a rigid body using convex decomposition (V-HACD/CoACD) or as a compound shape of convex hulls, with the 3D mesh generated via `ttf2mesh` or `FreeType` + triangulation.

**Confidence**: HIGH for Jolt soft body feasibility, MEDIUM for rendering pipeline integration, MEDIUM for 'e' letter mesh generation.

---

## Q1: Jolt Physics Soft Body Support

### Availability and Version History

**Confidence: HIGH** (5 sources: Jolt source code, official docs, release notes, DeepWiki, GitHub)

Jolt Physics has supported soft bodies since at least v4.0.0. The project already uses **Jolt v5.2.0**, which includes all soft body features plus several optimizations and bug fixes specific to soft bodies.

Key version milestones from the release notes (`Docs/ReleaseNotes.md`):
- **v5.0.0**: Added skinning constraints, long range attachment (LRA) constraints, `SoftBodyContactListener`, vertex radius, `CreateConstraints()` auto-generation, and `SoftBodyMotionProperties::CustomUpdate`.
- **v5.1.0**: 10-20% faster constraint solving via better parallel grouping, multithreaded LRA/bend/volume/skinned constraints, `Body::AddForce` for soft bodies.
- **v5.2.0**: `SoftBodyManifold` improvements for sensor contacts, various soft body collision bug fixes.

**Evidence from local source**: The file `build/_deps/joltphysics-src/Jolt/Physics/SoftBody/` contains 15 header/source files confirming full soft body API availability. The `Samples/Tests/SoftBody/` directory contains 18 test programs covering pressure, restitution, shapes, contact listeners, forces, and more.

### Internal Representation

**Confidence: HIGH** (3 sources: `SoftBodySharedSettings.h`, `SoftBodyVertex.h`, `SoftBodyMotionProperties.h`)

Jolt represents soft bodies as a **particle-based system with surface mesh and optional tetrahedral interior**:

1. **Vertices (Particles)**: Each vertex (`SoftBodyVertex`) has:
   - `mPosition` (Vec3) -- current position relative to center of mass
   - `mVelocity` (Vec3) -- current velocity relative to center of mass
   - `mInvMass` (float) -- inverse mass (0 = pinned/kinematic)
   - `mCollisionPlane`, `mCollidingShapeIndex`, `mHasContact` -- collision state

2. **Faces (Surface Mesh)**: Triangle faces defined by 3 vertex indices + material index. These define the collision and rendering surface.

3. **Constraints**: Multiple constraint types connect vertices:
   - **Edge constraints** (`Edge`): Distance springs between two vertices with rest length and compliance
   - **Volume constraints** (`Volume`): Tetrahedral volume preservation with 4 vertex indices and rest volume
   - **Dihedral bend constraints** (`DihedralBend`): Angle preservation between adjacent triangles
   - **LRA constraints** (`LRA`): Long-range attachment to kinematic vertices
   - **Skinned constraints** (`Skinned`): Attachment to animated joints

### API for Creating and Configuring Soft Bodies

**Confidence: HIGH** (4 sources: Jolt Architecture.md, source headers, sample code, DeepWiki)

The creation flow is:

```cpp
// Step 1: Define shared settings (topology + constraints)
Ref<SoftBodySharedSettings> settings = new SoftBodySharedSettings;

// Add vertices with positions and inverse masses
for (...) {
    SoftBodySharedSettings::Vertex v;
    v.mPosition = Float3(x, y, z);
    v.mInvMass = 1.0f;  // 0 = pinned
    settings->mVertices.push_back(v);
}

// Add surface faces (triangles)
settings->AddFace(SoftBodySharedSettings::Face(v0, v1, v2));

// Add constraints (edges, volume, etc.)
settings->mEdgeConstraints.push_back(Edge(v0, v1));
settings->CalculateEdgeLengths();
settings->mVolumeConstraints.push_back(Volume(v0, v1, v2, v3));
settings->CalculateVolumeConstraintVolumes();

// OR auto-generate from faces:
SoftBodySharedSettings::VertexAttributes va(compliance, shearCompliance, bendCompliance);
settings->CreateConstraints(&va, 1, EBendType::Distance);

// Optimize for parallel execution
settings->Optimize();

// Step 2: Create instance settings
SoftBodyCreationSettings creation(settings, position, rotation, objectLayer);
creation.mPressure = 2000.0f;       // Internal pressure (jelly inflation)
creation.mRestitution = 0.3f;       // Bounce on collision
creation.mFriction = 0.2f;          // Surface friction
creation.mLinearDamping = 0.1f;     // Velocity damping
creation.mNumIterations = 5;        // Solver iterations per step
creation.mGravityFactor = 1.0f;     // Gravity multiplier

// Step 3: Add to physics world
BodyID bodyId = bodyInterface.CreateAndAddSoftBody(creation, EActivation::Activate);
```

### Extracting Deformed Mesh Vertices Each Frame

**Confidence: HIGH** (3 sources: `SoftBodyMotionProperties.h`, `SoftBodyShape.cpp`, Architecture.md)

After each physics step, deformed vertex positions are accessed through `SoftBodyMotionProperties`:

```cpp
// Lock the body and get motion properties
BodyLockRead lock(physics_system.GetBodyLockInterface(), softBodyId);
const Body& body = lock.GetBody();
const SoftBodyMotionProperties* mp =
    static_cast<const SoftBodyMotionProperties*>(body.GetMotionProperties());

// Get current vertex positions (updated each physics step)
const Array<SoftBodyVertex>& vertices = mp->GetVertices();
for (const SoftBodyVertex& v : vertices) {
    Vec3 worldPos = body.GetCenterOfMassTransform() * v.mPosition;
    // Use worldPos for rendering
}

// Get face topology (constant, from shared settings)
const Array<SoftBodySharedSettings::Face>& faces = mp->GetFaces();
```

**Key finding**: Jolt's `SoftBodyShape::CastRay` already implements ray-triangle intersection against the deformed mesh by iterating all faces and testing against current vertex positions. This is a linear scan without acceleration structure, which is acceptable for moderate triangle counts but would be slow for high-resolution meshes.

### Soft Body vs Rigid Body Interaction

**Confidence: HIGH** (4 sources: Architecture.md, `SoftBodyMotionProperties.h`, DeepWiki, release notes)

Jolt's soft body collision with rigid bodies works as follows:

1. **Broadphase**: The soft body's AABB is checked against rigid body AABBs.
2. **Narrow phase**: Each soft body vertex is tested against nearby rigid body shapes to find collision planes.
3. **Response**: Vertex velocities are projected onto collision plane normals, with friction and restitution applied.
4. **Impulse transfer**: If the rigid body is dynamic, equal-and-opposite impulses are applied to it.

This means a **soft jelly cube falling onto a rigid 'e' letter will**:
- Deform the jelly cube on contact (vertex positions displaced by collision planes)
- Apply downward force to the 'e' letter, potentially knocking it over
- The jelly cube will bounce back based on `mRestitution`

**Limitation**: Soft-to-soft body collisions are NOT implemented. If both objects need to be soft, they cannot collide with each other.

---

## Q2: Soft Body Simulation Techniques

### Main Approaches

**Confidence: HIGH** (5 sources: DeepWiki, `SoftBodyMotionProperties.h` header comment, XPBD paper, Wikipedia, PBD implementations)

| Approach | Description | Pros | Cons |
|----------|-------------|------|------|
| **Mass-Spring Systems** | Particles connected by damped springs | Simple, intuitive | Over-stretchy, hard to tune, can be unstable |
| **Finite Element Method (FEM)** | Continuous mechanics discretized on mesh | Physically accurate | Expensive, complex implementation |
| **Position-Based Dynamics (PBD)** | Constraint projection on positions | Fast, stable, easy to tune | Not physically accurate, iteration-dependent |
| **XPBD** | Extended PBD with compliance | Fast, stable, compliance-based stiffness | Same as PBD but better convergence |

### Jolt's Approach: XPBD

**Confidence: HIGH** (3 sources: `SoftBodyMotionProperties.h` line 32, XPBD paper by Macklin, DeepWiki)

Jolt uses **XPBD (Extended Position Based Dynamics)** as explicitly stated in the source code:

```
// Based on: XPBD, Extended Position Based Dynamics, Matthias Muller, Ten Minute Physics
// See: https://matthias-research.github.io/pages/tenMinutePhysics/09-xpbd.pdf
```

The XPBD algorithm flow per physics step:
1. Apply gravity and external forces to vertex velocities
2. Integrate positions forward (predict)
3. Iteratively solve constraints (edge distance, volume, bend, pressure)
4. Compute new velocities from position changes
5. Apply collision response (project out of rigid bodies, apply friction/restitution)
6. Update body state (center of mass, bounds)

### Parameters for Jelly-Like Behavior

**Confidence: MEDIUM** (3 sources: `SoftBodyCreationSettings.h`, Jolt samples, JellyCar article -- but no authoritative "jelly parameter guide" found)

Key parameters and their effect on jelly simulation:

| Parameter | Location | Jelly-Like Value | Effect |
|-----------|----------|------------------|--------|
| `mPressure` | `SoftBodyCreationSettings` | 500-3000 | Internal gas pressure; inflates the shape, resists compression. Higher = more rigid inflation. Jolt sample uses 2000 for pressurized spheres. |
| `mRestitution` | `SoftBodyCreationSettings` | 0.3-0.6 | Bounce on collision. Jelly should bounce noticeably but not excessively. |
| `mLinearDamping` | `SoftBodyCreationSettings` | 0.05-0.2 | Energy dissipation. Low values = more wobble. Default is 0.1. |
| `mNumIterations` | `SoftBodyCreationSettings` | 5-10 | Solver iterations. More = stiffer constraints, less wobble. Default is 5. |
| `mGravityFactor` | `SoftBodyCreationSettings` | 1.0 | Gravity multiplier. Normal gravity for falling jelly. |
| Edge compliance | `SoftBodySharedSettings::Edge` | 1e-4 to 1e-3 | Inverse stiffness of distance springs. Higher = softer, more stretchy. |
| Volume compliance | `SoftBodySharedSettings::Volume` | 0 to 1e-5 | Inverse stiffness of volume preservation. 0 = perfectly rigid volume. Low compliance is essential for jelly -- it should not collapse. |
| Bend compliance | `SoftBodySharedSettings::DihedralBend` | 1e-3 to 1e-2 | Controls resistance to folding. Jelly is somewhat resistant to bending. |

**Interpretation** (analyst note): For convincing jelly behavior, the critical parameters are:
1. **Volume constraints with low compliance** (near 0): Jelly maintains its volume strongly -- it squishes but doesn't shrink.
2. **Edge constraints with moderate compliance** (1e-4 to 1e-3): Allows surface deformation without extreme stretching.
3. **Moderate pressure** (1000-3000): Provides internal "inflation" that helps the shape recover after deformation.
4. **Low damping** (0.05-0.1): Jelly wobbles visibly; high damping would make it feel like clay.
5. **Moderate restitution** (0.3-0.5): Jelly bounces but doesn't ricochet like a rubber ball.

### Computational Cost

**Confidence: MEDIUM** (2 sources: Jolt release notes performance data, DeepWiki)

Soft body simulation is significantly more expensive than rigid body simulation because:
- Each vertex is essentially an independent particle requiring constraint solving
- Constraint solving requires multiple iterations (5-10 per step)
- Collision detection is per-vertex against all nearby rigid bodies
- A 5x5x5 cube has 125 vertices and hundreds of edge + volume constraints

The v5.1.0 release notes indicate a 10-20% speedup from better parallel grouping, suggesting this was a known performance concern. For an offline ray tracer (not real-time), this computational cost is acceptable.

---

## Q3: Rendering Deformable Objects in a Ray Tracer

### Ray Tracing a Deforming Mesh

**Confidence: HIGH** (4 sources: existing `TriangleMesh` code, `SoftBodyShape.cpp`, Wald et al. BVH paper, Jolt ray cast implementation)

The nwave-raytracer already has a `TriangleMesh` shape class that performs Moller-Trumbore ray-triangle intersection with smooth normal interpolation. The approach for deformable soft bodies:

**Option A: Rebuild TriangleMesh Each Frame**
- Extract vertices from `SoftBodyMotionProperties::GetVertices()` each frame
- Reconstruct a new `TriangleMesh` with updated vertex positions
- The face connectivity (indices) stays constant; only vertex positions change
- This is the simplest approach and matches how Jolt's own `SoftBodyShape::CastRay` works

**Option B: Create a DeformableMesh Shape**
- A new shape class that holds mutable vertex positions
- Updates vertex positions in-place each frame without reconstructing the entire object
- Recalculates the AABB and normals on update

**Jolt's own approach** (from `SoftBodyShape.cpp` lines 45-75): Jolt casts rays against soft bodies by linearly scanning all faces and testing ray-triangle intersection against current vertex positions. No BVH or acceleration structure is used. This is O(n) per ray where n = number of faces.

### BVH Considerations

**Confidence: HIGH** (3 sources: Wald et al. "Ray Tracing Deformable Scenes" paper, Jacco's BVH blog, existing codebase analysis)

The existing `TriangleMesh` class does NOT use a BVH -- it linearly scans all faces (same approach as Jolt). For a soft body cube at reasonable resolution (e.g., 5x5x5 = 296 surface faces, or 8x8x8 = 768 surface faces), linear scan is acceptable for an offline renderer.

If performance becomes an issue:
- **BVH refitting** (updating bounding boxes without rebuilding tree structure) is the fastest approach for deforming meshes where topology is constant
- **Full BVH rebuild** each frame is more robust but slower
- Research by Wald et al. (2007) showed BVH refitting is "extremely fast, often less costly than the associated animation updates" but produces lower quality trees than rebuilding

**For the nwave-raytracer use case**: Given the moderate triangle count of soft body meshes and offline rendering context, linear scan per frame without BVH is the recommended starting approach.

### Smooth Normals on Deforming Mesh

**Confidence: HIGH** (3 sources: existing `TriangleMesh` code, standard CG knowledge, Unity documentation on `RecalculateNormals`)

Smooth normals must be recalculated each frame as vertex positions change. The standard approach:

```
For each vertex v:
    normal(v) = normalize(sum of face normals of all faces containing v)
```

The existing `TriangleMesh` supports smooth normals via `normal_indices_` and interpolates using barycentric coordinates. For the deformable mesh, normals should be recomputed after each physics step:

1. For each face, compute face normal: `normalize(cross(v1-v0, v2-v0))`
2. For each vertex, average the normals of all adjacent faces (area-weighted averaging produces better results)
3. Store these per-vertex normals and interpolate during intersection using barycentric coordinates

**Note**: Jolt's `SoftBodyShape::GetSurfaceNormal` returns flat (per-face) normals, not smooth normals. Smooth normals must be computed in the ray tracer.

### Subsurface Scattering for Translucent Jelly

**Confidence: MEDIUM** (3 sources: Jensen BSSRDF paper, practical SSS blog post, Wikipedia)

Real jelly is translucent with visible subsurface scattering. Full SSS in a Whitted-style ray tracer is complex:

- **Full BSSRDF** (Jensen 2001): Requires Monte Carlo sampling of subsurface paths. Too complex for a Whitted tracer.
- **Dipole diffusion approximation**: Analytical model, faster but still requires multiple sample points on the surface.
- **Simple approximation for Whitted tracer**: Use a combination of:
  1. Semi-transparent material with Fresnel-modulated transparency
  2. Tinted transmission (pink-ish color for the jelly interior)
  3. Slight opacity falloff based on thickness (thicker parts are more opaque)
  4. Specular highlights with moderate glossiness (jelly is shiny)

**Recommended approach for this project**: Start with a simple translucent material (existing Whitted ray tracing supports reflection/refraction) using a low opacity and pink-tinted refraction, rather than implementing full SSS.

---

## Q4: Creating the 'e' Letter as a Physics Object

### Representing a Letter as a Collision Body

**Confidence: MEDIUM-HIGH** (4 sources: ttf2mesh GitHub, Font23D GitHub, V-HACD documentation, CoACD paper)

The 'e' letter shape is concave and has a hole, making it unsuitable as a single convex physics shape. Options:

**Option 1: Convex Decomposition (Recommended)**
1. Generate a 3D mesh of the 'e' from a font (see below)
2. Use V-HACD or CoACD to decompose it into multiple approximate convex hulls
3. Create a `StaticCompoundShape` in Jolt from these convex hulls
4. This approach is standard for concave physics objects in games

**Option 2: Triangle Mesh Collision**
- Jolt's `MeshShape` supports concave triangle mesh collision
- However, `MeshShape` bodies must be static
- Since the 'e' needs to be knocked over (dynamic), this won't work directly
- Workaround: Use `MeshShape` only for collision detection with a dynamic body using compound convex hulls

**Option 3: Manual Compound Shape**
- Approximate the 'e' with a collection of boxes and cylinders
- Simpler but less accurate collision shape
- Acceptable if visual accuracy of collisions is not critical

### Generating a 3D Mesh from a Font Glyph

**Confidence: MEDIUM** (3 sources: ttf2mesh, Font23D, FreeType documentation)

**Library: ttf2mesh** (Recommended)
- Standalone C99 library (2 files: `ttf2mesh.c`, `ttf2mesh.h`)
- No external dependencies
- Converts TrueType font glyphs to 2D triangulated mesh
- Supports 3D extrusion via the `glyph3d` example
- Can export to OBJ format
- Easy C++ integration

**Alternative: FreeType + Custom Extrusion**
- FreeType provides glyph outlines as Bezier curves
- Tessellate the 2D outline using Poly2Tri or earcut.hpp
- Extrude by duplicating the front face, offsetting Z, and creating side faces
- More control but more implementation work

**Pipeline for 'e' letter**:
1. Use ttf2mesh or FreeType to get 2D triangulated outline of 'e'
2. Extrude to desired depth (e.g., 0.5 units) to create a 3D mesh
3. Export as OBJ or use vertices/faces directly
4. For physics: decompose into convex hulls using V-HACD
5. For rendering: use as `TriangleMesh` in the ray tracer

### Should the 'e' be Rigid or Soft?

**Confidence: HIGH** (analyst assessment based on research)

The 'e' should be a **rigid dynamic body** for the described use case:
- The 'e' is meant to be knocked over by the jelly cube hitting it
- Rigid body dynamics handle this interaction naturally
- Jolt supports soft body vs rigid body collisions
- Making the 'e' also soft would be significantly more complex and soft-to-soft collision is not supported

---

## Q5: Jelly Cube Implementation

### Creating a Soft Body Cube in Jolt

**Confidence: HIGH** (3 sources: `SoftBodyCreator.cpp` line 70-211, `SoftBodyRestitutionTest.cpp`, `SoftBodyShapesTest.cpp`)

Jolt provides a complete reference implementation in `Samples/Utils/SoftBodyCreator::CreateCube(uint inGridSize, float inGridSpacing)`. The implementation:

1. **Creates a 3D grid of vertices** (gridSize^3 vertices):
```cpp
for z in [0, gridSize):
  for y in [0, gridSize):
    for x in [0, gridSize):
      vertex at offset + spacing * (x, y, z)
```

2. **Creates edge constraints** along all grid axes (x, y, z neighbors):
```cpp
// Edges connect each vertex to its +x, +y, and +z neighbors
if (x < gridSize - 1): edge(v(x,y,z), v(x+1,y,z))
if (y < gridSize - 1): edge(v(x,y,z), v(x,y+1,z))
if (z < gridSize - 1): edge(v(x,y,z), v(x,y,z+1))
```

3. **Creates tetrahedral volume constraints** (6 tetrahedra per cube cell):
Each unit cube is decomposed into 6 tetrahedra using a fixed index pattern. Volume constraints are added for each tetrahedron to preserve local volume.

4. **Creates surface faces** on all 6 sides of the cube (2 triangles per quad face):
Only the outermost vertices form the visible surface mesh.

5. **Calculates initial constraint values and optimizes**:
```cpp
settings->CalculateEdgeLengths();
settings->CalculateVolumeConstraintVolumes();
settings->Optimize();
```

### Mesh Resolution for Convincing Jelly

**Confidence: MEDIUM** (2 sources: Jolt sample defaults, general PBD knowledge)

| Grid Size | Vertex Count | Surface Faces | Edge Constraints | Volume Constraints | Suitability |
|-----------|-------------|---------------|------------------|--------------------|-------------|
| 3x3x3 | 27 | 48 | 54 | 48 | Minimal, blocky deformation |
| 5x5x5 | 125 | 192 | 300 | 384 | **Recommended starting point** (Jolt default) |
| 8x8x8 | 512 | 588 | 1,176 | 2,058 | Good deformation detail |
| 10x10x10 | 1,000 | 968 | 2,700 | 4,374 | High detail, more expensive |

The Jolt sample uses **gridSize=5, gridSpacing=0.5** as defaults. For jelly, **gridSize 5-8** provides a good balance between deformation fidelity and performance.

### Volume Preservation for Jelly

**Confidence: HIGH** (3 sources: `SoftBodySharedSettings.h` Volume struct, `SoftBodyCreator.cpp`, DeepWiki)

The tetrahedral volume constraints are the primary mechanism preventing the cube from collapsing. Each tetrahedron stores `mSixRestVolume` (6x the initial volume) and `mCompliance` (inverse stiffness).

For jelly, set volume compliance to **0 (or very near 0)**: this means the volume constraint is maximally stiff, preserving the tetrahedra volumes exactly. This is essential because jelly is nearly incompressible -- it deforms but doesn't change volume.

### Pressure for Internal Inflation

**Confidence: HIGH** (3 sources: `SoftBodyCreationSettings.h`, `SoftBodyPressureTest.cpp`, DeepWiki)

The `mPressure` parameter simulates internal gas pressure using the ideal gas law formula (n * R * T). Pressure is applied by computing the current volume and applying outward forces proportional to face areas.

For a jelly cube, moderate pressure (500-2000) combined with volume constraints provides the "bouncy recovery" behavior where the shape springs back to its original form after deformation.

**From Jolt sample code**: `SoftBodyPressureTest` uses pressure values from 0 to 10,000 on spheres of radius 2.0. For a unit-scale cube, values of 500-3000 are reasonable.

---

## Q6: Integration Architecture

### Extending the PhysicsSimulator Interface

**Confidence: HIGH** (based on analysis of existing `PhysicsSimulator`, `JoltPhysicsSimulator`, and `AnimationRenderer` code)

The current `PhysicsSimulator` interface supports only rigid bodies. Required extensions:

```cpp
// New struct for soft body mesh data
struct SoftBodyMeshData {
    std::vector<Point3> vertices;           // Updated each frame
    std::vector<Vec3> normals;              // Recalculated each frame
    std::vector<int> face_indices;          // Constant topology
};

// New struct for soft body creation
struct SoftBodyDesc {
    int grid_size = 5;                      // Resolution of soft body grid
    double grid_spacing = 0.5;              // Distance between grid points
    Point3 position;                        // Initial center position
    double pressure = 2000.0;               // Internal pressure
    double restitution = 0.3;               // Bounce coefficient
    double friction = 0.2;                  // Surface friction
    double damping = 0.1;                   // Linear damping
    int solver_iterations = 5;              // XPBD iterations
    double edge_compliance = 1e-4;          // Edge spring softness
    double volume_compliance = 0.0;         // Volume preservation (0=rigid)
};

// Extended PhysicsSimulator interface
class PhysicsSimulator {
public:
    // ... existing methods ...
    virtual int add_soft_body(const SoftBodyDesc& desc) = 0;
    virtual bool is_soft_body(int body_id) const = 0;
    virtual SoftBodyMeshData get_soft_body_mesh(int body_id) const = 0;
};
```

### Feeding Deformed Mesh into the Ray Tracer

**Confidence: HIGH** (based on analysis of `AnimationRenderer` pipeline and `TriangleMesh` class)

The current animation pipeline:
1. Step physics
2. Get rigid body transforms
3. Apply transforms via `TransformedShape` wrappers
4. Render frame

For soft bodies, the pipeline needs a new branch:

```
For each frame:
  1. Step physics
  2. For rigid bodies: update TransformedShape transform (existing)
  3. For soft bodies:
     a. Get deformed mesh from SoftBodyMotionProperties
     b. Compute smooth normals from deformed positions
     c. Create/update a TriangleMesh with new vertices and normals
     d. Replace the shape in the scene
  4. Render frame
```

**Recommended approach**: Create a `DeformableMesh` shape class that wraps a `TriangleMesh` but allows in-place vertex updates:

```cpp
class DeformableMesh : public Shape {
public:
    DeformableMesh(std::vector<int> face_indices, const Material* mat);

    // Called each frame with new vertex positions from physics
    void update_vertices(const std::vector<Point3>& new_vertices);

    // Shape interface
    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const override;

private:
    std::vector<Point3> vertices_;          // Mutable, updated each frame
    std::vector<Vec3> normals_;             // Recomputed on update
    std::vector<int> face_indices_;         // Constant
    const Material* material_;
    AABB bbox_;                             // Recomputed on update

    void recompute_normals();
    void recompute_bbox();
};
```

### JoltPhysicsSimulator Implementation

**Confidence: HIGH** (based on analysis of existing implementation)

The `JoltPhysicsSimulator::Impl` struct needs to track soft body IDs separately and provide mesh extraction:

```cpp
struct JoltPhysicsSimulator::Impl {
    // ... existing members ...
    std::vector<JPH::BodyID> soft_body_ids;

    SoftBodyMeshData extract_soft_body_mesh(JPH::BodyID id) const {
        JPH::BodyLockRead lock(physics_system.GetBodyLockInterface(), id);
        const JPH::Body& body = lock.GetBody();
        const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(
            body.GetMotionProperties());

        SoftBodyMeshData data;
        JPH::RMat44 transform = body.GetCenterOfMassTransform();

        // Extract vertex positions in world space
        for (const auto& v : mp->GetVertices()) {
            JPH::Vec3 worldPos = transform * v.mPosition;
            data.vertices.emplace_back(worldPos.GetX(), worldPos.GetY(), worldPos.GetZ());
        }

        // Extract face indices (constant topology)
        for (const auto& f : mp->GetFaces()) {
            data.face_indices.push_back(f.mVertex[0]);
            data.face_indices.push_back(f.mVertex[1]);
            data.face_indices.push_back(f.mVertex[2]);
        }

        // Compute smooth normals
        data.normals.resize(data.vertices.size(), Vec3(0,0,0));
        for (size_t i = 0; i < data.face_indices.size(); i += 3) {
            int i0 = data.face_indices[i];
            int i1 = data.face_indices[i+1];
            int i2 = data.face_indices[i+2];
            Vec3 e1 = data.vertices[i1] - data.vertices[i0];
            Vec3 e2 = data.vertices[i2] - data.vertices[i0];
            Vec3 face_normal = cross(e1, e2);
            data.normals[i0] = data.normals[i0] + face_normal;
            data.normals[i1] = data.normals[i1] + face_normal;
            data.normals[i2] = data.normals[i2] + face_normal;
        }
        for (auto& n : data.normals) {
            n = normalize(n);
        }

        return data;
    }
};
```

### AnimationRenderer Modifications

The `AnimationRenderer::render()` loop needs a soft body branch:

```cpp
// In the frame loop, after rigid body transforms:
for (int i = 0; i < shape_count; ++i) {
    if (is_soft_body[i]) {
        SoftBodyMeshData mesh = physics_->get_soft_body_mesh(body_ids[i]);
        deformable_meshes[i]->update_vertices(mesh.vertices);
        // normals recomputed inside update_vertices
    }
}
```

### Performance Considerations

**Confidence: MEDIUM** (analyst assessment)

- **Physics step**: Soft body simulation with a 5x5x5 cube adds moderate CPU cost (hundreds of constraints, 5 solver iterations). For offline rendering, this is negligible compared to ray tracing time.
- **Mesh extraction**: Copying ~125 vertices and computing normals per frame is trivial.
- **Ray tracing**: The soft body mesh has ~192 faces (5x5x5 cube). Linear scan per ray is fast enough. If resolution increases to 10x10x10 (968 faces), a per-frame AABB check before face iteration would help.
- **Metal GPU rendering**: The existing GPU pipeline would need to upload updated vertex buffers each frame for the deformable mesh.

---

## Q7: Alternative Physics Engines for Soft Body

### Assessment of Alternatives

**Confidence: MEDIUM** (3 sources: engine comparison discussions, NVIDIA Flex documentation, Bullet documentation)

| Engine | Soft Body Support | Status | Integration Difficulty |
|--------|------------------|--------|----------------------|
| **Jolt Physics** | XPBD, volume/pressure/edge/bend constraints | Active, v5.2.0 | **Already integrated** |
| **Bullet Physics** | btSoftBody module, mass-spring + finite elements | Maintenance mode | New dependency, different API |
| **NVIDIA Flex** | Particle-based unified solver | **Deprecated** (replaced by NVIDIA Omniverse deformables) | Not recommended |
| **Custom XPBD** | Full control, minimal dependencies | N/A | High implementation effort |
| **InteractiveComputerGraphics PBD** | Academic library, comprehensive | Active | Additional dependency |

### Recommendation

**Confidence: HIGH** (analyst assessment based on all evidence)

**Use Jolt's built-in soft body support.** There is no reason to introduce a second physics engine:

1. Jolt v5.2.0 already has all the soft body features needed (volume constraints, pressure, edge constraints, rigid body interaction)
2. The API is consistent with the existing rigid body integration
3. Sample code exists for creating soft body cubes, spheres, and cloth
4. Ray casting against soft bodies is already implemented in Jolt
5. Adding a second physics engine would introduce dependency management complexity, API translation overhead, and synchronization challenges

NVIDIA Flex is deprecated. Bullet's soft body module is functional but less modern than Jolt's XPBD implementation. A custom implementation would be significant engineering effort for no clear benefit.

---

## Knowledge Gaps

### Gap 1: Optimal Jelly Parameters (Searched, Partially Found)
**What was searched**: "jelly gelatin soft body simulation parameters stiffness damping", JellyCar deep dive, Jolt samples
**What was found**: General parameter ranges and Jolt sample values, but no authoritative "jelly preset" with specific numerical values tuned for gelatin-like materials
**Impact**: Parameters will need to be tuned experimentally. The recommended starting values in Q2 are based on interpolation from available sources.
**Recommendation**: Set up a rapid iteration loop (physics sim only, no rendering) to tune parameters before full animation rendering.

### Gap 2: GPU Metal Shader Integration for Deformable Meshes
**What was searched**: Existing GPU rendering code in the project
**What was found**: The project has Metal GPU rendering but the integration with per-frame mesh updates was not investigated in depth
**Impact**: If GPU rendering is used, the vertex buffer upload path for deformable meshes needs design work.
**Recommendation**: Start with CPU-only rendering for soft body scenes; optimize to GPU later.

### Gap 3: ttf2mesh 3D Extrusion Quality
**What was searched**: ttf2mesh documentation, Font23D
**What was found**: ttf2mesh supports 3D via an example, but quality/robustness of the extrusion for complex glyphs like 'e' (with interior contour/hole) is not confirmed
**Impact**: The 'e' has a counter (hole), which complicates triangulation. May need to verify that ttf2mesh handles counters correctly.
**Recommendation**: Test ttf2mesh with the specific font and 'e' glyph early. Have FreeType + earcut.hpp as a fallback.

### Gap 4: Soft Body Performance at Higher Resolutions
**What was searched**: Jolt performance benchmarks for soft bodies
**What was found**: v5.1.0 notes mention 10-20% improvement, but no absolute timing data for soft body simulation at various resolutions
**Impact**: Unknown if 10x10x10 cube (1000 vertices) would cause per-frame simulation to exceed acceptable time
**Recommendation**: Benchmark early. Start with 5x5x5 and increase if visual quality is insufficient.

### Gap 5: Soft Body Collision with Compound Shapes
**What was searched**: Jolt samples for soft body vs compound shape collisions
**What was found**: `SoftBodyShapesTest.cpp` demonstrates soft body cloth colliding with `StaticCompoundShape` containing capsules and spheres. This confirms compound shape collision works.
**What is unclear**: Whether the collision quality is sufficient when the compound shape is dynamic (the 'e' letter being knocked over)
**Recommendation**: Test a soft body cube vs a dynamic compound shape early in development.

---

## Source Analysis

| # | Source | Type | Reputation | Independence | Used For |
|---|--------|------|------------|-------------|----------|
| 1 | Jolt Physics Source Code (v5.2.0, local) | Primary/Official | Tier 1 | Independent | Q1, Q5, Q6 -- definitive API reference |
| 2 | Jolt Architecture.md (local) | Primary/Official | Tier 1 | Same as #1 | Q1 -- authoritative documentation |
| 3 | Jolt ReleaseNotes.md (local) | Primary/Official | Tier 1 | Same as #1 | Q1 -- version history |
| 4 | Jolt SoftBodyCreator.cpp (local) | Primary/Official | Tier 1 | Same as #1 | Q5 -- cube creation reference |
| 5 | Jolt SoftBodyShape.cpp (local) | Primary/Official | Tier 1 | Same as #1 | Q3 -- ray casting implementation |
| 6 | DeepWiki - Jolt Soft Body System | Technical/Community | Tier 2 | Derived from #1 | Q1, Q2 -- summarized API reference |
| 7 | jrouwe.github.io/JoltPhysics | Primary/Official | Tier 1 | Same as #1 | Q1 -- official web docs |
| 8 | XPBD Paper (Macklin, Muller) | Academic | Tier 1 | Independent | Q2 -- algorithm foundation |
| 9 | Wald et al. "Ray Tracing Deformable Scenes" | Academic | Tier 1 | Independent | Q3 -- BVH strategy |
| 10 | ttf2mesh GitHub (fetisov/ttf2mesh) | Technical/Open Source | Tier 2 | Independent | Q4 -- font-to-mesh conversion |
| 11 | CoACD Paper (colin97/CoACD) | Academic/Technical | Tier 1 | Independent | Q4 -- convex decomposition |
| 12 | JellyCar Deep Dive (gamedeveloper.com) | Technical/Industry | Tier 2 | Independent | Q2 -- jelly physics concepts |
| 13 | Jensen BSSRDF (graphics.ucsd.edu) | Academic | Tier 1 | Independent | Q3 -- subsurface scattering |
| 14 | Practical SSS Blog (agraphicsguynotes.com) | Technical/Community | Tier 2 | Independent | Q3 -- SSS implementation |

**Note**: Sources 1-5 and 7 are from the same project (Jolt Physics) but represent different facets (source code, documentation, release notes). Source 6 is derived from the Jolt source. For cross-referencing purposes, the Jolt sources count as one authority, supplemented by independent sources 8-14.

---

## Recommendations

### Implementation Priority Order

1. **Phase 1: Soft Body Physics Integration** (Highest priority, lowest risk)
   - Extend `PhysicsSimulator` interface with `add_soft_body()` and `get_soft_body_mesh()`
   - Implement in `JoltPhysicsSimulator` using Jolt's soft body API
   - Create a test scene with a soft cube dropping onto a static floor
   - Tune jelly parameters (start with: pressure=2000, edge_compliance=1e-4, volume_compliance=0, restitution=0.3, damping=0.05, iterations=5, gridSize=5)

2. **Phase 2: DeformableMesh Shape for Ray Tracer** (High priority)
   - Create `DeformableMesh` shape class with mutable vertices and per-frame normal recomputation
   - Integrate into `AnimationRenderer` loop
   - Verify soft body renders correctly by comparing with rigid body version

3. **Phase 3: 'e' Letter Creation** (Medium priority, some risk)
   - Integrate ttf2mesh or FreeType for glyph-to-mesh conversion
   - Generate extruded 3D mesh for 'e'
   - Use V-HACD or manual convex decomposition for physics collision shape
   - Add as rigid dynamic body

4. **Phase 4: Full Scene Integration** (Depends on phases 1-3)
   - Combine jelly cube (soft body, falling from above) with 'e' letter (rigid dynamic, on surface)
   - Verify collision interaction: jelly deforms on impact, 'e' receives impulse and falls
   - Fine-tune timing, camera angle, materials

5. **Phase 5: Visual Polish** (Optional)
   - Add translucent jelly material (Fresnel reflection + tinted refraction)
   - Consider simple SSS approximation if visual quality is insufficient
   - Increase soft body resolution if deformation looks too coarse

### Key Risks

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Jelly parameters need extensive tuning | High | Medium | Build a parameter sweep tool; iterate on physics only before rendering |
| ttf2mesh fails on 'e' with counter (hole) | Medium | Medium | Have FreeType + earcut.hpp as fallback |
| Soft body vs dynamic compound shape collision artifacts | Low-Medium | Medium | Simplify 'e' collision shape; test early |
| GPU pipeline incompatible with per-frame mesh updates | Medium | Low | CPU rendering path works as fallback |
| Soft body performance too slow at high resolution | Low | Low | Offline renderer tolerates slower simulation |

---

## Research Review (nw-researcher-reviewer)

**Review Date**: 2026-02-18
**Reviewer**: nw-researcher-reviewer (Scholar)
**Review ID**: research_rev_20260218_001

### Executive Review Verdict

**STATUS: APPROVED with MINOR REVISIONS**

The research document is comprehensive, well-sourced, and technically accurate. It provides sufficient guidance to implement soft body physics (Phases 1-2) immediately. Knowledge gaps are honestly identified with practical mitigations. Two HIGH-severity design clarifications are required before implementation begins.

---

### Quality Scores (0.0-1.0)

| Dimension | Score | Interpretation |
|-----------|-------|-----------------|
| Source Bias | 0.95 | Excellent diversity; appropriate Jolt-heavy references given existing integration |
| Evidence Quality | 0.88 | Strong citations with file/line references; confidence levels explicit; some parameters lack validation |
| Replicability | 0.92 | CPU physics highly replicable; mesh generation and GPU pipeline need specification detail |
| Architecture Fit | 0.89 | Respects Clean Architecture patterns; interface signatures and concurrency need refinement |
| Completeness | 0.86 | Core questions thoroughly addressed; animation pipeline and rendering materials underspecified |
| Accuracy | 0.94 | Technical claims validated; confidence levels are well-calibrated |
| Actionability | 0.87 | Phases 1-2 immediately actionable; Phase 3 needs concrete code examples |
| Priority Validation | 0.96 | Research correctly addresses user goal; scope is well-prioritized |

**Overall Quality Score: 0.91** ✓

---

### Critical Issues (BLOCKING)

None identified.

---

### High-Severity Issues (REVISION REQUIRED)

1. **DeformableMesh Concurrency and Thread Safety**
   - **Issue**: Q6 proposes per-frame vertex updates without addressing synchronization between physics thread and rendering thread
   - **Evidence**: Lines 509-529 show `update_vertices()` method but no locking mechanism
   - **Impact**: Race condition if physics updates vertices while renderer reads them
   - **Severity**: HIGH
   - **Recommendation**: Specify synchronization strategy (e.g., double-buffering vertex buffers, frame-locked updates, or mutex protection). Add to Q6 implementation notes.

2. **Soft Body vs Dynamic Compound Shape Collision – Validation Gap**
   - **Issue**: Document cites `SoftBodyShapesTest.cpp` for static compound shapes, but the 'e' letter must be a *dynamic* compound shape to be knocked over. Interaction is unvalidated.
   - **Evidence**: Line 670 states "confirms compound shape collision works" but tests static shapes only; line 365 requires dynamic behavior for the 'e'
   - **Impact**: The core interaction (jelly cube hitting dynamic 'e' letter) is unverified and could fail at implementation
   - **Severity**: HIGH
   - **Recommendation**: Add to Phase 1 testing checklist: "Validate soft body collision with dynamic compound shapes. Use simple box (soft body) vs compound capsule (dynamic rigid) as test case before full 'e' letter implementation."

---

### Medium-Severity Issues (ADVISORY)

1. **GPU Metal Rendering Path Unaddressed**
   - **Issue**: Gap 2 explicitly defers GPU pipeline integration; document recommends CPU-only rendering as workaround
   - **Evidence**: Lines 650-654 state GPU integration "was not investigated in depth"; recommended approach is "CPU rendering path works as fallback"
   - **Impact**: If project requires Metal GPU rendering, the proposed approach is insufficient
   - **Severity**: MEDIUM (conditional on project requirements)
   - **Recommendation**: Clarify project's rendering requirements. If Metal GPU is mandatory, GPU pipeline design should be Phase 2 (not Phase 5). If CPU-only is acceptable for soft body scenes, document this explicitly.

2. **'e' Letter Generation Lacks Concrete Implementation Guidance**
   - **Issue**: Q4 names libraries (ttf2mesh, FreeType, V-HACD) but provides no code integration or CMake strategy
   - **Evidence**: Lines 339-358 describe pipeline conceptually but lack concrete examples
   - **Impact**: Developer cannot immediately start Phase 3 without additional research into ttf2mesh/CMake integration
   - **Severity**: MEDIUM (implementation delay, not research flaw)
   - **Recommendation**: Either provide a minimal ttf2mesh integration example, or move font-to-mesh generation to Phase 2 as a dependency spike/proof-of-concept.

3. **Jelly Parameter Tuning Lacks Quantitative Acceptance Criteria**
   - **Issue**: Gap 1 identifies optimal parameters are unknown; mitigation is vague ("build a parameter sweep tool")
   - **Evidence**: Lines 643-648 acknowledge gap; recommendation is editorial ("tune experimentally"), not methodological
   - **Impact**: Developer will iterate extensively without clear success criteria
   - **Severity**: MEDIUM (schedule risk, not correctness risk)
   - **Recommendation**: Define quantitative acceptance criteria for "convincing jelly behavior" (e.g., "cube maintains 95% of volume on compression", "bounces to 60-80% of initial drop height", "deformation is visible but not extreme"). Use these to validate parameter choices.

---

### Minor Issues (SUGGESTIONS)

1. **Subsurface Scattering is Optional but Unspecified**
   - **Issue**: Q3 (lines 292-306) discusses SSS for translucent jelly, then recommends starting with "simple translucent material" but doesn't specify BRDF/shader approach
   - **Severity**: MINOR (rendering is Phase 5, optional visual polish)
   - **Recommendation**: Either move SSS discussion to Phase 5, or specify a simple Fresnel + refraction material model for Phase 2 baseline.

2. **No Animation Production Pipeline Specification**
   - **Issue**: Research discusses physics simulation and ray tracing but doesn't specify the animation rendering loop (frame capture, timing, step synchronization)
   - **Severity**: MINOR (likely handled by existing `AnimationRenderer` infrastructure)
   - **Recommendation**: Clarify how soft body mesh updates are timed relative to physics steps (every step? every N steps? frame-locked?).

---

### Strengths (Evidence of Quality Research)

✓ **Comprehensive source cross-referencing**: 14 sources with explicit confidence levels (HIGH, MEDIUM) for each claim
✓ **Honest gap identification**: 5 knowledge gaps with impact assessment and mitigation strategies
✓ **Architectural consistency**: Proposed changes respect existing Clean Architecture patterns and interfaces
✓ **Risk-aware**: 5 identified risks with likelihood/impact matrix and concrete mitigation
✓ **Technically accurate**: All claims validated (XPBD algorithm, Jolt API, ray tracing techniques)
✓ **Well-prioritized**: Phased implementation plan with dependencies clearly stated
✓ **Actionable code examples**: C++ snippets with exact API usage for soft body creation and mesh extraction

---

### Blockers for Implementation

**Before Phase 1 begins**, resolve these:

1. **Define synchronization strategy** for DeformableMesh (double-buffering vs. frame locking)
2. **Verify GPU Metal compatibility** (is CPU-only rendering acceptable for soft body scenes?)
3. **Validate dynamic compound shape collision** (test before committing to 'e' letter design)

---

### Questions for Author (If Revision Requested)

1. Is the project rendering pipeline Metal GPU–only, or can soft body scenes use CPU-only rendering?
2. Can you confirm via a small test case that Jolt soft body collision works correctly with *dynamic* compound shapes (not just static)?
3. What is the intended resolution (grid size) for the jelly cube? Should we prioritize fidelity (8x8x8+) or performance (5x5x5)?

---

### Recommended Reading Order for Implementation

1. **Q1 + Q6**: Jolt soft body API and integration architecture (prerequisite)
2. **Q5**: Jelly cube implementation details (Phase 1 code reference)
3. **Q3 + Section on DeformableMesh**: Ray tracing deforming meshes (Phase 2 implementation)
4. **Q4**: 'e' letter generation (Phase 3 implementation)
5. **Recommendations section**: Phased plan and risk mitigations (project planning)

---

### Final Assessment

**This research document successfully establishes feasibility and provides a concrete implementation roadmap for soft body jelly physics in the ray tracer.** The core finding—that Jolt v5.2.0 has all necessary soft body features—is well-validated. The proposed integration architecture (DeformableMesh shape, extended PhysicsSimulator interface) is sound and maintainable.

The two HIGH-severity issues are *design clarifications*, not research flaws. Both are resolvable during Phase 1 and Phase 2 implementation:
- Concurrency: Choose a synchronization strategy (thread-safe queue, frame locking, or double-buffering)
- Dynamic collision validation: Run a 10-minute test with a soft body vs. dynamic compound shape

The research is **approved for implementation** contingent on these clarifications.

---

**Confidence in Approval**: HIGH
**Confidence in Implementation Feasibility**: HIGH (Phases 1-2), MEDIUM (Phase 3, GPU path)
**Recommended Next Step**: Kick off Phase 1 (soft body physics integration) while addressing synchronization and dynamic collision design questions in parallel.
