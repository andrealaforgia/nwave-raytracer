# nwave-raytracer -- Technology Stack

**Document ID**: TECH-RAYTRACER-001
**Date**: 2026-02-16
**Status**: Draft

---

## 1. Language: C++17

### 1.1 Decision

C++17 is the language standard for all production and test code.

### 1.2 Rationale

- **Performance**: C++ is the standard choice for CPU ray tracers. Direct memory control and zero-cost abstractions enable competitive performance.
- **Ecosystem**: All major ray tracing references (PBRT, "Ray Tracing in One Weekend") use C++.
- **Portability**: C++17 is supported by all major compilers on Linux, macOS, and Windows.
- **User selected**: The user explicitly specified C++17.

### 1.3 C++17 Features Used

| Feature | Usage | Why |
|---|---|---|
| **`std::optional`** | Return types for operations that may fail (e.g., intersection results that miss) | Explicit "no value" semantics without sentinel values or output parameters |
| **`std::variant`** | Not used -- polymorphism via virtual dispatch is the chosen pattern | User selected OO hierarchy over variant-based design |
| **`std::filesystem`** | Scene file path validation, output directory existence checks in CLI | Portable path handling across Linux/macOS/Windows |
| **`std::string_view`** | Lightweight string references in parsing and error messages | Avoids unnecessary string copies in hot paths |
| **Structured bindings** | `auto [hit, record] = shape.intersect(ray)` style returns | Cleaner multi-value returns |
| **`if constexpr`** | Compile-time branching in template utility code (e.g., math helpers) | Zero-overhead conditional compilation |
| **Inline variables** | Constants in header files (`inline constexpr double pi = 3.14159...`) | Avoids ODR violations for header-only math constants |
| **Nested namespaces** | `namespace nwave::core`, `namespace nwave::domain::shapes` | Cleaner namespace declarations |
| **Class template argument deduction (CTAD)** | `std::vector v{1.0, 2.0, 3.0}` | Reduces template verbosity |
| **`[[nodiscard]]`** | On functions returning intersection results and error codes | Prevents silently ignoring important return values |

### 1.4 Alternatives Considered

| Alternative | Evaluation | Rejection Reason |
|---|---|---|
| **C++20** | Concepts, ranges, and modules would improve code clarity | Not universally supported across all target compilers (especially older GCC/MSVC on some platforms). C++17 is the safer portable choice. |
| **C++14** | Widely supported | Lacks `std::optional`, `std::filesystem`, structured bindings, inline variables. These are quality-of-life features worth requiring C++17. |
| **Rust** | Strong safety guarantees, good performance | User explicitly selected C++. Rust's borrow checker adds friction for the graph-like scene data structures common in ray tracers. |

---

## 2. Build System: CMake

### 2.1 Decision

CMake 3.16+ is the build system.

### 2.2 Rationale

- **Industry standard**: CMake is the de-facto standard for cross-platform C++ projects.
- **Generator flexibility**: Supports Makefiles (Linux), Ninja (all platforms), Xcode (macOS), MSVC (Windows).
- **FetchContent**: Built-in support for downloading and building third-party dependencies at configure time.
- **Target-based**: Modern CMake (3.16+) uses target-based dependency management, which maps cleanly to the Clean Architecture rings.

### 2.3 CMake Structure

```
CMakeLists.txt (root)
  - Set project name, C++17 standard, compiler flags
  - FetchContent for yaml-cpp and GoogleTest
  - Add subdirectories: src/, tests/

src/CMakeLists.txt
  - Library targets per ring:
    - nwave_core (Ring 1: core/*.cpp)
    - nwave_domain (Ring 2: domain/**/*.cpp, links nwave_core)
    - nwave_application (Ring 3: application/*.cpp, links nwave_domain)
    - nwave_infrastructure (Ring 4: infrastructure/*.cpp, links nwave_application, yaml-cpp)
  - Executable target: nwave (links nwave_infrastructure, main.cpp)

tests/CMakeLists.txt
  - Test executable: nwave_tests (links all library targets + GoogleTest)
  - Organized by ring: tests/core/, tests/domain/, tests/application/, tests/infrastructure/
```

### 2.4 Dependency Direction Enforcement

CMake `target_link_libraries` enforces ring boundaries at build time:

- `nwave_core` links nothing (only C++ standard library)
- `nwave_domain` links only `nwave_core`
- `nwave_application` links only `nwave_domain` (transitively gets `nwave_core`)
- `nwave_infrastructure` links `nwave_application`, `yaml-cpp`, and stb_image_write header

If a developer accidentally `#include`s an infrastructure header from domain code, the build will fail because the domain target does not link the infrastructure target.

### 2.5 Alternatives Considered

| Alternative | Evaluation | Rejection Reason |
|---|---|---|
| **Meson** | Fast, clean syntax, good C++ support | Smaller ecosystem, fewer tutorials, less IDE integration than CMake. CMake is the safer choice for a project targeting Linux/macOS/Windows. |
| **Bazel** | Excellent for large-scale builds with hermetic caching | Over-engineered for a single-executable project. Complex setup for a small codebase. |
| **Plain Makefiles** | Simple, no build tool dependency | Not portable to Windows without MSYS/Cygwin. No built-in dependency management. |

---

## 3. External Libraries

### 3.1 yaml-cpp (YAML Parsing)

| Attribute | Value |
|---|---|
| **Purpose** | Parse YAML scene files into C++ data structures |
| **Version** | 0.8.x (latest stable) |
| **License** | MIT |
| **Integration** | CMake FetchContent |
| **Ring** | Infrastructure (Ring 4 only) |

**Rationale**: yaml-cpp is the most widely used C++ YAML parser. MIT license, well-maintained, no external dependencies of its own.

**Alternatives Considered**:

| Alternative | Evaluation | Rejection Reason |
|---|---|---|
| **rapidyaml (ryml)** | Faster parsing, header-only option | Less mature API, fewer examples. yaml-cpp's maturity and documentation outweigh ryml's speed advantage for this use case (scene files are small, parsed once). |
| **nlohmann/json** | If using JSON instead of YAML | JSON is more verbose and does not support comments. YAML is the user-selected format. |
| **Custom parser** | Full control | Significant development effort for a solved problem. Not core to the ray tracing domain. |

### 3.2 stb_image_write (PNG Output)

| Attribute | Value |
|---|---|
| **Purpose** | Write PNG image files from pixel buffer |
| **Version** | Latest (single header, no versioning) |
| **License** | MIT / Public Domain (dual-licensed) |
| **Integration** | Vendored as single header in `third_party/stb/stb_image_write.h` |
| **Ring** | Infrastructure (Ring 4 only) |

**Rationale**: stb_image_write is a single-header library requiring zero build configuration. It is the standard choice for lightweight PNG writing in C/C++ applications. No build complexity, no external dependencies.

**Alternatives Considered**:

| Alternative | Evaluation | Rejection Reason |
|---|---|---|
| **libpng** | The reference PNG implementation | Significantly more complex to build and link. Requires zlib as a dependency. Over-engineered for "write one PNG file" use case. |
| **lodepng** | Single-file PNG encoder/decoder | Viable alternative to stb. stb_image_write is more widely used and supports additional formats (BMP, TGA) if needed later. |
| **No PNG support (PPM only)** | Simplest possible approach | PPM files are large and not universally viewable. PNG is needed for practical use (US-901 specifies PNG as Phase 2 output). |

### 3.3 GoogleTest (Testing Framework)

| Attribute | Value |
|---|---|
| **Purpose** | Unit and integration testing framework |
| **Version** | 1.14.x (latest stable) |
| **License** | BSD 3-Clause |
| **Integration** | CMake FetchContent (test binary only, not linked into production executable) |
| **Ring** | Test infrastructure only (not part of production rings) |

**Rationale**: GoogleTest is the de-facto standard C++ testing framework. Rich assertion library, test discovery, parameterized tests, and death tests. BSD license is permissive.

**Alternatives Considered**:

| Alternative | Evaluation | Rejection Reason |
|---|---|---|
| **Catch2** | Header-only (v2) or single-library (v3), BDD-style macros | Viable alternative. GoogleTest chosen for wider adoption, better IDE integration, and team familiarity assumptions. |
| **doctest** | Faster compilation than Catch2, similar API | Less mature ecosystem and fewer community resources. |
| **No framework (assert-based)** | Zero dependencies | No test discovery, no structured reporting, no parameterized tests. Unacceptable for a project with 20 user stories and hundreds of acceptance criteria. |

---

## 4. Compiler and Platform Requirements

| Platform | Compiler | Minimum Version | Notes |
|---|---|---|---|
| Linux | GCC | 7.0+ | Full C++17 support from GCC 7. `std::filesystem` requires GCC 8+ or `-lstdc++fs`. |
| Linux | Clang | 5.0+ | Full C++17 support. |
| macOS | Apple Clang | 10.0+ (Xcode 10) | Ships with macOS Mojave and later. |
| Windows | MSVC | 19.14+ (VS 2017 15.7) | Full C++17 support. |

### 4.1 Compiler Flags

```cmake
target_compile_options(nwave PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall -Wextra -Wpedantic -Werror
        -O2                    # Optimization for release
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /WX
        /O2                    # Optimization for release
    >
)
```

Debug builds use `-O0 -g` (GCC/Clang) or `/Od /Zi` (MSVC).

---

## 5. No-Dependency Guarantee for Core Rings

Rings 1-3 (Core, Domain, Application) have zero external library dependencies. They depend only on the C++17 standard library. This guarantees:

- Core rendering logic is portable to any C++17 environment
- Unit tests for Rings 1-3 require only GoogleTest (test infrastructure), not yaml-cpp or stb
- The renderer can be embedded in other applications without carrying I/O library baggage

Only Ring 4 (Infrastructure) introduces external library dependencies (yaml-cpp, stb_image_write).

---

## 6. Dependency Summary

```
Production Dependencies:
  Ring 1 (Core):           C++17 standard library only
  Ring 2 (Domain):         C++17 standard library only
  Ring 3 (Application):    C++17 standard library only
  Ring 4 (Infrastructure): yaml-cpp (MIT), stb_image_write (MIT/PD)

Test Dependencies:
  All rings:               GoogleTest (BSD 3-Clause)

Build Dependencies:
  CMake 3.16+
  C++17-compliant compiler
```

All external dependencies are open source with permissive licenses (MIT, Public Domain, BSD). No proprietary or copyleft libraries are used.
