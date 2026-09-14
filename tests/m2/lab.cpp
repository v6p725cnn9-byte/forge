#include "lab.hpp"

#include "engine/core/paths/paths.hpp"
#include "engine/render/passes/opaque/pbr_pass.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <cstdlib>
#include <filesystem>

namespace forge::labs {

bool PbrHelmetLab::setup(rhi::Host& host, [[maybe_unused]] render::Renderer& renderer, Camera& camera)
{
    const auto path = std::getenv("FORGE_MODEL")
        ? std::filesystem::path(std::getenv("FORGE_MODEL"))
        : forge::assets_directory() / "models/FlightHelmet/FlightHelmet.gltf";
    std::string error;
    if (!scene_.load(host.device(), path, error)) {
        SDL_Log("Scene load failed: %s", error.c_str());
        return false;
    }
    pbr_cull_ = render::make_pbr_pipeline(host, renderer, false);
    pbr_double_ = render::make_pbr_pipeline(host, renderer, true);
    if (!pbr_cull_ || !pbr_double_) return false;
    debug_.model = scene_.model_name.c_str();
    debug_.materials = scene_.material_count;
    debug_.textures = scene_.texture_count;
    debug_.lab = "m2-pbr";
    debug_.help = "M2 lab | RMB+WASD fly | F1 overlay";
    camera.frame(scene_.bounds_min, scene_.bounds_max);
    SDL_Log("M2 PBR helmet: %u triangles", scene_.triangle_count);
    return true;
}

void PbrHelmetLab::update(float dt, Camera& camera, const app::LabInput& input)
{
    if (input.captured) camera.move(input.move, dt, input.boost);
}

rhi::FrameResult PbrHelmetLab::draw(rhi::Host& host, [[maybe_unused]] render::Renderer& renderer, rhi::Command& command, SDL_GPUTexture* swapchain,
                                     Uint32 width, Uint32 height, Camera& camera, bool)
{
    if (!renderer.ensure(host, width, height, frame_config())) return rhi::FrameResult::failed;
    render::CameraUniforms camera_ubo{};
    camera_ubo.view_projection =
        camera.projection(static_cast<float>(width) / static_cast<float>(height)) * camera.view();
    SDL_PushGPUVertexUniformData(command.handle, 0, &camera_ubo, sizeof(camera_ubo));

    SDL_GPUColorTargetInfo color{};
    color.texture = renderer.hdr();
    color.clear_color = {0.02f, 0.025f, 0.035f, 1.0f};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = renderer.depth();
    depth.clear_depth = 1.0f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.cycle = true;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, &depth);
    if (!pass) return rhi::FrameResult::failed;
    scene_.draw(command.handle, pass, pbr_cull_, pbr_double_, debug_, camera.position);
    SDL_EndGPURenderPass(pass);
    if (!renderer.apply_tonemap(command, swapchain, debug_.exposure, 0.0f)) return rhi::FrameResult::failed;
    return rhi::FrameResult::presented;
}

void PbrHelmetLab::teardown(rhi::Host& host, [[maybe_unused]] render::Renderer& renderer)
{
    scene_.destroy(host.device());
    if (pbr_cull_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_cull_);
    if (pbr_double_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_double_);
    pbr_cull_ = pbr_double_ = nullptr;
}

} // namespace forge::labs
