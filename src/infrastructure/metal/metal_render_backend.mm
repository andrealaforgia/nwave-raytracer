#include "infrastructure/metal/metal_render_backend.h"
#include "infrastructure/metal/metal_device.h"
#include "infrastructure/metal/metal_buffer_manager.h"
#include "infrastructure/gpu/scene_flattener.h"
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

    return gpu;
}

struct MetalRenderBackend::Impl {
    std::unique_ptr<MetalDevice> device;
    std::unique_ptr<MetalBufferManager> buffer_manager;
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
    SceneFlattener flattener;
    FlatScene flat = flattener.flatten(scene);
    GPUCamera gpu_camera = pack_gpu_camera(camera, settings);
    return impl_->buffer_manager->dispatch_ray_trace(gpu_camera, flat);
}

std::vector<Color3> MetalRenderBackend::render_gradient(int width, int height) {
    if (!impl_ || !impl_->buffer_manager) {
        return {};
    }
    return impl_->buffer_manager->dispatch_gradient(width, height);
}

} // namespace nwave
