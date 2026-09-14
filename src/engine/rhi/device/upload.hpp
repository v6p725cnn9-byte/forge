#pragma once

#include "engine/rhi/texture/texture.hpp"

#include <SDL3/SDL.h>
#include <cstdint>

namespace forge::rhi {

// Transfer-buffer upload path used by texture/buffer creation. Callers own
// the destination GPU resource; this only copies bytes.
bool upload_texture_bytes(SDL_GPUDevice* device, SDL_GPUTexture* texture, const void* bytes, std::uint32_t size,
                          const std::vector<TextureBlit>& slices, bool generate_mips);

} // namespace forge::rhi
