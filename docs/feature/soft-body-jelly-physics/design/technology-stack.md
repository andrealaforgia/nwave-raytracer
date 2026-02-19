# Technology Stack: Soft Body Jelly Physics

**Date**: 2026-02-19

---

## Existing Dependencies (No Changes)

| Technology | Version | Purpose | License |
|---|---|---|---|
| Jolt Physics | v5.2.0 | Rigid body physics (already integrated) | MIT |
| yaml-cpp | (current) | YAML scene parsing | MIT |
| CMake | 3.x | Build system | BSD-3-Clause |

---

## Existing Dependency Extensions

### Jolt Physics v5.2.0 -- Soft Body API

**What changes**: Use Jolt's existing soft body module (already compiled into the project; headers available at `build/_deps/joltphysics-src/Jolt/Physics/SoftBody/`).

**Key Jolt soft body headers to consume**:
- `SoftBodyCreationSettings.h` -- pressure, restitution, damping, solver iterations
- `SoftBodySharedSettings.h` -- vertex grid, edge/volume constraints, surface faces
- `SoftBodyMotionProperties.h` -- per-frame deformed vertex extraction
- `SoftBodyVertex.h` -- vertex position/velocity data

**No version change required**. Jolt v5.2.0 has full XPBD soft body support including volume constraints, pressure, and rigid-soft collision response.

**Risk**: LOW. The soft body module is part of the already-integrated Jolt build.

---

## New Dependencies

### 1. ttf2mesh -- Font Glyph to 2D Triangulated Mesh

| Attribute | Value |
|---|---|
| **Repository** | https://github.com/nickg/ttf2mesh (originally fetisov/ttf2mesh) |
| **License** | MIT |
| **Language** | C99 (2 files: `ttf2mesh.c`, `ttf2mesh.h`) |
| **External deps** | None |
| **Purpose** | Convert TrueType font glyph outlines to 2D triangulated mesh. Used for generating the front/back faces of extruded 3D letter shapes. |
| **Integration** | CMake FetchContent or direct file inclusion (only 2 source files) |
| **Risk** | MEDIUM -- glyph counter (hole) handling for 'e' needs early validation |

**Why ttf2mesh over alternatives**:
- **vs FreeType + earcut.hpp**: FreeType is a large dependency (font rasterization engine) when we only need outline extraction and triangulation. ttf2mesh is purpose-built for mesh generation from glyphs.
- **vs Font23D**: Less maintained, heavier dependency.
- **Fallback**: If ttf2mesh fails on complex glyphs with counters, FreeType (MIT) + earcut.hpp (ISC) is the fallback pair.

**ADR**: See ADR-001 below.

### 2. V-HACD -- Volumetric Hierarchical Approximate Convex Decomposition

| Attribute | Value |
|---|---|
| **Repository** | https://github.com/kmammou/v-hacd |
| **License** | BSD-3-Clause |
| **Language** | C++ (header-only option available) |
| **External deps** | None |
| **Purpose** | Decompose concave letter mesh into approximate convex hulls for Jolt dynamic physics body |
| **Integration** | CMake FetchContent |
| **Risk** | LOW -- mature library, widely used in game physics pipelines |

**Why V-HACD over alternatives**:
- **vs CoACD**: CoACD produces tighter fits but is newer, less battle-tested, and has heavier build requirements.
- **vs manual compound shapes**: Manual approximation is per-character work, poor fidelity, not maintainable.
- **vs Jolt MeshShape**: MeshShape is static-only in Jolt; dynamic bodies need convex shapes.

**ADR**: See ADR-002 below.

### 3. Default Bundled Font

| Attribute | Value |
|---|---|
| **Font** | Open Sans, Roboto, or Liberation Sans (any open-source sans-serif) |
| **License** | Apache 2.0 (Open Sans/Roboto) or SIL OFL 1.1 (Liberation Sans) |
| **Purpose** | Provide a default font for `font: "default"` in YAML without requiring user to supply a .ttf file |
| **Integration** | Bundle .ttf file in project assets directory; embed path at compile time or copy to build output |
| **Risk** | LOW |

---

## CMake Integration

All new dependencies use FetchContent for reproducible builds:

```cmake
# ttf2mesh (2-file C99 library)
FetchContent_Declare(
    ttf2mesh
    GIT_REPOSITORY https://github.com/nickg/ttf2mesh.git
    GIT_TAG <pinned-commit-hash>
)

# V-HACD (convex decomposition)
FetchContent_Declare(
    vhacd
    GIT_REPOSITORY https://github.com/kmammou/v-hacd.git
    GIT_TAG <pinned-commit-hash>
)
```

Both libraries are small and compile quickly. No system-level package installation required.

---

## ADR-001: Font Parsing Library Selection

**Status**: Proposed

**Context**: The system needs to convert TrueType font glyphs into triangulated 2D meshes for 3D extrusion. The 'e' glyph has an interior contour (counter/hole) that must be handled correctly.

**Decision**: Use ttf2mesh as primary library, with FreeType + earcut.hpp as fallback.

**Alternatives Considered**:
- **FreeType + earcut.hpp**: FreeType (MIT) extracts Bezier outlines; earcut.hpp (ISC) triangulates with holes. Proven, but FreeType is a large dependency (~500 source files) for a narrow use case. Earcut requires separate integration.
- **Font23D**: Less maintained, sparse documentation. Higher integration risk.

**Consequences**:
- Positive: Minimal dependency footprint (2 source files), no transitive dependencies, MIT license
- Negative: Less community usage than FreeType; counter handling quality unverified for all glyphs
- Mitigation: Validate 'e' glyph with counter early. If ttf2mesh fails, fall back to FreeType path.

## ADR-002: Convex Decomposition Library Selection

**Status**: Proposed

**Context**: Dynamic rigid bodies in Jolt require convex collision shapes. The letter mesh is concave with holes. Decomposition into approximate convex hulls is needed.

**Decision**: Use V-HACD for convex decomposition.

**Alternatives Considered**:
- **CoACD**: Produces tighter approximations via Approximate Convex Decomposition. Newer library (2022), fewer production deployments, requires Eigen dependency.
- **Manual compound shapes**: Approximate each letter with boxes/cylinders. Not scalable, poor fidelity, per-character effort.

**Consequences**:
- Positive: Mature, widely used, BSD-3-Clause license, no heavy dependencies
- Negative: Approximations may have small gaps; higher hull counts for complex shapes
- Mitigation: Tune V-HACD resolution parameter per letter complexity. 8-15 hulls typical for letters.

---

## Technology Compatibility Matrix

| Component | C++17 | macOS (Darwin) | CMake FetchContent | CPU Rendering | GPU Metal |
|---|---|---|---|---|---|
| Jolt Physics v5.2.0 (soft body) | Yes | Yes | Already integrated | N/A | N/A |
| ttf2mesh | Yes (C99) | Yes | Yes | N/A | N/A |
| V-HACD | Yes | Yes | Yes | N/A | N/A |
| DeformableMesh (new) | Yes | Yes | N/A | Yes | No (v1) |

All dependencies are compatible with the project's C++17 standard and macOS build target.
