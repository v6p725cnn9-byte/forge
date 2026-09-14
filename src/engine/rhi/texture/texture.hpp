#pragma once

#include "engine/assets/gltf/scene.hpp"
#include "engine/rhi/sampler/sampler.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <string>
#include <vector>

namespace forge::rhi {

struct Texture {
    SDL_GPUTexture* handle = nullptr;
    Uint32 levels = 1;
    Uint32 faces = 1;
    Uint32 width = 0;
    Uint32 height = 0;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
};

struct TextureBlit {
    Uint32 offset = 0;
    Uint32 level = 0;
    Uint32 face = 0;
    Uint32 width = 1;
    Uint32 height = 1;
};

Texture upload_texture(SDL_GPUDevice* device, SDL_GPUTextureFormat format, Uint32 width, Uint32 height,
                       Uint32 levels, Uint32 faces, const void* bytes, Uint32 size,
                       const std::vector<TextureBlit>& slices, bool generate_mips);

Texture load_texture(SDL_GPUDevice* device, const assets::ImageSource& source, bool srgb, std::string& error);
Texture solid_texture(SDL_GPUDevice* device, std::array<Uint8, 4> color, bool srgb);

void destroy_texture(SDL_GPUDevice* device, Texture& texture);

} // namespace forge::rhi
