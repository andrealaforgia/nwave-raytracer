#include "infrastructure/metal/metal_render_backend.h"
#include "infrastructure/metal/metal_device.h"
#include "infrastructure/metal/metal_buffer_manager.h"

#include <iostream>
#include <memory>

namespace nwave {

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

    impl_->buffer_manager = std::make_unique<MetalBufferManager>(*impl_->device);
    return true;
}

std::vector<Color3> MetalRenderBackend::render(const Camera& camera,
                                               const Scene& /*scene*/,
                                               const RenderSettings& /*settings*/) {
    return render_gradient(camera.image_width(), camera.image_height());
}

std::vector<Color3> MetalRenderBackend::render_gradient(int width, int height) {
    if (!impl_ || !impl_->buffer_manager) {
        return {};
    }
    return impl_->buffer_manager->dispatch_gradient(width, height);
}

} // namespace nwave
