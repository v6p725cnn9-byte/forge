#pragma once

#include "engine/app/lab.hpp"
#include "engine/phys/world.hpp"
#include "engine/render/pbr_scene.hpp"

namespace forge::labs {

class ShadowWalkLab final : public app::Lab {
public:
    const char* name() const override { return "m3"; }
    std::vector<const char*> shaders() const override
    {
        return {"pbr.vert", "pbr.frag", "tonemap.vert", "tonemap.frag", "bloom.frag", "shadow.vert", "shadow.frag"};
    }
    rhi::HostConfig host_config() const override { return {.hdr = true, .bloom = true}; }
    bool setup(rhi::Host& host, Camera& camera) override;
    void update(float dt, Camera& camera, const app::LabInput& input) override;
    rhi::FrameResult draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                          Uint32 height, Camera& camera, bool captured) override;
    void teardown(rhi::Host& host) override;
    std::uint32_t triangles() const override { return scene_.triangle_count; }

private:
    void compute_cascades(const Camera& camera, float aspect);
    bool create_shadow_map(rhi::Host& host);

    render::PbrScene scene_;
    phys::World physics_;
    SDL_GPUGraphicsPipeline* pbr_cull_ = nullptr;
    SDL_GPUGraphicsPipeline* pbr_double_ = nullptr;
    SDL_GPUGraphicsPipeline* shadow_pipeline_ = nullptr;
    SDL_GPUTexture* shadow_map_ = nullptr;
    SDL_GPUSampler* shadow_sampler_ = nullptr;
    render::ShadowInputs shadows_{};
    glm::vec3 character_{0, 1.2f, 2.5f};
    float accumulator_ = 0;
};

} // namespace forge::labs
