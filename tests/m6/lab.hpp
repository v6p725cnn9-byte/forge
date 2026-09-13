#pragma once

#include "engine/app/lab.hpp"
#include "engine/phys/world.hpp"
#include "engine/render/pbr_scene.hpp"
#include "engine/script/vm.hpp"

#include <string>

namespace forge::labs {

class LuaGamemodeLab final : public app::Lab {
public:
    const char* name() const override { return "m6"; }
    std::vector<const char*> shaders() const override
    {
        return {"pbr.vert", "pbr.frag", "tonemap.vert", "tonemap.frag", "bloom.frag"};
    }
    rhi::HostConfig host_config() const override { return {.hdr = true, .bloom = false}; }
    bool setup(rhi::Host& host, Camera& camera) override;
    void update(float dt, Camera& camera, const app::LabInput& input) override;
    rhi::FrameResult draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                          Uint32 height, Camera& camera, bool captured) override;
    void teardown(rhi::Host& host) override;
    std::uint32_t triangles() const override { return cube_.triangle_count * 24; }

private:
    void sync_debug();
    void follow_camera(Camera& camera) const;

    script::Registry world_;
    script::Vm vm_;
    render::PbrScene cube_;
    phys::World physics_;
    SDL_GPUGraphicsPipeline* pbr_ = nullptr;
    glm::vec3 character_{0, 1, 0};
    float character_yaw_ = 90.0f;
    bool driving_ = false;
    bool has_car_ = false;
    float accumulator_ = 0;
    std::string gamemode_name_ = "main.lua";
};

} // namespace forge::labs
