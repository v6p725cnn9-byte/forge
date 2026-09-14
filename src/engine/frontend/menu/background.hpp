#pragma once

#include "engine/platform/window/host.hpp"
#include "engine/rhi/texture/texture.hpp"

#include <SDL3/SDL.h>

namespace forge::app {

// Menu backdrop: a fullscreen cover-fit blit of assets/menu/background.png
// onto the swapchain. The UI pass then draws on top with LOAD (clear=false).
// If the asset is missing or undecodable the menu falls back to a solid
// clear color, so create() failing is non-fatal.
class MenuBackground {
public:
    bool create(rhi::Host& host);
    void destroy(rhi::Host& host);
    bool ready() const { return background_.handle != nullptr; }
    void blit(rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width, Uint32 height);

private:
    rhi::Texture background_{};
};

// Largest centered source sub-rectangle with the destination aspect ratio
// (cover fit: no letterbox, edges are cropped). Empty rect on bad input.
struct CoverCrop {
    Uint32 x = 0, y = 0, w = 0, h = 0;
};

CoverCrop cover_crop(Uint32 src_w, Uint32 src_h, Uint32 dst_w, Uint32 dst_h);

} // namespace forge::app
