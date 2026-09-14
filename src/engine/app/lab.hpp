#pragma once

#include "engine/core/camera/camera.hpp"
#include "engine/platform/input/input.hpp"
#include "engine/render/renderer/renderer.hpp"
#include "engine/render/renderer/settings.hpp"
#include "engine/rhi/command/command.hpp"
#include "engine/platform/window/host.hpp"

#include <string>
#include <vector>

namespace forge::app {

using LabInput = platform::Input;
using platform::compose_walk;

class Lab {
public:
    virtual ~Lab() = default;
    virtual const char* name() const = 0;
    virtual std::vector<const char*> shaders() const = 0;
    virtual render::FrameConfig frame_config() const { return {}; }
    virtual bool setup(rhi::Host& host, render::Renderer& renderer, Camera& camera) = 0;
    virtual void update(float dt, Camera& camera, const LabInput& input) = 0;
    virtual rhi::FrameResult draw(rhi::Host& host, render::Renderer& renderer, rhi::Command& command,
                                  SDL_GPUTexture* swapchain, Uint32 width, Uint32 height, Camera& camera,
                                  bool captured) = 0;
    virtual void teardown(rhi::Host& host, render::Renderer& renderer) = 0;
    virtual std::uint32_t triangles() const { return 0; }
    render::DebugState& debug_state() { return debug_; }

protected:
    render::DebugState debug_{};
};

int run_lab(Lab& lab, const char* window_title);

} // namespace forge::app
