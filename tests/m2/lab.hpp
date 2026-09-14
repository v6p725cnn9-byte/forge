#pragma once

#include "engine/app/lab.hpp"
#include "engine/render/passes/opaque/pbr_scene.hpp"

namespace forge::labs {

class PbrHelmetLab final : public app::Lab {
public:
    const char* name() const override { return "m2"; }
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
    std::uint32_t triangles() const override { return scene_.triangle_count; }

protected:
    render::PbrScene scene_;
    SDL_GPUGraphicsPipeline* pbr_cull_ = nullptr;
    SDL_GPUGraphicsPipeline* pbr_double_ = nullptr;
};

} // namespace forge::labs
