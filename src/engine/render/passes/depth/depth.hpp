#pragma once

#include "engine/render/renderer/renderer.hpp"

namespace forge::render {

inline SDL_GPUTexture* scene_depth(const Renderer& renderer) { return renderer.depth(); }

} // namespace forge::render
