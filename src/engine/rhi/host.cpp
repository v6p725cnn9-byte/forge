#include "engine/rhi/host.hpp"

#include "engine/rhi/shader.hpp"
#include "engine/rhi/texture.hpp"

#include <algorithm>

namespace forge::rhi {
namespace {

bool fail(const char* operation)
{
    SDL_Log("%s failed: %s", operation, SDL_GetError());
    return false;
}

} // namespace

bool Host::open(const char* title, int width, int height, const std::vector<const char*>& shaders)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return fail("SDL_Init");
    sdl_started_ = true;
    const auto formats = available_shader_formats(shader_directory(), shaders);
    if (!formats) {
        SDL_Log("No complete cooked shader set in %s; rebuild forge", shader_directory().string().c_str());
        return false;
    }
    device_ = SDL_CreateGPUDevice(formats, FORGE_GPU_DEBUG != 0, nullptr);
    if (!device_) return fail("SDL_CreateGPUDevice");
    backend_ = SDL_GetGPUDeviceDriver(device_);
    SDL_Log("GPU backend: %s", backend_.c_str());
    window_ = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_) return fail("SDL_CreateWindow");
    if (!SDL_ClaimWindowForGPUDevice(device_, window_)) return fail("SDL_ClaimWindowForGPUDevice");
    claimed_ = true;
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    if (!SDL_GPUTextureSupportsFormat(device_, depth_format_, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        depth_format_ = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    }
    if (!SDL_GPUTextureSupportsFormat(device_, hdr_format_, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        hdr_format_ = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    }

    assets::TextureRef clamp;
    clamp.min_filter = 9729;
    clamp.mag_filter = 9729;
    clamp.wrap_s = 33071;
    clamp.wrap_t = 33071;
    linear_clamp_ = create_sampler(device_, clamp, true);
    if (!linear_clamp_) return fail("SDL_CreateGPUSampler (clamp)");
    if (!overlay_.init(window_, device_)) return false;
    SDL_Log("Forge %s host ready", FORGE_VERSION);
    return true;
}

bool Host::create_post_pipelines()
{
    if (tonemap_) return true;
    const auto directory = shader_directory();
    auto* tone_vs = load_shader(device_, directory, {"tonemap.vert", SDL_GPU_SHADERSTAGE_VERTEX});
    auto* tone_fs = load_shader(device_, directory, {"tonemap.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 2});
    auto* bloom_fs = load_shader(device_, directory, {"bloom.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1});
    if (!tone_vs || !tone_fs || !bloom_fs) {
        if (tone_vs) SDL_ReleaseGPUShader(device_, tone_vs);
        if (tone_fs) SDL_ReleaseGPUShader(device_, tone_fs);
        if (bloom_fs) SDL_ReleaseGPUShader(device_, bloom_fs);
        return fail("post-process shaders");
    }
    SDL_GPUColorTargetDescription swap{};
    swap.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    SDL_GPUGraphicsPipelineCreateInfo tone{};
    tone.vertex_shader = tone_vs;
    tone.fragment_shader = tone_fs;
    tone.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    tone.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    tone.target_info.num_color_targets = 1;
    tone.target_info.color_target_descriptions = &swap;
    tonemap_ = SDL_CreateGPUGraphicsPipeline(device_, &tone);

    SDL_GPUColorTargetDescription hdr{};
    hdr.format = hdr_format_;
    SDL_GPUGraphicsPipelineCreateInfo bloom{};
    bloom.vertex_shader = tone_vs;
    bloom.fragment_shader = bloom_fs;
    bloom.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    bloom.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    bloom.target_info.num_color_targets = 1;
    bloom.target_info.color_target_descriptions = &hdr;
    bloom_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &bloom);

    SDL_ReleaseGPUShader(device_, tone_vs);
    SDL_ReleaseGPUShader(device_, tone_fs);
    SDL_ReleaseGPUShader(device_, bloom_fs);
    return (tonemap_ && bloom_pipeline_) || fail("SDL_CreateGPUGraphicsPipeline (post)");
}

bool Host::resize(Uint32 width, Uint32 height, const HostConfig& config)
{
    config_ = config;
    if (config.hdr && !create_post_pipelines()) return false;
    if (width_ == width && height_ == height && depth_ && (!config.hdr || hdr_) && (!config.bloom || bloom_)) {
        return true;
    }

    auto make = [&](SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage, Uint32 w, Uint32 h) {
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = format;
        info.usage = usage;
        info.width = w;
        info.height = h;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        return SDL_CreateGPUTexture(device_, &info);
    };

    auto* depth = make(depth_format_,
                       SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, width, height);
    if (!depth) return fail("SDL_CreateGPUTexture (depth)");
    SDL_GPUTexture* hdr = nullptr;
    SDL_GPUTexture* bloom = nullptr;
    if (config.hdr) {
        hdr = make(hdr_format_, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, width, height);
        if (!hdr) {
            SDL_ReleaseGPUTexture(device_, depth);
            return fail("SDL_CreateGPUTexture (hdr)");
        }
    }
    if (config.bloom) {
        bloom = make(hdr_format_, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
                     std::max(1u, width / 2), std::max(1u, height / 2));
        if (!bloom) {
            if (hdr) SDL_ReleaseGPUTexture(device_, hdr);
            SDL_ReleaseGPUTexture(device_, depth);
            return fail("SDL_CreateGPUTexture (bloom)");
        }
    }
    if (hdr_) SDL_ReleaseGPUTexture(device_, hdr_);
    if (depth_) SDL_ReleaseGPUTexture(device_, depth_);
    if (bloom_) SDL_ReleaseGPUTexture(device_, bloom_);
    hdr_ = hdr;
    depth_ = depth;
    bloom_ = bloom;
    width_ = width;
    height_ = height;
    SDL_Log("Render targets: %ux%u pixels", width, height);
    return true;
}

bool Host::apply_display(int width, int height, bool fullscreen, bool vsync)
{
    if (!window_ || !device_) return false;
    if (fullscreen) {
        if (!SDL_SetWindowFullscreen(window_, true)) return fail("SDL_SetWindowFullscreen");
    } else {
        if (!SDL_SetWindowFullscreen(window_, false)) return fail("SDL_SetWindowFullscreen");
        if (!SDL_SetWindowSize(window_, width, height)) return fail("SDL_SetWindowSize");
    }
    auto present = vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
    if (!SDL_WindowSupportsGPUPresentMode(device_, window_, present)) {
        present = vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_MAILBOX;
        if (!SDL_WindowSupportsGPUPresentMode(device_, window_, present)) present = SDL_GPU_PRESENTMODE_VSYNC;
    }
    if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present))
        SDL_Log("Swapchain present mode fallback: %s", SDL_GetError());
    return true;
}

bool Host::present_overlay(Command& command, SDL_GPUTexture* swapchain)
{
    SDL_GPUColorTargetInfo color{};
    color.texture = swapchain;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, nullptr);
    if (!pass) return fail("SDL_BeginGPURenderPass (overlay)");
    overlay_.render(command.handle, pass);
    SDL_EndGPURenderPass(pass);
    return true;
}

void Host::close()
{
    if (device_) SDL_WaitForGPUIdle(device_);
    overlay_.shutdown();
    if (hdr_) SDL_ReleaseGPUTexture(device_, hdr_);
    if (depth_) SDL_ReleaseGPUTexture(device_, depth_);
    if (bloom_) SDL_ReleaseGPUTexture(device_, bloom_);
    if (linear_clamp_) SDL_ReleaseGPUSampler(device_, linear_clamp_);
    if (tonemap_) SDL_ReleaseGPUGraphicsPipeline(device_, tonemap_);
    if (bloom_pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, bloom_pipeline_);
    hdr_ = depth_ = bloom_ = nullptr;
    linear_clamp_ = nullptr;
    tonemap_ = bloom_pipeline_ = nullptr;
    if (claimed_) SDL_ReleaseWindowFromGPUDevice(device_, window_);
    claimed_ = false;
    if (window_) SDL_DestroyWindow(window_);
    if (device_) SDL_DestroyGPUDevice(device_);
    window_ = nullptr;
    device_ = nullptr;
    if (sdl_started_) SDL_Quit();
    sdl_started_ = false;
}

} // namespace forge::rhi
