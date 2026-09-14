#pragma once

#include "engine/assets/gltf/scene.hpp"

#include <SDL3/SDL.h>

namespace forge::rhi {

SDL_GPUSampler* create_sampler(SDL_GPUDevice* device, const assets::TextureRef& ref, bool clamp = false);

} // namespace forge::rhi
