#include "engine/rhi/composite.hpp"

namespace forge::rhi {

bool Composite::ensure(SDL_GPUDevice* device, SDL_GPUTextureFormat format, std::uint32_t width, std::uint32_t height)
{
    if (!device || format == SDL_GPU_TEXTUREFORMAT_INVALID || width == 0 || height == 0) return false;
    if (color_ && format_ == format && width_ == width && height_ == height) return true;
    destroy(device);
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    color_ = SDL_CreateGPUTexture(device, &info);
    if (!color_) return false;
    format_ = format;
    width_ = width;
    height_ = height;
    return true;
}

void Composite::destroy(SDL_GPUDevice* device)
{
    if (device && color_) SDL_ReleaseGPUTexture(device, color_);
    color_ = nullptr;
    format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    width_ = height_ = 0;
}

void Composite::clear(SDL_GPUCommandBuffer* command, SDL_FColor color)
{
    if (!command || !color_) return;
    SDL_GPUColorTargetInfo target{};
    target.texture = color_;
    target.clear_color = color;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
    if (!pass) return;
    SDL_EndGPURenderPass(pass);
}

void Composite::present(SDL_GPUCommandBuffer* command, SDL_GPUTexture* swapchain, std::uint32_t width,
                        std::uint32_t height)
{
    if (!command || !color_ || !swapchain || width == 0 || height == 0) return;
    SDL_GPUBlitInfo info{};
    info.source.texture = color_;
    info.source.w = width_;
    info.source.h = height_;
    info.destination.texture = swapchain;
    info.destination.w = width;
    info.destination.h = height;
    info.load_op = SDL_GPU_LOADOP_CLEAR;
    info.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    info.flip_mode = SDL_FLIP_NONE;
    info.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(command, &info);
}

} // namespace forge::rhi
