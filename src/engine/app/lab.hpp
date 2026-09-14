#pragma once

#include "engine/core/camera.hpp"
#include "engine/render/settings.hpp"
#include "engine/rhi/command.hpp"
#include "engine/rhi/host.hpp"

#include <glm/glm.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace forge::app {

struct LabInput {
    bool captured = false;
    bool jump = false;
    bool toggle_walk = false;
    bool toggle_person = false;
    bool boost = false;
    bool interact = false;
    bool place = false;
    glm::vec3 move{0};
};

// Camera-relative ground movement from WASD, capped at unit length. A raw
// diagonal reaches 1.41 per axis, which the network input validation rejects
// outright; normalizing keeps full speed without tripping it.
inline glm::vec3 compose_walk(const glm::vec3& flat_forward, float strafe, float push)
{
    if (glm::length(flat_forward) < 1e-6f) return glm::vec3{0};
    const glm::vec3 right = glm::normalize(glm::cross(flat_forward, glm::vec3{0.0f, 1.0f, 0.0f}));
    glm::vec3 walk = right * strafe + flat_forward * push;
    const float length = glm::length(glm::vec2(walk.x, walk.z));
    if (length > 1.0f) walk *= 1.0f / length;
    walk.x = std::clamp(walk.x, -1.0f, 1.0f);
    walk.z = std::clamp(walk.z, -1.0f, 1.0f);
    return walk;
}

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
