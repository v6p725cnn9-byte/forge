#pragma once

#include "engine/core/camera.hpp"
#include "engine/render/settings.hpp"
#include "engine/rhi/command.hpp"
#include "engine/rhi/host.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace forge::app {

struct LabInput {
    bool captured = false;
    bool jump = false;
    bool toggle_walk = false;
    bool boost = false;
    bool interact = false;
    bool place = false;
    glm::vec3 move{0};
};

class Lab {
public:
    virtual ~Lab() = default;
    virtual const char* name() const = 0;
    virtual std::vector<const char*> shaders() const = 0;
    virtual rhi::HostConfig host_config() const { return {}; }
    virtual bool setup(rhi::Host& host, Camera& camera) = 0;
    virtual void update(float dt, Camera& camera, const LabInput& input) = 0;
    virtual rhi::FrameResult draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                                  Uint32 height, Camera& camera, bool captured) = 0;
    virtual void teardown(rhi::Host& host) = 0;
    virtual std::uint32_t triangles() const { return 0; }
    render::DebugState& debug_state() { return debug_; }

protected:
    render::DebugState debug_{};
};

int run_lab(Lab& lab, const char* window_title);

} // namespace forge::app
