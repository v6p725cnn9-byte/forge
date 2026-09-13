#pragma once

#include "engine/rhi/host.hpp"

#include <glm/glm.hpp>

namespace forge::render {

SDL_GPUGraphicsPipeline* make_pbr_pipeline(rhi::Host& host, bool double_sided);
SDL_GPUGraphicsPipeline* make_pbr_instanced_pipeline(rhi::Host& host, bool double_sided);
SDL_GPUGraphicsPipeline* make_pbr_skinned_pipeline(rhi::Host& host, bool double_sided);
SDL_GPUGraphicsPipeline* make_shadow_pipeline(rhi::Host& host);
void apply_tonemap(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, float exposure,
                   float bloom_strength);
void apply_bloom(rhi::Host& host, rhi::Command& command, float threshold);

struct CameraUniforms {
    glm::mat4 view_projection{1.0f};
    glm::mat4 model{1.0f};
};

struct CameraViewUniforms {
    glm::mat4 view_projection{1.0f};
};

} // namespace forge::render
