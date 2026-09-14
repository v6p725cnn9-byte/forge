#pragma once

#include "engine/platform/window/host.hpp"
#include "engine/rhi/device/resources.hpp"
#include "engine/render/framegraph/framegraph.hpp"

#include <SDL3/SDL.h>
#include <cstdint>

namespace forge::render {

struct FrameConfig {
    bool hdr = true;
    bool bloom = false;
};

// Owns scene targets and post pipelines. rhi::Host is window + device only.
class Renderer {
public:
    Renderer() = default;
    ~Renderer() { destroy(); }
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool prepare(rhi::Host& host);
    bool ensure(rhi::Host& host, std::uint32_t width, std::uint32_t height, FrameConfig config);
    void destroy();

    SDL_GPUTexture* hdr() const { return resources_.native(hdr_); }
    SDL_GPUTexture* depth() const { return resources_.native(depth_); }
    SDL_GPUTexture* bloom() const { return resources_.native(bloom_); }
    SDL_GPUTextureFormat hdr_format() const { return hdr_format_; }
    SDL_GPUTextureFormat depth_format() const { return depth_format_; }
    SDL_GPUSampler* linear_clamp() const { return linear_clamp_; }
    SDL_GPUGraphicsPipeline* tonemap_pipeline() const { return tonemap_; }
    SDL_GPUGraphicsPipeline* bloom_pipeline() const { return bloom_pipeline_; }
    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }
    FrameConfig config() const { return config_; }
    bool ready() const { return device_ != nullptr; }

    bool apply_bloom(rhi::Command& command, float threshold);
    bool apply_tonemap(rhi::Command& command, SDL_GPUTexture* swapchain, float exposure, float bloom_strength);
    bool post(rhi::Command& command, SDL_GPUTexture* swapchain, float exposure, float bloom_strength, float bloom_threshold);

private:
    bool create_post_pipelines(rhi::Host& host);

    rhi::Resources resources_;
    rhi::TextureHandle hdr_{};
    rhi::TextureHandle depth_{};
    rhi::TextureHandle bloom_{};
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUSampler* linear_clamp_ = nullptr;
    SDL_GPUGraphicsPipeline* tonemap_ = nullptr;
    SDL_GPUGraphicsPipeline* bloom_pipeline_ = nullptr;
    SDL_GPUTextureFormat depth_format_ = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    SDL_GPUTextureFormat hdr_format_ = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    FrameConfig config_{};
};

} // namespace forge::render
