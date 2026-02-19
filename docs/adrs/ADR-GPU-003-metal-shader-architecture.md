# ADR-GPU-003: Metal Shader Architecture (Single Kernel)

## Status

Accepted

## Context

The Metal compute shader must implement the full ray tracing pipeline: ray generation, scene intersection, material evaluation, shadow rays, multi-bounce iteration, SPP accumulation, and gamma correction. This can be organized as a single monolithic kernel or split across multiple specialized kernels.

The pipeline stages are:
1. Camera ray generation (per pixel)
2. BVH traversal and shape intersection (per ray)
3. Material scatter/reflect/refract evaluation (per hit)
4. Shadow ray casting toward lights (per hit per light)
5. Iterative bounce loop (up to max_depth per pixel per sample)
6. SPP accumulation and averaging (per pixel)
7. Gamma correction and NaN clamping (per pixel)

## Decision

**Single compute kernel** (`ray_trace_kernel`) that performs all pipeline stages in one dispatch. Each GPU thread processes one pixel, running the full SPP loop, multi-bounce iteration, and gamma correction before writing the final color to the output buffer.

**Kernel structure** (pseudocode):
```
kernel ray_trace_kernel(buffers, gid):
    if gid out of bounds: return

    accumulated_color = (0, 0, 0)
    rng_state = init_pcg(gid, frame_seed)

    for sample in 0..spp:
        // Stage 1: ray generation with jitter
        ray = generate_camera_ray(camera, gid.x, gid.y, rng)

        // Stages 2-5: iterative multi-bounce
        color = (0, 0, 0)
        throughput = (1, 1, 1)

        for bounce in 0..max_depth:
            hit = traverse_bvh_and_intersect(ray, shapes, bvh)
            if no hit:
                color += throughput * sky_gradient(ray)
                break

            // Stage 3: material evaluation
            color += throughput * material_emitted(hit)
            // Stage 4: shadow rays
            for light in lights:
                color += throughput * direct_lighting(hit, light, shapes, bvh)
            // Scatter
            throughput *= attenuation
            ray = scattered_ray

        // Stage 6: accumulate
        accumulated_color += color

    // Stage 6-7: average, gamma, clamp
    final = sqrt(clamp(accumulated_color / spp, 0, 1))
    output[gid.y * width + gid.x] = float4(final, 1.0)
```

**Threadgroup size**: 16x16 (256 threads). Grid size: `ceil(width/16) x ceil(height/16)`.

**GPU random number generator**: PCG (Permuted Congruential Generator) with per-pixel, per-sample state. Seed: `pixel_index * max_spp + sample_index + frame_seed`.

## Alternatives Considered

### Alternative 1: Multi-kernel pipeline (generate -> trace -> shade -> accumulate)
Split the pipeline into 4+ kernels, each processing one stage. Intermediate results stored in device buffers between dispatches.

**Rejected because**:
- Intermediate buffers for ray state (origin, direction, throughput, hit info) at per-pixel-per-sample-per-bounce granularity would consume enormous memory. For 4K at 48 SPP, max_depth 10: 8M pixels * 48 samples * ~64 bytes/ray = ~24 GB of intermediate state. This exceeds most GPU memory.
- Multiple dispatch synchronization adds overhead (command buffer encoding, GPU idle time between dispatches).
- The iterative bounce loop has data-dependent termination (miss exits early). Multi-kernel would require tracking which rays are still active (stream compaction), adding complexity.
- The single-kernel approach is the standard for offline GPU path tracers (e.g., Blender Cycles GPU backend, PBRT-v4 GPU).

### Alternative 2: Two kernels (trace + accumulate)
One kernel performs all ray tracing (SPP loop + bounces) writing per-sample results to a buffer. A second kernel averages samples and applies gamma.

**Rejected because**: The per-sample intermediate buffer (8M pixels * 48 samples * 16 bytes/color = ~6 GB for 4K) is unnecessarily large. Accumulation in registers (running sum) within the single kernel is free. The second kernel saves no computation -- it just adds a buffer allocation and a dispatch.

### Alternative 3: Wavefront path tracing (regeneration-based)
Process rays in waves. Each dispatch handles one bounce for all active rays. Terminated rays are regenerated as new camera rays. Maximizes GPU occupancy by always having full waves.

**Rejected because**: Wavefront tracing is optimal for real-time renderers with low SPP (1-4) where occupancy is critical. For offline rendering at 48 SPP, the megakernel approach (single kernel, full path per thread) has sufficient occupancy. Wavefront adds significant architectural complexity (ray state buffers, regeneration queues, compaction kernels) without proportional benefit for offline workloads. This is a well-documented trade-off in GPU ray tracing literature. The megakernel approach is simpler to implement, debug, and maintain.

## Consequences

- **Positive**: Minimal GPU memory usage (only input buffers + output buffer, no intermediate state). Single dispatch per frame (simple command buffer encoding). Each thread is self-contained -- no synchronization between threads. Standard pattern, well-documented in GPU ray tracing implementations.
- **Negative**: Thread divergence when nearby pixels hit different materials or have different bounce counts. Threads that terminate early (sky miss on bounce 1) sit idle while other threads in the same wavefront continue bouncing. On Apple Silicon GPUs with SIMD-group width 32, divergence penalty is bounded: in the worst case, 31 of 32 threads idle while 1 thread bounces. At 48 SPP, this averages out across samples.
- **Trade-off**: For extremely complex scenes with high max_depth, thread divergence could become a bottleneck. A future wavefront implementation could address this, but the megakernel is the correct starting point for simplicity and correctness.
