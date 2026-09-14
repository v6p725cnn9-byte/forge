#include "lab.hpp"

#include "engine/core/paths.hpp"
#include "engine/render/pbr_pass.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace forge::labs {
namespace {

glm::mat4 cube_at(const glm::vec3& center, const glm::vec3& size)
{
    return glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), size);
}

} // namespace

bool SkinVehicleLab::setup(rhi::Host& host, Camera& camera)
{
    const auto fox_path = std::getenv("FORGE_MODEL")
        ? std::filesystem::path(std::getenv("FORGE_MODEL"))
        : forge::assets_directory() / "models/Fox/Fox.glb";
    std::string error;
    if (!fox_.load(host.device(), fox_path, error)) {
        SDL_Log("Fox load failed: %s", error.c_str());
        return false;
    }
    if (fox_.cpu().skins.empty() || fox_.cpu().animations.empty()) {
        SDL_Log("Fox is missing skins or animations");
        return false;
    }
    if (!cube_.ingest(host.device(), assets::make_unit_cube(), "cube", error)) {
        SDL_Log("Cube ingest failed: %s", error.c_str());
        return false;
    }
    pbr_ = render::make_pbr_pipeline(host, false);
    skinned_ = render::make_pbr_skinned_pipeline(host, true);
    if (!pbr_ || !skinned_) return false;
    if (!physics_.init()) return false;

    physics_.add_box({0.0f, -0.5f, 0.0f}, {40.0f, 0.5f, 40.0f});
    physics_.add_box({8.0f, 0.5f, 6.0f}, {1.2f, 0.5f, 1.2f});
    physics_.add_box({-6.0f, 0.75f, 10.0f}, {1.0f, 0.75f, 1.0f});
    physics_.add_box({12.0f, 0.35f, -4.0f}, {2.5f, 0.35f, 0.4f});
    physics_.spawn_character(character_, 0.18f, 0.28f);
    if (!physics_.spawn_vehicle({6.0f, 2.0f, 3.0f}, 200.0f)) return false;

    const float height = fox_.bounds_max.y - fox_.bounds_min.y;
    if (height > 1.0f) fox_scale_ = 0.95f / height;
    clip_walk_ = anim::find_clip(fox_.cpu(), "Walk");
    clip_run_ = anim::find_clip(fox_.cpu(), "Run");
    clip_idle_ = anim::find_clip(fox_.cpu(), "Survey");
    clip_ = clip_idle_ >= 0 ? clip_idle_ : 0;

    debug_.lab = "m5-skin";
    debug_.model = fox_.model_name.c_str();
    debug_.materials = fox_.material_count;
    debug_.textures = fox_.texture_count;
    debug_.has_vehicle = true;
    debug_.help = "M5 | WASD walk/drive | F enter/exit | Space jump/brake | RMB look";
    camera.position = {-4.0f, 2.2f, -6.0f};
    camera.look_at(character_);
    camera.speed = 12.0f;
    camera.near_plane = 0.08f;
    camera.far_plane = 120.0f;
    debug_.home_position = camera.position;
    debug_.home_yaw = camera.yaw;
    debug_.home_pitch = camera.pitch;
    SDL_Log("M5 fox: %u triangles, %zu joints, %zu clips, scale %.4f", fox_.triangle_count,
            fox_.cpu().skins.front().joints.size(), fox_.cpu().animations.size(), fox_scale_);
    return true;
}

void SkinVehicleLab::follow_camera(Camera& camera) const
{
    const glm::vec3 target = driving_ ? physics_.vehicle_position() : character_;
    glm::vec3 flat = glm::vec3{camera.forward().x, 0.0f, camera.forward().z};
    if (glm::length(flat) < 1e-4f) flat = {0.0f, 0.0f, 1.0f};
    flat = glm::normalize(flat);
    const float back = driving_ ? 8.5f : 4.2f;
    const float up = driving_ ? 2.8f : 1.6f;
    camera.position = target - flat * back + glm::vec3{0.0f, up, 0.0f};
    camera.look_at(target + glm::vec3{0.0f, driving_ ? 0.8f : 0.55f, 0.0f});
}

void SkinVehicleLab::update(float dt, Camera& camera, const app::LabInput& input)
{
    if (input.toggle_walk && physics_.has_vehicle()) {
        const float distance = glm::length(physics_.vehicle_position() - character_);
        if (driving_) {
            driving_ = false;
            const auto side = physics_.vehicle_transform()[0];
            physics_.warp_character(physics_.vehicle_position() + glm::vec3{side} * 1.8f + glm::vec3{0, 0.7f, 0});
            physics_.set_character_enabled(true);
            physics_.set_vehicle_input(0, 0, 0);
        } else if (distance < 4.0f) {
            driving_ = true;
            physics_.set_character_enabled(false);
        }
    }

    glm::vec3 walk{0};
    if (!driving_ && input.captured) {
        const glm::vec3 forward = glm::normalize(glm::vec3{camera.forward().x, 0.0f, camera.forward().z}
                                                 + glm::vec3{0.0001f, 0, 0});
        walk = app::compose_walk(forward, input.move.x, input.move.z);
        if (input.boost) walk *= 1.7f;
        if (glm::length(glm::vec2(walk.x, walk.z)) > 0.05f)
            character_yaw_ = glm::degrees(std::atan2(walk.x, walk.z));
    }

    if (driving_ && input.captured) {
        const float throttle = input.move.z;
        const float steer = input.move.x;
        physics_.set_vehicle_input(throttle, steer, input.jump ? 1.0f : 0.0f);
    } else {
        physics_.set_vehicle_input(0, 0, 0);
    }

    accumulator_ += dt;
    const float step = 1.0f / 60.0f;
    while (accumulator_ >= step) {
        physics_.tick(step, walk, input.jump && !driving_);
        accumulator_ -= step;
    }
    character_ = physics_.character_position();

    const float speed = driving_ ? physics_.vehicle_speed()
                                 : glm::length(glm::vec2(walk.x, walk.z)) * (input.boost ? 1.7f : 1.0f);
    if (driving_) {
        clip_ = clip_idle_;
        character_yaw_ = physics_.vehicle_yaw();
    } else if (speed > 0.15f) {
        clip_ = (input.boost && clip_run_ >= 0) ? clip_run_ : clip_walk_;
    } else {
        clip_ = clip_idle_;
    }
    if (clip_ < 0) clip_ = 0;
    anim_time_ += dt;
    anim::evaluate(fox_.cpu(), 0, clip_, anim_time_, palette_);

    debug_.driving = driving_;
    debug_.speed = driving_ ? physics_.vehicle_speed() : speed * 5.5f;
    debug_.clip = clip_ >= 0 && clip_ < static_cast<int>(fox_.cpu().animations.size())
        ? fox_.cpu().animations[static_cast<std::size_t>(clip_)].name.c_str()
        : "";
    follow_camera(camera);
}

rhi::FrameResult SkinVehicleLab::draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain,
                                      Uint32 width, Uint32 height, Camera& camera, bool)
{
    if (!host.resize(width, height, host_config())) return rhi::FrameResult::failed;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    render::CameraUniforms camera_ubo{};
    camera_ubo.view_projection = camera.projection(aspect) * camera.view();

    SDL_GPUColorTargetInfo color{};
    color.texture = host.hdr();
    color.clear_color = {0.22f, 0.38f, 0.52f, 1.0f};
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

    const auto draw_cube = [&](const glm::mat4& model, const glm::vec4& tint) {
        camera_ubo.model = model;
        SDL_PushGPUVertexUniformData(command.handle, 0, &camera_ubo, sizeof(camera_ubo));
        cube_.draw(command.handle, pass, pbr_, pbr_, debug_, camera.position, {}, tint);
    };
    draw_cube(cube_at({0.0f, -0.5f, 0.0f}, {80.0f, 1.0f, 80.0f}), {0.28f, 0.36f, 0.24f, 1.0f});
    draw_cube(cube_at({8.0f, 0.5f, 6.0f}, {2.4f, 1.0f, 2.4f}), {0.45f, 0.42f, 0.38f, 1.0f});
    draw_cube(cube_at({-6.0f, 0.75f, 10.0f}, {2.0f, 1.5f, 2.0f}), {0.40f, 0.38f, 0.44f, 1.0f});
    draw_cube(cube_at({12.0f, 0.35f, -4.0f}, {5.0f, 0.7f, 0.8f}), {0.50f, 0.36f, 0.28f, 1.0f});

    const glm::mat4 chassis = physics_.vehicle_transform() * glm::translate(glm::mat4(1.0f), {0.0f, 0.22f, 0.0f})
        * glm::scale(glm::mat4(1.0f), {1.7f, 0.44f, 3.6f});
    draw_cube(chassis, {0.72f, 0.16f, 0.14f, 1.0f});
    const glm::vec4 rubber{0.08f, 0.08f, 0.09f, 1.0f};
    for (int i = 0; i < 4; ++i)
        draw_cube(physics_.wheel_transform(i) * glm::scale(glm::mat4(1.0f), {0.18f, 0.64f, 0.64f}), rubber);

    const float foot = 0.18f + 0.28f;
    camera_ubo.model = glm::translate(glm::mat4(1.0f), character_ - glm::vec3{0.0f, foot, 0.0f})
        * glm::rotate(glm::mat4(1.0f), glm::radians(character_yaw_), glm::vec3{0, 1, 0})
        * glm::scale(glm::mat4(1.0f), glm::vec3{fox_scale_});
    SDL_PushGPUVertexUniformData(command.handle, 0, &camera_ubo, sizeof(camera_ubo));
    fox_.draw_skinned(command.handle, pass, skinned_, palette_, debug_, camera.position);
    SDL_EndGPURenderPass(pass);
    if (!render::apply_tonemap(host, command, swapchain, debug_.exposure, 0.0f)) return rhi::FrameResult::failed;
    return rhi::FrameResult::presented;
}

void SkinVehicleLab::teardown(rhi::Host& host)
{
    fox_.destroy(host.device());
    cube_.destroy(host.device());
    if (pbr_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_);
    if (skinned_) SDL_ReleaseGPUGraphicsPipeline(host.device(), skinned_);
    pbr_ = skinned_ = nullptr;
}

} // namespace forge::labs
