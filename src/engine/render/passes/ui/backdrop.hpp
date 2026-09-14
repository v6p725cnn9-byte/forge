#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

namespace forge::ui {

// Frosted glass in the UMG BackgroundBlur spirit: a mip chain of the frame
// behind the UI. This is a render-pipeline dependency (scene color → downsample
// → blur → UI composite), not a logical widget. Logical tree stays in ui::Ui.
class Backdrop {
public:
    bool create(SDL_GPUDevice* device);
    void destroy(SDL_GPUDevice* device);
    void set_format(SDL_GPUTextureFormat format) { format_ = format; }
    bool prepare(SDL_GPUDevice* device, SDL_GPUCommandBuffer* command, SDL_GPUTexture* swapchain, std::uint32_t width,
                 std::uint32_t height);
    SDL_GPUTexture* texture() const { return chain_ ? chain_ : flat_; }
    SDL_GPUSampler* sampler() const { return sampler_; }

private:
    bool rebuild(SDL_GPUDevice* device, std::uint32_t width, std::uint32_t height);

    SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUTexture* chain_ = nullptr;
    SDL_GPUTexture* flat_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

} // namespace forge::ui
