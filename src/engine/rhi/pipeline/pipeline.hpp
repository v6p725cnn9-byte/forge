#pragma once

#include <SDL3/SDL.h>

namespace forge::rhi {

inline void destroy_pipeline(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline*& pipeline)
{
    if (device && pipeline) SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    pipeline = nullptr;
}

} // namespace forge::rhi
