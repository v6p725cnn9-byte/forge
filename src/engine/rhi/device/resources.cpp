#include "engine/rhi/device/resources.hpp"

namespace forge::rhi {

TextureHandle Resources::create_texture(const TextureCreate& create)
{
    TextureHandle handle{};
    if (!device_ || create.width == 0 || create.height == 0) return handle;
    SDL_GPUTextureCreateInfo info{};
    info.type = create.type;
    info.format = create.format;
    info.usage = create.usage;
    info.width = create.width;
    info.height = create.height;
    info.layer_count_or_depth = create.layers;
    info.num_levels = create.levels;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    auto* texture = SDL_CreateGPUTexture(device_, &info);
    if (!texture) return handle;
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(textures_.size()); ++i) {
        if (textures_[i].native) continue;
        textures_[i].native = texture;
        ++textures_[i].generation;
        if (textures_[i].generation == 0) textures_[i].generation = 1;
        return {i + 1, textures_[i].generation};
    }
    textures_.push_back({texture, 1});
    return {static_cast<std::uint32_t>(textures_.size()), 1};
}

void Resources::destroy(TextureHandle handle)
{
    if (!handle || handle.index == 0 || handle.index > textures_.size()) return;
    auto& slot = textures_[handle.index - 1];
    if (slot.generation != handle.generation || !slot.native) return;
    if (device_) SDL_ReleaseGPUTexture(device_, slot.native);
    slot.native = nullptr;
}

SDL_GPUTexture* Resources::native(TextureHandle handle) const
{
    if (!handle || handle.index == 0 || handle.index > textures_.size()) return nullptr;
    const auto& slot = textures_[handle.index - 1];
    if (slot.generation != handle.generation) return nullptr;
    return slot.native;
}

void Resources::destroy_all()
{
    if (device_) {
        for (auto& slot : textures_) {
            if (slot.native) SDL_ReleaseGPUTexture(device_, slot.native);
            slot.native = nullptr;
        }
    } else {
        for (auto& slot : textures_) slot.native = nullptr;
    }
}

} // namespace forge::rhi
