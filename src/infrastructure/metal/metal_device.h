#ifndef NWAVE_INFRASTRUCTURE_METAL_METAL_DEVICE_H
#define NWAVE_INFRASTRUCTURE_METAL_METAL_DEVICE_H

#include <memory>
#include <string>

namespace nwave {

/// Wraps a Metal GPU device and command queue.
/// Pure C++ header -- Objective-C types are hidden behind pimpl.
class MetalDevice {
public:
    MetalDevice();
    ~MetalDevice();

    MetalDevice(const MetalDevice&) = delete;
    MetalDevice& operator=(const MetalDevice&) = delete;

    bool is_available() const;
    std::string device_name() const;

    /// Loads a compiled Metal shader library (.metallib) from the given path.
    bool load_library(const std::string& path);

    /// Creates a compute pipeline state for the named kernel function.
    /// Requires a library to have been loaded first via load_library().
    bool create_pipeline(const std::string& function_name);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_METAL_METAL_DEVICE_H
