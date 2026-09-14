#include "engine/app/menu_scene.hpp"

#include "engine/core/paths.hpp"
#include "engine/render/pbr_pass.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

namespace forge::app {
namespace {

// One slow revolution, cinematic pace.
constexpr float kOrbitSpeed = 0.10f;
constexpr float kOrbitRadius = 14.0f;
constexpr float kOrbitHeight = 6.0f;
const glm::vec3 kCamp{0.0f, 2.0f, 0.0f};
// Low warm sun over the lake (+X).
constexpr float kSunAzimuth = 8.0f;
constexpr float kSunElevation = 9.0f;

} // namespace

bool MenuScene::create(rhi::Host& host)
{
    destroy(host);
    if (!host.device()) return false;
    std::string error;
    const glm::vec3 sun = render::sun_direction(kSunAzimuth, kSunElevation);
    if (!vista_.ingest(host.device(), assets::make_vista_terrain(sun), "menu_vista", error)) {
        SDL_Log("Menu vista failed: %s", error.c_str());
        return false;
    }
    const auto helmet_path = forge::assets_directory() / "models/FlightHelmet/FlightHelmet.gltf";
    if (!helmet_.load(host.device(), helmet_path, error)) {
        SDL_Log("Menu scene load failed: %s", error.c_str());
        return false;
    }
    pbr_cull_ = render::make_pbr_pipeline(host, false);
    pbr_double_ = render::make_pbr_pipeline(host, true);
    if (!pbr_cull_ || !pbr_double_) {
        destroy(host);
        return false;
    }
    // Sit the helmet prop on the camp knoll (knoll top is y=1.5).
    helmet_lift_ = 1.5f - helmet_.bounds_min.y;
    debug_.model = "menu_vista";
    debug_.lab = "menu-scene";
    debug_.light_azimuth = kSunAzimuth;
    debug_.light_elevation = kSunElevation;
    debug_.light_color = {1.0f, 0.55f, 0.30f};
    debug_.light_intensity = 3.5f;
    debug_.fog_color = {0.72f, 0.48f, 0.32f};
    debug_.fog_density = 0.0016f;
    camera_.vertical_fov = 42.0f;
    SDL_Log("Menu scene: vista (%u triangles) + helmet (%u triangles)", vista_.triangle_count,
            helmet_.triangle_count);
    return true;
}

void MenuScene::destroy(rhi::Host& host)
{
    if (host.device()) {
        vista_.destroy(host.device());
        helmet_.destroy(host.device());
        if (pbr_cull_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_cull_);
        if (pbr_double_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_double_);
    }
    pbr_cull_ = pbr_double_ = nullptr;
}

bool MenuScene::draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width, Uint32 height,
                     float now_seconds)
{
    if (!ready() || !swapchain || width == 0 || height == 0) return false;
    if (!host.resize(width, height, {.hdr = true, .bloom = false})) return false;
    const float angle = now_seconds * kOrbitSpeed;
    camera_.position = kCamp + glm::vec3(std::cos(angle) * kOrbitRadius, kOrbitHeight - kCamp.y,
                                         std::sin(angle) * kOrbitRadius);
    camera_.look_at(kCamp + glm::vec3{0.0f, 1.5f, 0.0f});

    SDL_GPUColorTargetInfo color{};
    color.texture = host.hdr();
    color.clear_color = {0.02f, 0.025f, 0.035f, 1.0f};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = host.depth();
    depth.clear_depth = 1.0f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.cycle = true;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, &depth);
    if (!pass) return false;
    render::CameraUniforms camera_ubo{};
    camera_ubo.view_projection =
        camera_.projection(static_cast<float>(width) / static_cast<float>(height)) * camera_.view();
    SDL_PushGPUVertexUniformData(command.handle, 0, &camera_ubo, sizeof(camera_ubo));
    vista_.draw(command.handle, pass, pbr_cull_, pbr_double_, debug_, camera_.position);
    camera_ubo.model = glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, helmet_lift_, 0.0f});
    SDL_PushGPUVertexUniformData(command.handle, 0, &camera_ubo, sizeof(camera_ubo));
    helmet_.draw(command.handle, pass, pbr_cull_, pbr_double_, debug_, camera_.position);
    SDL_EndGPURenderPass(pass);
    return render::apply_tonemap(host, command, swapchain, debug_.exposure, 0.0f);
}

} // namespace forge::app
