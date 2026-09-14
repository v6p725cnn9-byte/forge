#include "engine/app/background.hpp"

#include "engine/assets/scene.hpp"
#include "engine/core/paths.hpp"
#include "engine/rhi/host.hpp"

#include <SDL3/SDL.h>
#include <string>

namespace forge::app {

CoverCrop cover_crop(Uint32 src_w, Uint32 src_h, Uint32 dst_w, Uint32 dst_h)
{
    if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) return {};
    // Compare aspects with cross products to stay in integer math.
    const auto left = static_cast<std::uint64_t>(dst_w) * src_h;
    const auto right = static_cast<std::uint64_t>(src_w) * dst_h;
    CoverCrop crop{0, 0, src_w, src_h};
    if (left > right) {
        // Destination is wider: crop the top and bottom of the source.
        crop.h = static_cast<Uint32>(static_cast<std::uint64_t>(src_w) * dst_h / dst_w);
        if (crop.h == 0) return {};
        crop.y = (src_h - crop.h) / 2;
    } else if (left < right) {
        // Destination is taller: crop the left and right of the source.
        crop.w = static_cast<Uint32>(static_cast<std::uint64_t>(src_h) * dst_w / dst_h);
        if (crop.w == 0) return {};
        crop.x = (src_w - crop.w) / 2;
    }
    return crop;
}

bool MenuBackground::create(rhi::Host& host)
{
    destroy(host);
    if (!host.device()) return false;
    assets::ImageSource source;
    source.path = forge::assets_directory() / "menu/background.png";
    // UNORM, not sRGB: the blit copies raw texels into the swapchain, which
    // the UI pass already treats as final display values.
    std::string error;
    background_ = rhi::load_texture(host.device(), source, false, error);
    if (!background_.handle) {
        SDL_Log("Menu background unavailable (%s): %s", source.path.string().c_str(), error.c_str());
        return false;
    }
    SDL_Log("Menu background: %s (%ux%u)", source.path.string().c_str(), background_.width, background_.height);
    return true;
}

void MenuBackground::destroy(rhi::Host& host)
{
    if (background_.handle) rhi::destroy_texture(host.device(), background_);
    background_ = {};
}

void MenuBackground::blit(rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width, Uint32 height)
{
    if (!background_.handle || !swapchain || width == 0 || height == 0) return;
    const CoverCrop crop = cover_crop(background_.width, background_.height, width, height);
    if (crop.w == 0 || crop.h == 0) return;
    SDL_GPUBlitInfo info{};
    info.source.texture = background_.handle;
    info.source.x = crop.x;
    info.source.y = crop.y;
    info.source.w = crop.w;
    info.source.h = crop.h;
    info.destination.texture = swapchain;
    info.destination.w = width;
    info.destination.h = height;
    info.load_op = SDL_GPU_LOADOP_CLEAR;
    info.clear_color = {0.04f, 0.06f, 0.05f, 1.0f};
    info.flip_mode = SDL_FLIP_NONE;
    info.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(command.handle, &info);
}

} // namespace forge::app
