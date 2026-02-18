#include "infrastructure/metal/metal_device.h"

#import <Metal/Metal.h>

namespace nwave {

struct MetalDevice::Impl {
    id<MTLDevice> device;
    id<MTLCommandQueue> command_queue;

    Impl() : device(nil), command_queue(nil) {
        device = MTLCreateSystemDefaultDevice();
        if (device) {
            command_queue = [device newCommandQueue];
        }
    }
};

MetalDevice::MetalDevice() : impl_(std::make_unique<Impl>()) {
}

MetalDevice::~MetalDevice() = default;

bool MetalDevice::is_available() const {
    return impl_->device != nil && impl_->command_queue != nil;
}

std::string MetalDevice::device_name() const {
    if (!impl_->device) {
        return "";
    }
    return std::string([impl_->device.name UTF8String]);
}

} // namespace nwave
