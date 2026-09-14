#pragma once

#include "engine/core/camera/camera.hpp"
#include "engine/render/passes/opaque/pbr_scene.hpp"
#include "engine/render/renderer/renderer.hpp"
#include "engine/rhi/command/command.hpp"
#include "engine/rhi/device/device.hpp"

#include <SDL3/SDL.h>

namespace forge::app {

// Minimal 3D menu backdrop: a loaded glTF model on a slow orbit, drawn with
// the regular PBR + tonemap pass. The menu UI is submitted on top with
// LOAD (clear=false). Falls back to the static image when create() fails.
class MenuScene {
public:
    bool create(SDL_GPUDevice* device, render::Renderer& renderer);
    void destroy(SDL_GPUDevice* device);
    bool ready() const { return pbr_cull_ != nullptr; }
    bool draw(rhi::Device& gpu, SDL_Window* window, render::Renderer& renderer, rhi::Command& command,
              SDL_GPUTexture* swapchain, Uint32 width, Uint32 height, float now_seconds);

private:
    render::PbrScene vista_;
    render::PbrScene helmet_;
    render::DebugState debug_{};
    Camera camera_{};
    SDL_GPUGraphicsPipeline* pbr_cull_ = nullptr;
    SDL_GPUGraphicsPipeline* pbr_double_ = nullptr;
    float helmet_lift_ = 0.0f;
};

} // namespace forge::app
