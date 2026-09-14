#include "engine/render/passes/ui/backdrop.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace forge::ui {
namespace {

constexpr int kMaxLevels = 6;

int mip_levels(std::uint32_t width, std::uint32_t height)
{
    const float side = static_cast<float>(std::min(width, height));
    if (!(side >= 1.0f)) return 1;
    return std::clamp(1 + static_cast<int>(std::floor(std::log2(side))), 1, kMaxLevels);
}

} // namespace

bool Backdrop::create(SDL_GPUDevice* device)
{
    destroy(device);
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_ = SDL_CreateGPUSampler(device, &sampler);
    SDL_GPUTextureCreateInfo flat{};
    flat.type = SDL_GPU_TEXTURETYPE_2D;
    flat.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    flat.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    flat.width = 4;
    flat.height = 4;
    flat.layer_count_or_depth = 1;
    flat.num_levels = 1;
    flat_ = SDL_CreateGPUTexture(device, &flat);
    if (!sampler_ || !flat_) {
        destroy(device);
        return false;
    }
    SDL_GPUTransferBufferCreateInfo transfer{};
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer.size = 64;
    SDL_GPUTransferBuffer* upload = SDL_CreateGPUTransferBuffer(device, &transfer);
    if (!upload) {
        destroy(device);
        return false;
    }
    auto* map = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device, upload, false));
    if (!map) {
        SDL_ReleaseGPUTransferBuffer(device, upload);
        destroy(device);
        return false;
    }
    std::memset(map, 255, 64);
    SDL_UnmapGPUTransferBuffer(device, upload);
    SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
    if (!command) {
        SDL_ReleaseGPUTransferBuffer(device, upload);
        destroy(device);
        return false;
    }
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
        SDL_ReleaseGPUTransferBuffer(device, upload);
        SDL_CancelGPUCommandBuffer(command);
        destroy(device);
        return false;
    }
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = upload;
    SDL_GPUTextureRegion target{};
    target.texture = flat_;
    target.w = 4;
    target.h = 4;
    target.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &target, false);
    SDL_EndGPUCopyPass(copy);
    const bool submitted = SDL_SubmitGPUCommandBuffer(command);
    SDL_ReleaseGPUTransferBuffer(device, upload);
    if (!submitted || !SDL_WaitForGPUIdle(device)) {
        destroy(device);
        return false;
    }
    return true;
}

void Backdrop::destroy(SDL_GPUDevice* device)
{
    if (!device) return;
    if (chain_) SDL_ReleaseGPUTexture(device, chain_);
    if (flat_) SDL_ReleaseGPUTexture(device, flat_);
    if (sampler_) SDL_ReleaseGPUSampler(device, sampler_);
    chain_ = nullptr;
    flat_ = nullptr;
    sampler_ = nullptr;
    width_ = height_ = 0;
}

bool Backdrop::rebuild(SDL_GPUDevice* device, std::uint32_t width, std::uint32_t height)
{
    if (chain_) SDL_ReleaseGPUTexture(device, chain_);
    chain_ = nullptr;
    width_ = height_ = 0;
    if (format_ == SDL_GPU_TEXTUREFORMAT_INVALID || width == 0 || height == 0) return false;
    SDL_GPUTextureCreateInfo chain{};
    chain.type = SDL_GPU_TEXTURETYPE_2D;
    chain.format = format_;
    chain.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    chain.width = width;
    chain.height = height;
    chain.layer_count_or_depth = 1;
    chain.num_levels = static_cast<Uint32>(mip_levels(width, height));
    chain_ = SDL_CreateGPUTexture(device, &chain);
    if (!chain_) return false;
    width_ = width;
    height_ = height;
    return true;
}

bool Backdrop::prepare(SDL_GPUDevice* device, SDL_GPUCommandBuffer* command, SDL_GPUTexture* source,
                       std::uint32_t width, std::uint32_t height)
{
    if (!device || !command || !source || !sampler_ || width == 0 || height == 0) return false;
    if (!chain_ || width_ != width || height_ != height) {
        if (!rebuild(device, width, height)) return false;
    }
    SDL_GPUBlitInfo blit{};
    blit.source.texture = source;
    blit.source.mip_level = 0;
    blit.source.layer_or_depth_plane = 0;
    blit.source.x = 0;
    blit.source.y = 0;
    blit.source.w = width;
    blit.source.h = height;
    blit.destination.texture = chain_;
    blit.destination.mip_level = 0;
    blit.destination.layer_or_depth_plane = 0;
    blit.destination.x = 0;
    blit.destination.y = 0;
    blit.destination.w = width;
    blit.destination.h = height;
    blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
    blit.flip_mode = SDL_FLIP_NONE;
    blit.filter = SDL_GPU_FILTER_LINEAR;
    blit.cycle = false;
    SDL_BlitGPUTexture(command, &blit);
    SDL_GenerateMipmapsForGPUTexture(command, chain_);
    return true;
}

} // namespace forge::ui
