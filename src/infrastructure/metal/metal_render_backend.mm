#include "infrastructure/metal/metal_render_backend.h"
#include "infrastructure/metal/metal_device.h"
#include "infrastructure/metal/metal_buffer_manager.h"
#include "infrastructure/gpu/scene_flattener.h"
#include "infrastructure/gpu/bvh_flattener.h"
#include "core/gpu_types.h"

#include <iostream>
#include <memory>

namespace nwave {

GPUCamera pack_gpu_camera(const Camera& camera, const RenderSettings& settings) {
    GPUCamera gpu{};
    const auto& lf = camera.lookfrom();
    gpu.lookfrom[0] = static_cast<float>(lf.x());
    gpu.lookfrom[1] = static_cast<float>(lf.y());
    gpu.lookfrom[2] = static_cast<float>(lf.z());

    const auto& p00 = camera.pixel00_loc();
    gpu.pixel00_loc[0] = static_cast<float>(p00.x());
    gpu.pixel00_loc[1] = static_cast<float>(p00.y());
    gpu.pixel00_loc[2] = static_cast<float>(p00.z());

    const auto& du = camera.pixel_delta_u();
    gpu.pixel_delta_u[0] = static_cast<float>(du.x());
    gpu.pixel_delta_u[1] = static_cast<float>(du.y());
    gpu.pixel_delta_u[2] = static_cast<float>(du.z());

    const auto& dv = camera.pixel_delta_v();
    gpu.pixel_delta_v[0] = static_cast<float>(dv.x());
    gpu.pixel_delta_v[1] = static_cast<float>(dv.y());
    gpu.pixel_delta_v[2] = static_cast<float>(dv.z());

    gpu.background_top[0] = static_cast<float>(settings.background_top.r());
    gpu.background_top[1] = static_cast<float>(settings.background_top.g());
    gpu.background_top[2] = static_cast<float>(settings.background_top.b());

    gpu.background_bottom[0] = static_cast<float>(settings.background_bottom.r());
    gpu.background_bottom[1] = static_cast<float>(settings.background_bottom.g());
    gpu.background_bottom[2] = static_cast<float>(settings.background_bottom.b());

    gpu.image_width = static_cast<uint32_t>(camera.image_width());
    gpu.image_height = static_cast<uint32_t>(camera.image_height());
    gpu.samples_per_pixel = static_cast<uint32_t>(settings.samples_per_pixel);
    gpu.max_depth = static_cast<uint32_t>(settings.max_depth);
    gpu.ambient_factor = settings.ambient_factor;
    gpu.batch_index = 0;
    gpu.num_batches = 1;
    gpu._pad6 = 0.0f;

    return gpu;
}

struct MetalRenderBackend::Impl {
    std::unique_ptr<MetalDevice> device;
    std::unique_ptr<MetalBufferManager> buffer_manager;
    SceneFlattener flattener;  // persistent across frames to cache materials/textures
};

MetalRenderBackend::MetalRenderBackend() = default;
MetalRenderBackend::~MetalRenderBackend() = default;

bool MetalRenderBackend::initialise(const std::string& metallib_path) {
    impl_ = std::make_unique<Impl>();
    impl_->device = std::make_unique<MetalDevice>();

    if (!impl_->device->is_available()) {
        std::cerr << "Error: Metal GPU device not available\n";
        return false;
    }

    if (!impl_->device->load_library(metallib_path)) {
        std::cerr << "Error: failed to load Metal shader library: "
                  << metallib_path << "\n";
        return false;
    }

    if (!impl_->device->create_pipeline("gradient_kernel")) {
        std::cerr << "Error: failed to create gradient_kernel pipeline\n";
        return false;
    }

    if (!impl_->device->create_pipeline("ray_trace_kernel")) {
        std::cerr << "Error: failed to create ray_trace_kernel pipeline\n";
        return false;
    }

    impl_->buffer_manager = std::make_unique<MetalBufferManager>(*impl_->device);
    return true;
}

std::vector<Color3> MetalRenderBackend::render(const Camera& camera,
                                               const Scene& scene,
                                               const RenderSettings& settings) {
    if (!impl_ || !impl_->buffer_manager) {
        return {};
    }
    FlatScene flat = impl_->flattener.flatten(scene);
    GPUCamera gpu_camera = pack_gpu_camera(camera, settings);

    // Build BVH and reorder shapes for GPU traversal
    BVHFlattener bvh_flattener;
    auto bvh_result = bvh_flattener.build_and_flatten(flat.shapes);

    if (!bvh_result.nodes.empty()) {
        // Reorder shapes according to BVH shape_order
        std::vector<GPUShape> reordered(flat.shapes.size());
        for (size_t i = 0; i < bvh_result.shape_order.size(); ++i) {
            reordered[i] = flat.shapes[bvh_result.shape_order[i]];
        }
        flat.shapes = std::move(reordered);
    }

    // DIAGNOSTIC: dump shape/material/BVH statistics (first frame only)
    static bool diag_done = false;
    if (!diag_done) {
        diag_done = true;
        std::cerr << "[DIAG] shapes=" << flat.shapes.size()
                  << " materials=" << flat.materials.size()
                  << " lights=" << flat.lights.size()
                  << " bvh_nodes=" << bvh_result.nodes.size() << "\n";
        // Count shape types and material usage
        int type_counts[5] = {};
        int mat_usage[20] = {};
        for (size_t i = 0; i < flat.shapes.size(); ++i) {
            uint32_t st = flat.shapes[i].shape_type;
            if (st < 5) type_counts[st]++;
            uint32_t mi = flat.shapes[i].material_index;
            if (mi < 20) mat_usage[mi]++;
        }
        std::cerr << "[DIAG] shape types: sphere=" << type_counts[0]
                  << " plane=" << type_counts[1]
                  << " box=" << type_counts[2]
                  << " cylinder=" << type_counts[3]
                  << " triangle=" << type_counts[4] << "\n";
        for (size_t m = 0; m < flat.materials.size() && m < 20; ++m) {
            std::cerr << "[DIAG] material[" << m << "] type=" << flat.materials[m].material_type
                      << " albedo=(" << flat.materials[m].albedo[0] << ","
                      << flat.materials[m].albedo[1] << "," << flat.materials[m].albedo[2]
                      << ") param1=" << flat.materials[m].param1
                      << " tint=(" << flat.materials[m].tint[0] << ","
                      << flat.materials[m].tint[1] << "," << flat.materials[m].tint[2]
                      << ") usage=" << mat_usage[m] << "\n";
        }
        // Dump root BVH node AABB
        if (!bvh_result.nodes.empty()) {
            const auto& root = bvh_result.nodes[0];
            std::cerr << "[DIAG] BVH root AABB: ("
                      << root.aabb_min[0] << "," << root.aabb_min[1] << "," << root.aabb_min[2]
                      << ") -> (" << root.aabb_max[0] << "," << root.aabb_max[1] << "," << root.aabb_max[2]
                      << ")\n";
        }
        // Check for triangles with large or NaN coords
        int nan_count = 0, large_count = 0;
        for (size_t i = 0; i < flat.shapes.size(); ++i) {
            if (flat.shapes[i].shape_type == 4) { // TRIANGLE
                for (int p = 0; p < 12; ++p) {
                    float v = flat.shapes[i].params[p];
                    if (std::isnan(v)) nan_count++;
                    if (std::abs(v) > 100.0f) large_count++;
                }
            }
        }
        std::cerr << "[DIAG] Triangle NaN params=" << nan_count
                  << " large params(>100)=" << large_count << "\n";
        std::cerr << "[DIAG] texture_data size=" << flat.texture_data.size() << " bytes\n";
        for (size_t m = 0; m < flat.materials.size() && m < 20; ++m) {
            if (flat.materials[m].texture_offset >= 0) {
                std::cerr << "[DIAG] material[" << m << "] has texture: offset="
                          << flat.materials[m].texture_offset
                          << " width=" << flat.materials[m].texture_width
                          << " height=" << flat.materials[m].texture_height << "\n";
            }
        }
    }

    return impl_->buffer_manager->dispatch_ray_trace(gpu_camera, flat, bvh_result.nodes);
}

std::vector<Color3> MetalRenderBackend::render_gradient(int width, int height) {
    if (!impl_ || !impl_->buffer_manager) {
        return {};
    }
    return impl_->buffer_manager->dispatch_gradient(width, height);
}

} // namespace nwave
