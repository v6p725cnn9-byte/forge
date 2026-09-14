#include "engine/rhi/sampler/sampler.hpp"

namespace forge::rhi {

SDL_GPUSampler* create_sampler(SDL_GPUDevice* device, const assets::TextureRef& ref, bool clamp)
{
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = (ref.min_filter == 9728 || ref.min_filter == 9984 || ref.min_filter == 9986)
        ? SDL_GPU_FILTER_NEAREST
        : SDL_GPU_FILTER_LINEAR;
    info.mag_filter = ref.mag_filter == 9728 ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = (ref.min_filter == 9986 || ref.min_filter == 9987) ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
                                                                          : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    const auto wrap = [clamp](int value) {
        if (clamp) return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        if (value == 33071) return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        if (value == 33648) return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
        return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    };
    info.address_mode_u = wrap(ref.wrap_s);
    info.address_mode_v = wrap(ref.wrap_t);
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.max_lod = (ref.min_filter == 9728 || ref.min_filter == 9729) ? 0.0f : 16.0f;
    return SDL_CreateGPUSampler(device, &info);
}

} // namespace forge::rhi
