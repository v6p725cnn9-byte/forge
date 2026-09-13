#pragma once

#include "engine/app/lab.hpp"
#include "engine/render/pbr_scene.hpp"
#include "engine/world/world.hpp"

namespace forge::labs {

class StreamCityLab final : public app::Lab {
public:
    const char* name() const override { return "m4"; }
    std::vector<const char*> shaders() const override
    {
        return {"pbr_instanced.vert", "pbr.frag", "tonemap.vert", "tonemap.frag", "bloom.frag"};
    }
    rhi::HostConfig host_config() const override { return {.hdr = true, .bloom = false}; }
    bool setup(rhi::Host& host, Camera& camera) override;
    void update(float dt, Camera& camera, const app::LabInput& input) override;
    rhi::FrameResult draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                          Uint32 height, Camera& camera, bool captured) override;
    void teardown(rhi::Host& host) override;
    std::uint32_t triangles() const override { return 12u * stats_.instances_drawn; }

private:
    bool upload_instances(rhi::Host& host, rhi::Command& command);

    world::Store store_;
    world::SectorIndex index_;
    world::StreamStats stats_{};
    std::vector<world::GpuInstance> packed_;
    render::PbrScene scene_;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUBuffer* instances_ = nullptr;
    SDL_GPUTransferBuffer* transfer_ = nullptr;
    Uint32 instance_capacity_ = 0;
};

} // namespace forge::labs
