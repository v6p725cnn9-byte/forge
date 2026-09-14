#pragma once

#include "engine/render/passes/post/composite.hpp"
#include "engine/render/renderer/renderer.hpp"

namespace forge::render {

inline bool apply_post(Renderer& renderer, rhi::Command& command, SDL_GPUTexture* swapchain, float exposure,
                       float bloom_strength, float bloom_threshold)
{
    return renderer.post(command, swapchain, exposure, bloom_strength, bloom_threshold);
}

} // namespace forge::render
