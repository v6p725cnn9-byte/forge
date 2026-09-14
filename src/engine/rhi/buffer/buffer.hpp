#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

namespace forge::rhi {

SDL_GPUBuffer* create_buffer(SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage, std::uint32_t size,
                             const void* bytes = nullptr);
void destroy_buffer(SDL_GPUDevice* device, SDL_GPUBuffer*& buffer);

} // namespace forge::rhi
