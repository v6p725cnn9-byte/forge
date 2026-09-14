#pragma once

#include "engine/anim/skeleton/animator.hpp"
#include "engine/app/lab.hpp"
#include "engine/physics/world/world.hpp"
#include "engine/render/passes/opaque/pbr_scene.hpp"

namespace forge::labs {

class SkinVehicleLab final : public app::Lab {
public:
    const char* name() const override { return "m5"; }
    std::vector<const char*> shaders() const override
    {
        return {"pbr.vert", "pbr.frag", "pbr_skinned.vert", "tonemap.vert", "tonemap.frag", "bloom.frag"};
    }
    render::FrameConfig frame_config() const override { return {.hdr = true, .bloom = false}; }
    bool setup(rhi::Host& host, render::Renderer& renderer, Camera& camera) override;
    void update(float dt, Camera& camera, const app::LabInput& input) override;
    rhi::FrameResult draw(rhi::Host& host, render::Renderer& renderer, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                          Uint32 height, Camera& camera, bool captured) override;
    void teardown(rhi::Host& host, render::Renderer& renderer) override;
    std::uint32_t triangles() const override { return fox_.triangle_count + cube_.triangle_count * 8; }

private:
    void follow_camera(Camera& camera) const;

    render::PbrScene fox_;
    render::PbrScene cube_;
    phys::World physics_;
    SDL_GPUGraphicsPipeline* pbr_ = nullptr;
    SDL_GPUGraphicsPipeline* skinned_ = nullptr;
    anim::Palette palette_{};
    glm::vec3 character_{0, 0.7f, 0};
    float character_yaw_ = 90.0f;
    float anim_time_ = 0;
    int clip_walk_ = -1;
    int clip_run_ = -1;
    int clip_idle_ = -1;
    int clip_ = -1;
    bool driving_ = false;
    float accumulator_ = 0;
    float fox_scale_ = 0.012f;
};

} // namespace forge::labs
