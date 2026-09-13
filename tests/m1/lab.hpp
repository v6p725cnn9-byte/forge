#pragma once

#include "engine/app/lab.hpp"

namespace forge::labs {

class UnlitCubesLab final : public app::Lab {
public:
    const char* name() const override { return "m1"; }
    std::vector<const char*> shaders() const override { return {"unlit.vert", "unlit.frag"}; }
    rhi::HostConfig host_config() const override { return {.hdr = false, .bloom = false}; }
    bool setup(rhi::Host& host, Camera& camera) override;
    void update(float dt, Camera& camera, const app::LabInput& input) override;
    rhi::FrameResult draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                          Uint32 height, Camera& camera, bool captured) override;
    void teardown(rhi::Host& host) override;
    std::uint32_t triangles() const override { return index_count_ / 3; }

private:
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUBuffer* vertices_ = nullptr;
    SDL_GPUBuffer* indices_ = nullptr;
    Uint32 index_count_ = 0;
};

} // namespace forge::labs
