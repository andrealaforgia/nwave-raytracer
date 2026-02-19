# ADR-GPU-004: Float Precision Strategy (Double CPU / Float GPU)

## Status

Accepted

## Context

The CPU ray tracer uses `double` (64-bit) precision throughout: `Vec3` stores `double e[3]`, Camera parameters are `double`, intersection math uses `double`. Metal compute shaders do not support `double` precision -- `float` (32-bit) is the only available numeric type for compute operations on Apple GPUs.

This creates a precision boundary: the CPU side works in double; the GPU side works in float. The question is where and how to manage this conversion, and what visual tolerance to accept.

## Decision

**Conversion point**: The `SceneFlattener` performs all `double -> float` narrowing when constructing GPU data structs. The `MetalBufferManager::readback_output` performs `float -> double` widening when converting GPU output back to `Color3`.

**Ring 1 types**: The existing `Vec3`/`Point3`/`Color3` remain `double`. The new `gpu_types.h` structs use `float`. There are no mixed-precision types.

**Camera precision**: Camera parameters (lookfrom, pixel00_loc, pixel_delta_u, pixel_delta_v) are computed on the CPU in `double` precision (by the existing `Camera` class), then narrowed to `float` when packing into `GPUCamera`. The GPU generates rays using these `float` parameters. Ray direction precision loss is bounded: for a 4K image, the worst-case pixel_delta magnitude is ~1e-4 in world units; float mantissa (23 bits, ~7 decimal digits) preserves this to ~1e-11 relative error. No visible artifact.

**Intersection precision**: Sphere quadratic discriminant, plane dot product, box slab test all use `float` on GPU. The epsilon for shadow ray offset (0.001) is representable in float without loss. The main precision concern is catastrophic cancellation in the quadratic formula for grazing-angle sphere hits. The CPU uses `double` to handle this; the GPU uses `float` which may produce slightly different t values for edge cases. This is acceptable: it affects 1-2 pixels per sphere silhouette.

**Tolerance**: GPU and CPU renders are expected to differ by up to +/-5 per RGB channel (out of 255) when averaged over 100+ SPP. At low SPP (1), differences can be larger (+/-10) due to RNG-path sensitivity. This tolerance is documented in the acceptance criteria.

## Alternatives Considered

### Alternative 1: Emulate double precision in MSL
Use `float2` to represent a double (double-float or Dekker arithmetic): `value = hi + lo` where `hi` and `lo` are both float. All operations (add, multiply, sqrt) are reimplemented using error-free transformations.

**Rejected because**: Doubles the register usage and halves throughput (every operation becomes 2-4 float operations). The 50-200x speedup target would be cut to 25-100x. The visual improvement is negligible -- the difference between float and double is sub-pixel for ray tracing. This technique is used in scientific computing where exact numeric results matter; for image rendering, perceptual quality is the metric, and float is sufficient.

### Alternative 2: Use float everywhere (CPU and GPU)
Change the CPU code to use `float` instead of `double` to eliminate the conversion boundary entirely.

**Rejected because**: This modifies the existing CPU renderer, violating the constraint "all 243 existing tests pass without modification." The CPU renderer's `double` precision is part of its correctness contract. Some tests may assert specific numeric values that would change with float precision. Additionally, double precision on CPU is free (modern CPUs have 64-bit FPUs with no throughput penalty), so there is no performance reason to downgrade.

### Alternative 3: Precision mode flag (float GPU vs double GPU)
Allow the user to select float or double GPU precision. On Apple Silicon M3+, Metal supports limited double operations.

**Rejected because**: Apple Silicon M3 supports double only in specific contexts (not general compute), and at drastically reduced throughput (1/32 of float). The performance impact would negate the GPU advantage. Additionally, M1 and M2 chips (the primary target) have no double support at all. Maintaining two GPU code paths (float and double) doubles the shader codebase for negligible benefit.

## Consequences

- **Positive**: Clean separation. CPU code is 100% double, GPU code is 100% float. The conversion happens at two well-defined points (SceneFlattener and readback). No mixed-precision code anywhere. GPU achieves full float throughput.
- **Negative**: Per-pixel color differences between CPU and GPU are expected. Users comparing images at the byte level will see differences. This must be documented in user-facing output (e.g., "GPU uses float precision; minor differences from CPU are expected").
- **Mitigation**: The SPP accumulation (averaging many samples) statistically reduces per-pixel variance. At 48 SPP, the float-vs-double contribution to per-pixel error is dwarfed by Monte Carlo noise. At 100+ SPP, convergence brings GPU and CPU images within +/-5 per channel.
