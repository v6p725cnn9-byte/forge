#pragma once

#include "engine/rhi/device/handles.hpp"

#include <SDL3/SDL.h>
#include <vector>

namespace forge::rhi {

struct TextureCreate {
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUTextureUsageFlags usage = 0;
    std::uint32_t width = 1;
    std::uint32_t height = 1;
    std::uint32_t layers = 1;
    std::uint32_t levels = 1;
    SDL_GPUTextureType type = SDL_GPU_TEXTURETYPE_2D;
};

// Owns GPU objects for one device. Call destroy_all() before destroying the
// device (resize, level change, shutdown). Handles go stale after destroy.
class Resources {
public:
    Resources() = default;
    ~Resources() { destroy_all(); }
    Resources(const Resources&) = delete;
    Resources& operator=(const Resources&) = delete;

    void bind(SDL_GPUDevice* device) { device_ = device; }
    SDL_GPUDevice* device() const { return device_; }

    TextureHandle create_texture(const TextureCreate& create);
    void destroy(TextureHandle handle);
    SDL_GPUTexture* native(TextureHandle handle) const;

    void destroy_all();

private:
    struct TextureSlot {
        SDL_GPUTexture* native = nullptr;
        std::uint32_t generation = 0;
    };

    SDL_GPUDevice* device_ = nullptr;
    std::vector<TextureSlot> textures_;
};

} // namespace forge::rhi
