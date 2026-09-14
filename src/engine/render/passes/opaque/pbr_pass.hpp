#pragma once

#include "engine/render/renderer/renderer.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

namespace forge::render {

SDL_GPUGraphicsPipeline* make_pbr_pipeline(SDL_GPUDevice* device, const Renderer& renderer, bool double_sided);
SDL_GPUGraphicsPipeline* make_pbr_instanced_pipeline(SDL_GPUDevice* device, const Renderer& renderer, bool double_sided);
SDL_GPUGraphicsPipeline* make_pbr_skinned_pipeline(SDL_GPUDevice* device, const Renderer& renderer, bool double_sided);
SDL_GPUGraphicsPipeline* make_shadow_pipeline(SDL_GPUDevice* device, const Renderer& renderer);

struct CameraUniforms {
    glm::mat4 view_projection{1.0f};
    glm::mat4 model{1.0f};
};

struct CameraViewUniforms {
    glm::mat4 view_projection{1.0f};
};

} // namespace forge::render
