#pragma once

#include "engine/render/passes/opaque/pbr_pass.hpp"
#include "engine/render/passes/opaque/pbr_scene.hpp"

namespace forge::render {

inline SDL_GPUTexture* scene_color(const Renderer& renderer) { return renderer.hdr(); }

} // namespace forge::render
