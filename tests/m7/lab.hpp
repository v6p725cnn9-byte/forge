#pragma once

#include "engine/app/lab.hpp"
#include "engine/game_net/client/client.hpp"
#include "engine/game_net/server/server.hpp"
#include "engine/render/passes/opaque/pbr_scene.hpp"
#include "engine/script/vm/vm.hpp"

#include <string>

namespace forge::labs {

class NetLab final : public app::Lab {
public:
    const char* name() const override { return "m7"; }
    std::vector<const char*> shaders() const override
    {
        return {"pbr.vert", "pbr.frag", "tonemap.vert", "tonemap.frag", "bloom.frag"};
    }
    render::FrameConfig frame_config() const override { return {.hdr = true, .bloom = false}; }
    bool setup(rhi::Host& host, render::Renderer& renderer, Camera& camera) override;
    void update(float dt, Camera& camera, const app::LabInput& input) override;
    rhi::FrameResult draw(rhi::Host& host, render::Renderer& renderer, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                          Uint32 height, Camera& camera, bool captured) override;
    void teardown(rhi::Host& host, render::Renderer& renderer) override;
    std::uint32_t triangles() const override { return cube_.triangle_count * 20; }

private:
    void sync_debug();
    void follow_camera(Camera& camera) const;
    const net::Ghost* self() const;

    script::Registry world_;
    script::Vm vm_;
    net::Server server_;
    net::Client client_;
    render::PbrScene cube_;
    SDL_GPUGraphicsPipeline* pbr_ = nullptr;
    std::string gamemode_name_ = "net.lua";
    float yaw_ = 90.0f;
};

} // namespace forge::labs
