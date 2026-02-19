#include "infrastructure/metal/metal_device.h"

#import <Metal/Metal.h>

#include <unordered_map>

namespace nwave {

struct MetalDevice::Impl {
    id<MTLDevice> device;
    id<MTLCommandQueue> command_queue;
    id<MTLLibrary> library;
    std::unordered_map<std::string, id<MTLComputePipelineState>> pipelines;

    Impl() : device(nil), command_queue(nil), library(nil) {
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

bool MetalDevice::load_library(const std::string& path) {
    if (!impl_->device) {
        return false;
    }

    NSString* ns_path = [NSString stringWithUTF8String:path.c_str()];
    NSURL* url = [NSURL fileURLWithPath:ns_path];
    NSError* error = nil;
    impl_->library = [impl_->device newLibraryWithURL:url error:&error];

    if (error || !impl_->library) {
        return false;
    }
    return true;
}

bool MetalDevice::create_pipeline(const std::string& function_name) {
    if (!impl_->device || !impl_->library) {
        return false;
    }

    NSString* ns_name = [NSString stringWithUTF8String:function_name.c_str()];
    id<MTLFunction> function = [impl_->library newFunctionWithName:ns_name];
    if (!function) {
        return false;
    }

    NSError* error = nil;
    id<MTLComputePipelineState> pipeline =
        [impl_->device newComputePipelineStateWithFunction:function error:&error];

    if (error || !pipeline) {
        return false;
    }

    impl_->pipelines[function_name] = pipeline;
    return true;
}

void* MetalDevice::native_device() const {
    return (__bridge void*)impl_->device;
}

void* MetalDevice::native_command_queue() const {
    return (__bridge void*)impl_->command_queue;
}

void* MetalDevice::native_pipeline(const std::string& function_name) const {
    auto it = impl_->pipelines.find(function_name);
    if (it == impl_->pipelines.end()) {
        return nullptr;
    }
    return (__bridge void*)it->second;
}

} // namespace nwave
