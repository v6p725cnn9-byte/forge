#pragma once

#include "engine/render/renderer/renderer.hpp"
#include "engine/platform/window/host.hpp"

#include <glm/glm.hpp>

namespace forge::render {

SDL_GPUGraphicsPipeline* make_pbr_pipeline(rhi::Host& host, const Renderer& renderer, bool double_sided);
SDL_GPUGraphicsPipeline* make_pbr_instanced_pipeline(rhi::Host& host, const Renderer& renderer, bool double_sided);
SDL_GPUGraphicsPipeline* make_pbr_skinned_pipeline(rhi::Host& host, const Renderer& renderer, bool double_sided);
SDL_GPUGraphicsPipeline* make_shadow_pipeline(rhi::Host& host, const Renderer& renderer);

struct CameraUniforms {
    glm::mat4 view_projection{1.0f};
    glm::mat4 model{1.0f};
};

struct CameraViewUniforms {
    glm::mat4 view_projection{1.0f};
};

} // namespace forge::render
