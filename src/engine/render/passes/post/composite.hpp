#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

namespace forge::render {

// Offscreen scene target in the swapchain format. The frame renders the 3D
// scene (or a clear color) here first so post/UI effects can sample it —
// swapchain textures cannot be blitted or sampled, only presented.
class Composite {
public:
    bool ensure(SDL_GPUDevice* device, SDL_GPUTextureFormat format, std::uint32_t width, std::uint32_t height);
    void destroy(SDL_GPUDevice* device);
    void clear(SDL_GPUCommandBuffer* command, SDL_FColor color);
    void present(SDL_GPUCommandBuffer* command, SDL_GPUTexture* swapchain, std::uint32_t width, std::uint32_t height);
    SDL_GPUTexture* texture() const { return color_; }

private:
    SDL_GPUTexture* color_ = nullptr;
    SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

} // namespace forge::render
