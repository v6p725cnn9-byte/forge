#pragma once

#include "engine/app/lab.hpp"
#include "engine/game/session/session.hpp"
#include "engine/game_net/client/client.hpp"
#include "engine/game_net/server/server.hpp"
#include "engine/render/passes/opaque/pbr_scene.hpp"
#include "engine/render/renderer/survival_visuals.hpp"
#include "engine/script/bindings/registry.hpp"

#include <string>

namespace forge::labs {

struct SessionLaunch {
    bool hosting = true;
    bool lan = false;
    std::string connect;
    int port = 27015;
};

class SurvivalLab final : public app::Lab {
public:
    const char* name() const override { return "survival"; }
    void configure(const SessionLaunch& launch)
    {
        launch_ = launch;
        configured_ = true;
    }
    std::vector<const char*> shaders() const override
    {
        return {"pbr.vert", "pbr_skinned.vert", "pbr.frag", "tonemap.vert", "tonemap.frag", "bloom.frag"};
    }
    render::FrameConfig frame_config() const override { return {.hdr = true, .bloom = true}; }
    bool setup(rhi::Host& host, render::Renderer& renderer, Camera& camera) override;
    const net::Snapshot& snapshot() const { return client_.snapshot(); }
    bool action_pending() const { return client_.action_pending(); }
    bool request(game::Action action, std::uint8_t argument) { return client_.request(action, argument); }
    void update(float dt, Camera& camera, const app::LabInput& input) override;
    rhi::FrameResult draw(rhi::Host& host, render::Renderer& renderer, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                          Uint32 height, Camera& camera, bool captured) override;
    void teardown(rhi::Host& host, render::Renderer& renderer) override;
    std::uint32_t triangles() const override { return visuals_.triangles() + cube_.triangle_count * 2; }

private:
    void sync_debug();
    void follow_camera(Camera& camera) const;
    const net::Ghost* self() const;

    script::Registry world_;
    game::Sim sim_;
    net::Server server_;
    net::Client client_;
    render::PbrScene cube_;
    render::SurvivalVisuals visuals_;
    SDL_GPUGraphicsPipeline* pbr_ = nullptr;
    bool hosting_ = true;
    bool configured_ = false;
    float yaw_ = 0;
    bool boosting_ = false;
    std::string join_line_;
    SessionLaunch launch_{};
};

} // namespace forge::labs

namespace forge::app {
int run_game(const char* window_title);
}
