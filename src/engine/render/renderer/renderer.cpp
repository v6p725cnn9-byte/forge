#include "engine/render/renderer/renderer.hpp"

#include "engine/assets/gltf/scene.hpp"
#include "engine/core/log/log.hpp"
#include "engine/core/profiler/profiler.hpp"
#include "engine/rhi/sampler/sampler.hpp"
#include "engine/rhi/shader/shader.hpp"
#include "engine/rhi/texture/texture.hpp"

#include <algorithm>
#include <glm/glm.hpp>

namespace forge::render {
namespace {

bool fail(const char* operation)
{
    forge::log::writef(forge::log::Channel::Render, forge::log::Level::Error, "%s failed: %s", operation,
                       SDL_GetError());
    return false;
}

} // namespace

bool Renderer::prepare(rhi::Host& host)
{
    destroy();
    device_ = host.device();
    if (!device_) return false;
    resources_.bind(device_);
    depth_format_ = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    if (!SDL_GPUTextureSupportsFormat(device_, depth_format_, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        depth_format_ = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    }
    hdr_format_ = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    if (!SDL_GPUTextureSupportsFormat(device_, hdr_format_, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        hdr_format_ = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    }
    assets::TextureRef clamp;
    clamp.min_filter = 9729;
    clamp.mag_filter = 9729;
    clamp.wrap_s = 33071;
    clamp.wrap_t = 33071;
    linear_clamp_ = rhi::create_sampler(device_, clamp, true);
    if (!linear_clamp_) return fail("SDL_CreateGPUSampler (clamp)");
    return true;
}

bool Renderer::create_post_pipelines(rhi::Host& host)
{
    if (tonemap_) return true;
    const auto directory = rhi::shader_directory();
    auto* tone_vs = rhi::load_shader(device_, directory, {"tonemap.vert", SDL_GPU_SHADERSTAGE_VERTEX});
    auto* tone_fs = rhi::load_shader(device_, directory, {"tonemap.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 2});
    auto* bloom_fs = rhi::load_shader(device_, directory, {"bloom.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1});
    if (!tone_vs || !tone_fs || !bloom_fs) {
        if (tone_vs) SDL_ReleaseGPUShader(device_, tone_vs);
        if (tone_fs) SDL_ReleaseGPUShader(device_, tone_fs);
        if (bloom_fs) SDL_ReleaseGPUShader(device_, bloom_fs);
        return fail("post-process shaders");
    }
    SDL_GPUColorTargetDescription swap{};
    swap.format = SDL_GetGPUSwapchainTextureFormat(device_, host.window());
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

bool Renderer::ensure(rhi::Host& host, std::uint32_t width, std::uint32_t height, FrameConfig config)
{
    if (!device_ && !prepare(host)) return false;
    config_ = config;
    if (config.hdr && !create_post_pipelines(host)) return false;
    if (width_ == width && height_ == height && depth() && (!config.hdr || hdr()) && (!config.bloom || bloom())) {
        return true;
    }

    resources_.destroy(hdr_);
    resources_.destroy(depth_);
    resources_.destroy(bloom_);
    hdr_ = depth_ = bloom_ = {};

    const auto color_usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    const auto depth_usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    depth_ = resources_.create_texture({depth_format_, depth_usage, width, height});
    if (!depth()) return fail("SDL_CreateGPUTexture (depth)");
    if (config.hdr) {
        hdr_ = resources_.create_texture({hdr_format_, color_usage, width, height});
        if (!hdr()) return fail("SDL_CreateGPUTexture (hdr)");
    }
    if (config.bloom) {
        bloom_ = resources_.create_texture(
            {hdr_format_, color_usage, std::max(1u, width / 2), std::max(1u, height / 2)});
        if (!bloom()) return fail("SDL_CreateGPUTexture (bloom)");
    }
    width_ = width;
    height_ = height;
    SDL_Log("Render targets: %ux%u pixels", width, height);
    return true;
}

bool Renderer::apply_bloom(rhi::Command& command, float threshold)
{
    if (!bloom() || !bloom_pipeline_ || !hdr() || !linear_clamp_) return false;
    SDL_GPUColorTargetInfo color{};
    color.texture = bloom();
    color.clear_color = {0, 0, 0, 1};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, nullptr);
    if (!pass) return false;
    SDL_BindGPUGraphicsPipeline(pass, bloom_pipeline_);
    const SDL_GPUTextureSamplerBinding binding{hdr(), linear_clamp_};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    const glm::vec4 params{threshold, 0.1f, 0.0f, 0.0f};
    SDL_PushGPUFragmentUniformData(command.handle, 0, &params, sizeof(params));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    return true;
}

bool Renderer::apply_tonemap(rhi::Command& command, SDL_GPUTexture* swapchain, float exposure, float bloom_strength)
{
    if (!tonemap_ || !hdr() || !linear_clamp_ || !swapchain) return false;
    SDL_GPUColorTargetInfo color{};
    color.texture = swapchain;
    color.clear_color = {0, 0, 0, 1};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, nullptr);
    if (!pass) return false;
    SDL_BindGPUGraphicsPipeline(pass, tonemap_);
    SDL_GPUTexture* bloom_tex = bloom() ? bloom() : hdr();
    const SDL_GPUTextureSamplerBinding bindings[2] = {
        {hdr(), linear_clamp_},
        {bloom_tex, linear_clamp_},
    };
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);
    const glm::vec4 exposure_bloom{exposure, bloom_strength, 0, 0};
    SDL_PushGPUFragmentUniformData(command.handle, 0, &exposure_bloom, sizeof(exposure_bloom));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    return true;
}

bool Renderer::post(rhi::Command& command, SDL_GPUTexture* swapchain, float exposure, float bloom_strength,
                    float bloom_threshold)
{
    core::ProfileScope scope(core::frame_profiler(), "post");
    FrameGraph graph;
    auto hdr_h = graph.import("hdr", hdr());
    auto swap_h = graph.import("swapchain", swapchain);
    FrameGraph::Handle bloom_h = FrameGraph::kInvalid;
    if (config_.bloom && bloom()) bloom_h = graph.import("bloom", bloom());
    if (config_.bloom && bloom_h) {
        graph.add_pass(
            "bloom", {hdr_h}, {bloom_h},
            [this, bloom_threshold](rhi::Command& cmd) { return apply_bloom(cmd, bloom_threshold); });
        graph.add_pass("tonemap", {hdr_h, bloom_h}, {swap_h},
                      [this, swapchain, exposure, bloom_strength](rhi::Command& cmd) {
                          return apply_tonemap(cmd, swapchain, exposure, bloom_strength);
                      });
    } else {
        graph.add_pass("tonemap", {hdr_h}, {swap_h}, [this, swapchain, exposure, bloom_strength](rhi::Command& cmd) {
            return apply_tonemap(cmd, swapchain, exposure, bloom_strength);
        });
    }
    return graph.execute(command);
}

void Renderer::destroy()
{
    if (device_) SDL_WaitForGPUIdle(device_);
    resources_.destroy_all();
    hdr_ = depth_ = bloom_ = {};
    if (device_ && linear_clamp_) SDL_ReleaseGPUSampler(device_, linear_clamp_);
    if (device_ && tonemap_) SDL_ReleaseGPUGraphicsPipeline(device_, tonemap_);
    if (device_ && bloom_pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, bloom_pipeline_);
    linear_clamp_ = nullptr;
    tonemap_ = bloom_pipeline_ = nullptr;
    device_ = nullptr;
    width_ = height_ = 0;
}

} // namespace forge::render
