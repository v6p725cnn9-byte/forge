#pragma once

#include "engine/dev/overlay.hpp"
#include "engine/rhi/command.hpp"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace forge::rhi {

enum class FrameResult { presented, skipped, failed };

struct HostConfig {
    bool hdr = true;
    bool bloom = false;
};

class Host {
public:
    Host() = default;
    ~Host() { close(); }
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    bool open(const char* title, int width, int height, const std::vector<const char*>& shaders);
    void close();
    bool resize(Uint32 width, Uint32 height, const HostConfig& config);
    bool apply_display(int width, int height, bool fullscreen, bool vsync);

    SDL_Window* window() const { return window_; }
    SDL_GPUDevice* device() const { return device_; }
    const char* backend() const { return backend_.c_str(); }
    dev::Overlay& overlay() { return overlay_; }
    SDL_GPUTextureFormat depth_format() const { return depth_format_; }
    SDL_GPUTextureFormat hdr_format() const { return hdr_format_; }
    SDL_GPUTexture* hdr() const { return hdr_; }
    SDL_GPUTexture* depth() const { return depth_; }
    SDL_GPUTexture* bloom() const { return bloom_; }
    SDL_GPUSampler* linear_clamp() const { return linear_clamp_; }
    SDL_GPUGraphicsPipeline* tonemap_pipeline() const { return tonemap_; }
    SDL_GPUGraphicsPipeline* bloom_pipeline() const { return bloom_pipeline_; }
    Uint32 width() const { return width_; }
    Uint32 height() const { return height_; }

    bool present_overlay(Command& command, SDL_GPUTexture* swapchain);

private:
    bool create_post_pipelines();

    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUTexture* hdr_ = nullptr;
    SDL_GPUTexture* depth_ = nullptr;
    SDL_GPUTexture* bloom_ = nullptr;
    SDL_GPUSampler* linear_clamp_ = nullptr;
    SDL_GPUGraphicsPipeline* tonemap_ = nullptr;
    SDL_GPUGraphicsPipeline* bloom_pipeline_ = nullptr;
    SDL_GPUTextureFormat depth_format_ = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    SDL_GPUTextureFormat hdr_format_ = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    Uint32 width_ = 0;
    Uint32 height_ = 0;
    HostConfig config_{};
    dev::Overlay overlay_;
    std::string backend_ = "none";
    bool claimed_ = false;
    bool sdl_started_ = false;
};

} // namespace forge::rhi
