#include "engine/rhi/swapchain/swapchain.hpp"

namespace forge::rhi {

bool set_present_mode(SDL_GPUDevice* device, SDL_Window* window, bool vsync)
{
    if (!device || !window) return false;
    auto present = vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
    if (!SDL_WindowSupportsGPUPresentMode(device, window, present)) {
        present = vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_MAILBOX;
        if (!SDL_WindowSupportsGPUPresentMode(device, window, present)) present = SDL_GPU_PRESENTMODE_VSYNC;
    }
    if (!SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present)) return false;
    return true;
}

} // namespace forge::rhi
