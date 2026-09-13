#include "lab.hpp"

#include "engine/core/paths.hpp"
#include "engine/render/pbr_pass.hpp"
#include "engine/rhi/texture.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace forge::labs {
namespace {

constexpr int kCascades = 3;
constexpr int kShadowSize = 1024;

std::array<glm::vec3, 8> cascade_corners(float fov, float aspect, float near_plane, float far_plane)
{
    const float tan_half = std::tan(glm::radians(fov) * 0.5f);
    const float near_h = tan_half * near_plane;
    const float near_w = near_h * aspect;
    const float far_h = tan_half * far_plane;
    const float far_w = far_h * aspect;
    return {{
        {-near_w, -near_h, -near_plane}, {near_w, -near_h, -near_plane},
        {-near_w, near_h, -near_plane}, {near_w, near_h, -near_plane},
        {-far_w, -far_h, -far_plane}, {far_w, -far_h, -far_plane},
        {-far_w, far_h, -far_plane}, {far_w, far_h, -far_plane},
    }};
}

} // namespace

bool ShadowWalkLab::create_shadow_map(rhi::Host& host)
{
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    info.format = host.depth_format();
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = kShadowSize;
    info.height = kShadowSize;
    info.layer_count_or_depth = kCascades;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    shadow_map_ = SDL_CreateGPUTexture(host.device(), &info);
    assets::TextureRef clamp;
    clamp.min_filter = 9729;
    clamp.mag_filter = 9729;
    clamp.wrap_s = 33071;
    clamp.wrap_t = 33071;
    shadow_sampler_ = rhi::create_sampler(host.device(), clamp, true);
    return shadow_map_ && shadow_sampler_;
}

bool ShadowWalkLab::setup(rhi::Host& host, Camera& camera)
{
    const auto path = std::getenv("FORGE_MODEL")
        ? std::filesystem::path(std::getenv("FORGE_MODEL"))
        : forge::assets_directory() / "models/FlightHelmet/FlightHelmet.gltf";
    std::string error;
    if (!scene_.load(host.device(), path, error)) {
        SDL_Log("Scene load failed: %s", error.c_str());
        return false;
    }
    pbr_cull_ = render::make_pbr_pipeline(host, false);
    pbr_double_ = render::make_pbr_pipeline(host, true);
    shadow_pipeline_ = render::make_shadow_pipeline(host);
    if (!pbr_cull_ || !pbr_double_ || !shadow_pipeline_ || !create_shadow_map(host)) return false;
    if (!physics_.init()) return false;

    const glm::vec3 extent = (scene_.bounds_max - scene_.bounds_min) * 0.5f;
    const glm::vec3 center = (scene_.bounds_max + scene_.bounds_min) * 0.5f;
    const float ground_y = scene_.bounds_min.y;
    physics_.add_box({center.x, ground_y - 0.5f, center.z}, {40.0f, 0.5f, 40.0f});
    physics_.add_box(center, glm::max(extent, glm::vec3{0.05f}));
    physics_.add_box({center.x + 2.2f, ground_y + 0.4f, center.z + 1.4f}, {0.4f, 0.4f, 0.4f});
    physics_.add_box({center.x - 1.8f, ground_y + 0.6f, center.z + 2.0f}, {0.35f, 0.6f, 0.35f});
    character_ = {center.x + 1.6f, ground_y + 1.4f, center.z + 2.2f};
    physics_.spawn_character(character_, 0.32f, 0.55f);

    debug_.bloom = 0.18f;
    debug_.walk_mode = true;
    debug_.shadow_strength = 1.0f;
    debug_.lab = "m3-shadows";
    debug_.model = scene_.model_name.c_str();
    debug_.materials = scene_.material_count;
    debug_.textures = scene_.texture_count;
    debug_.help = "M3 | F walk/fly | Space jump | RMB look | WASD";
    camera.frame(scene_.bounds_min, scene_.bounds_max);
    shadows_.enabled = true;
    shadows_.map = shadow_map_;
    shadows_.sampler = shadow_sampler_;
    shadows_.texel = 1.0f / static_cast<float>(kShadowSize);
    SDL_Log("M3 shadow walk: %u triangles", scene_.triangle_count);
    return true;
}

void ShadowWalkLab::compute_cascades(const Camera& camera, float aspect)
{
    const glm::vec3 light = render::sun_direction(debug_.light_azimuth, debug_.light_elevation);
    const float near_plane = camera.near_plane;
    const float far_plane = std::min(camera.far_plane, 48.0f);
    const float lambda = 0.7f;
    float distances[4] = {near_plane, 0, 0, far_plane};
    for (int i = 1; i < kCascades; ++i) {
        const float p = static_cast<float>(i) / static_cast<float>(kCascades);
        const float log_split = near_plane * std::pow(far_plane / near_plane, p);
        const float uni_split = glm::mix(near_plane, far_plane, p);
        distances[i] = glm::mix(uni_split, log_split, lambda);
    }
    shadows_.splits = {distances[1], distances[2], distances[3], 0.0f};
    const glm::mat4 inv_view = glm::inverse(camera.view());
    for (int cascade = 0; cascade < kCascades; ++cascade) {
        auto corners = cascade_corners(camera.vertical_fov, aspect, distances[cascade], distances[cascade + 1]);
        glm::vec3 center{0};
        for (auto& corner : corners) {
            corner = glm::vec3(inv_view * glm::vec4(corner, 1.0f));
            center += corner;
        }
        center /= 8.0f;
        const glm::mat4 light_view = glm::lookAtRH(center + light * 30.0f, center, {0.0f, 1.0f, 0.0f});
        glm::vec3 mn{1.0e9f}, mx{-1.0e9f};
        for (const auto& corner : corners) {
            const glm::vec3 light_space = glm::vec3(light_view * glm::vec4(corner, 1.0f));
            mn = glm::min(mn, light_space);
            mx = glm::max(mx, light_space);
        }
        mn -= glm::vec3{2.0f};
        mx += glm::vec3{2.0f};
        const glm::mat4 projection = glm::orthoRH_ZO(mn.x, mx.x, mn.y, mx.y, -mx.z, -mn.z);
        shadows_.light_vp[cascade] = projection * light_view;
    }
    shadows_.strength = debug_.shadow_strength;
}

void ShadowWalkLab::update(float dt, Camera& camera, const app::LabInput& input)
{
    if (input.toggle_walk) debug_.walk_mode = !debug_.walk_mode;
    accumulator_ += dt;
    const float step = 1.0f / 60.0f;
    glm::vec3 walk{0};
    if (debug_.walk_mode && input.captured) {
        const glm::vec3 forward = glm::normalize(glm::vec3{camera.forward().x, 0.0f, camera.forward().z});
        const glm::vec3 right = glm::normalize(glm::cross(forward, {0, 1, 0}));
        walk = right * input.move.x + forward * input.move.z;
        if (input.boost) walk *= 1.8f;
    } else if (input.captured) {
        camera.move(input.move, dt, input.boost);
    }
    while (accumulator_ >= step) {
        physics_.tick(step, walk, input.jump && debug_.walk_mode);
        accumulator_ -= step;
    }
    character_ = physics_.character_position();
    if (debug_.walk_mode) {
        const glm::vec3 back = -glm::normalize(glm::vec3{camera.forward().x, 0.0f, camera.forward().z} + glm::vec3{0.0001f});
        camera.position = character_ + glm::vec3{0, 1.35f, 0} + back * 3.2f + glm::vec3{0, 0.55f, 0};
        camera.look_at(character_ + glm::vec3{0, 0.9f, 0});
        camera.near_plane = 0.08f;
        camera.far_plane = 80.0f;
    }
}

rhi::FrameResult ShadowWalkLab::draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain,
                                     Uint32 width, Uint32 height, Camera& camera, bool)
{
    if (!host.resize(width, height, host_config())) return rhi::FrameResult::failed;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    compute_cascades(camera, aspect);

    for (int cascade = 0; cascade < kCascades; ++cascade) {
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = shadow_map_;
        depth.clear_depth = 1.0f;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_STORE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.cycle = cascade == 0;
        depth.layer = static_cast<Uint8>(cascade);
        auto* pass = SDL_BeginGPURenderPass(command.handle, nullptr, 0, &depth);
        if (!pass) return rhi::FrameResult::failed;
        SDL_PushGPUVertexUniformData(command.handle, 0, glm::value_ptr(shadows_.light_vp[cascade]), sizeof(glm::mat4));
        scene_.draw_depth(pass, shadow_pipeline_);
        SDL_EndGPURenderPass(pass);
    }

    render::CameraUniforms camera_ubo{};
    camera_ubo.view_projection = camera.projection(aspect) * camera.view();
    SDL_PushGPUVertexUniformData(command.handle, 0, &camera_ubo, sizeof(camera_ubo));

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
    if (!pass) return rhi::FrameResult::failed;
    scene_.draw(command.handle, pass, pbr_cull_, pbr_double_, debug_, camera.position, shadows_);
    SDL_EndGPURenderPass(pass);

    if (debug_.bloom > 0.001f) render::apply_bloom(host, command, 1.0f);
    render::apply_tonemap(host, command, swapchain, debug_.exposure, debug_.bloom);
    return rhi::FrameResult::presented;
}

void ShadowWalkLab::teardown(rhi::Host& host)
{
    scene_.destroy(host.device());
    if (shadow_pipeline_) SDL_ReleaseGPUGraphicsPipeline(host.device(), shadow_pipeline_);
    if (pbr_cull_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_cull_);
    if (pbr_double_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_double_);
    if (shadow_sampler_) SDL_ReleaseGPUSampler(host.device(), shadow_sampler_);
    if (shadow_map_) SDL_ReleaseGPUTexture(host.device(), shadow_map_);
    shadow_pipeline_ = pbr_cull_ = pbr_double_ = nullptr;
    shadow_sampler_ = nullptr;
    shadow_map_ = nullptr;
}

} // namespace forge::labs
