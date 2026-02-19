# ADR-GPU-005: Build System Metal Integration Approach

## Status

Accepted

## Context

The nwave-raytracer uses CMake (3.16+) with FetchContent for external dependencies (GoogleTest, yaml-cpp, Jolt Physics). Metal GPU support requires:

1. Detecting Metal availability at configure time
2. Compiling `.metal` shader files to `.metallib` during build
3. Compiling Objective-C++ (`.mm`) source files
4. Linking Metal.framework and Foundation.framework
5. Ensuring the build succeeds on Linux/Windows where Metal is not available
6. Placing the compiled `.metallib` alongside the binary for runtime loading

## Decision

**Opt-in CMake option**: `NWAVE_ENABLE_GPU` (default: OFF). When ON on macOS, CMake detects Metal.framework, Foundation.framework, and the `xcrun` tool chain. If found, it sets `NWAVE_HAS_METAL=TRUE` and defines `NWAVE_HAS_METAL=1` as a compile definition.

**Shader compilation**: CMake `add_custom_command` invokes `xcrun -sdk macosx metal` to compile each `.metal` file to `.air` (Apple Intermediate Representation), then `xcrun -sdk macosx metallib` to link `.air` files into `nwave_shaders.metallib`. The `.metallib` is copied alongside the `nwave` binary via a POST_BUILD step.

**Conditional source inclusion**: `.mm` files are added to the `nwave_infrastructure` library only when `NWAVE_HAS_METAL` is TRUE. The `GPU_COMMON_SOURCES` (SceneFlattener, BVHFlattener -- pure C++) are always compiled on all platforms.

**Framework linking**: `Metal.framework` and `Foundation.framework` are linked to `nwave_infrastructure` only when `NWAVE_HAS_METAL` is TRUE. The `-framework Metal` CMake syntax is used.

**Non-macOS behavior**: `NWAVE_ENABLE_GPU=ON` on Linux/Windows produces a CMake warning but does not error. The build proceeds without GPU support. The `NWAVE_HAS_METAL` compile definition is not set, so `#ifdef NWAVE_HAS_METAL` guards in C++ code exclude Metal codepaths.

## Alternatives Considered

### Alternative 1: Runtime shader compilation (load .metal source at runtime)
Instead of compiling shaders at build time, embed the `.metal` source text in the binary and compile it at runtime using `MTLDevice.newLibraryWithSource:`.

**Rejected because**: Runtime compilation adds 200-500ms to the first GPU render. Shader syntax errors surface at runtime (user's machine), not at build time (developer's machine). Build-time compilation catches shader bugs during development. Additionally, `newLibraryWithSource:` requires the Metal compiler to be present at runtime, which is not guaranteed on all macOS systems.

### Alternative 2: Embed .metallib as C array in binary
Compile shaders at build time, then `xxd` the `.metallib` into a C byte array that is compiled into the binary. Load via `newLibraryWithData:` at runtime.

**Rejected because**: Adds build complexity (xxd step, generated source file). The `.metallib` file is small (~50-100 KB). Placing it alongside the binary (in the same directory) is simpler and standard practice for Metal applications. Embedding is useful for iOS apps (where a single `.app` bundle is expected) but unnecessary for a CLI tool.

### Alternative 3: Always compile GPU sources (guarded by #ifdef)
Compile all GPU C++ sources (including Metal-specific code) on all platforms, with `#ifdef NWAVE_HAS_METAL` guards around Metal API calls.

**Rejected because**: `.mm` files require an Objective-C++ compiler, which is not available on Linux/Windows. Even if we renamed them to `.cpp` and used `#ifdef` extensively, the Metal API types (`id<MTLDevice>`, etc.) are not declared on non-Apple platforms, causing compilation errors in even the type declarations. Conditional source inclusion (not compiling `.mm` files on non-macOS) is the only clean approach.

## Consequences

- **Positive**: Zero impact on non-GPU builds. The default `NWAVE_ENABLE_GPU=OFF` means existing developers see no change in CMake output, build time, or binary. Shader compilation errors are caught at build time with familiar clang-style error messages. The build succeeds on all platforms.
- **Negative**: Developers must opt in with `-DNWAVE_ENABLE_GPU=ON`. This is intentional (explicit is better than implicit for a new, platform-specific feature). Xcode Command Line Tools must be installed for the `xcrun` tool chain (standard on developer macOS systems).
- **CI consideration**: macOS CI runners can build with `NWAVE_ENABLE_GPU=ON` to test shader compilation. Linux CI runs with GPU=OFF to verify the CPU-only build. Both are needed for complete coverage.
