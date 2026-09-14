#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace forge::rhi {

class Device {
public:
    Device() = default;
    ~Device() { destroy(); }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    bool create(SDL_GPUShaderFormat formats, bool debug)
    {
        destroy();
        handle_ = SDL_CreateGPUDevice(formats, debug, nullptr);
        if (!handle_) return false;
        backend_ = SDL_GetGPUDeviceDriver(handle_);
        return true;
    }

    void destroy()
    {
        if (handle_) {
            SDL_WaitForGPUIdle(handle_);
            SDL_DestroyGPUDevice(handle_);
        }
        handle_ = nullptr;
        backend_ = "none";
    }

    SDL_GPUDevice* native() const { return handle_; }
    const char* backend() const { return backend_.c_str(); }

private:
    SDL_GPUDevice* handle_ = nullptr;
    std::string backend_ = "none";
};

} // namespace forge::rhi
