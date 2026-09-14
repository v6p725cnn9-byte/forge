#pragma once

#include "engine/render/passes/opaque/pbr_pass.hpp"

namespace forge::render {

inline SDL_GPUGraphicsPipeline* make_depth_shadow_pipeline(rhi::Host& host, const Renderer& renderer)
{
    return make_shadow_pipeline(host, renderer);
}

} // namespace forge::render
