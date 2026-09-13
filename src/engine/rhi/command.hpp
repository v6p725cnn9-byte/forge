#pragma once
#include <SDL3/SDL.h>
#include <utility>

namespace forge::rhi {
class Command {
public:
    explicit Command(SDL_GPUDevice* device) : handle(SDL_AcquireGPUCommandBuffer(device)) {}
    ~Command()
    {
        if (!handle) return;
        // SDL forbids cancellation once this command buffer owns a swapchain image.
        if (has_swapchain) SDL_SubmitGPUCommandBuffer(handle);
        else SDL_CancelGPUCommandBuffer(handle);
    }
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    bool submit() { return SDL_SubmitGPUCommandBuffer(std::exchange(handle, nullptr)); }
    SDL_GPUCommandBuffer* handle;
    bool has_swapchain = false;
};
} // namespace forge::rhi
