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

glm::mat4 cube_at(const glm::vec3& center, const glm::vec3& size, float yaw_degrees = 0.0f)
{
    return glm::translate(glm::mat4(1.0f), center)
        * glm::rotate(glm::mat4(1.0f), glm::radians(yaw_degrees), glm::vec3{0, 1, 0})
        * glm::scale(glm::mat4(1.0f), size);
}

} // namespace

bool LuaGamemodeLab::setup(rhi::Host& host, Camera& camera)
{
    std::string error;
    if (!cube_.ingest(host.device(), assets::make_unit_cube(), "cube", error)) {
        SDL_Log("Cube ingest failed: %s", error.c_str());
        return false;
    }
    pbr_ = render::make_pbr_pipeline(host, false);
    if (!pbr_) return false;
    if (!physics_.init()) return false;
    physics_.add_box({0.0f, -0.5f, 0.0f}, {40.0f, 0.5f, 40.0f});
    physics_.add_box({10.0f, 0.5f, -6.0f}, {1.5f, 0.5f, 1.5f});

    if (!vm_.open(world_)) {
        SDL_Log("Lua VM failed: %s", vm_.last_error().c_str());
        return false;
    }
    const auto path = std::getenv("FORGE_GAMEMODE")
        ? std::filesystem::path(std::getenv("FORGE_GAMEMODE"))
        : forge::scripts_directory() / "main.lua";
    gamemode_name_ = path.filename().string();
    if (!vm_.run_file(path) || !vm_.call("on_init")) {
        SDL_Log("Gamemode failed: %s", vm_.last_error().c_str());
        return false;
    }
    if (world_.alive_players() == 0) world_.spawn_player({0.0f, 1.0f, 0.0f});
    character_ = world_.player(0)->position;
    physics_.spawn_character(character_, 0.28f, 0.45f);
    if (world_.alive_vehicles() > 0) {
        const auto* vehicle = world_.vehicle(0);
        has_car_ = physics_.spawn_vehicle(vehicle->position + glm::vec3{0.0f, 1.0f, 0.0f}, vehicle->yaw);
    }

    debug_.lab = "m6-lua";
    debug_.model = "gamemode";
    debug_.has_vehicle = has_car_;
    debug_.gamemode = gamemode_name_.c_str();
    debug_.help = "M6 Lua | WASD walk/drive | F enter/exit | Space jump/brake | RMB look";
    camera.position = character_ + glm::vec3{-5.0f, 2.4f, -6.0f};
    camera.look_at(character_);
    camera.speed = 12.0f;
    camera.near_plane = 0.08f;
    camera.far_plane = 160.0f;
    debug_.home_position = camera.position;
    debug_.home_yaw = camera.yaw;
    debug_.home_pitch = camera.pitch;
    sync_debug();
    SDL_Log("M6 gamemode %s: %u players, %u vehicles, %u markers, %u labels", gamemode_name_.c_str(),
            world_.alive_players(), world_.alive_vehicles(), world_.alive_markers(), world_.alive_labels());
    return true;
}

void LuaGamemodeLab::sync_debug()
{
    debug_.script_players = world_.alive_players();
    debug_.script_vehicles = world_.alive_vehicles();
    debug_.script_markers = world_.alive_markers();
    debug_.script_labels = world_.alive_labels();
    debug_.world_label_count = 0;
    for (const auto& label : world_.labels()) {
        if (!label.alive || debug_.world_label_count >= render::DebugState::kMaxWorldLabels) continue;
        const auto i = debug_.world_label_count++;
        debug_.world_label_pos[i] = label.position;
        debug_.world_label_text[i] = label.text.c_str();
    }
    debug_.driving = driving_;
    debug_.speed = driving_ && has_car_ ? physics_.vehicle_speed() : 0.0f;
    debug_.has_vehicle = has_car_;
    debug_.gamemode = gamemode_name_.c_str();
}

void LuaGamemodeLab::follow_camera(Camera& camera) const
{
    const glm::vec3 target = driving_ && has_car_ ? physics_.vehicle_position() : character_;
    glm::vec3 flat{camera.forward().x, 0.0f, camera.forward().z};
    if (glm::length(flat) < 1e-4f) flat = {0.0f, 0.0f, 1.0f};
    flat = glm::normalize(flat);
    camera.position = target - flat * (driving_ ? 8.5f : 5.0f) + glm::vec3{0.0f, driving_ ? 2.8f : 1.8f, 0.0f};
    camera.look_at(target + glm::vec3{0.0f, driving_ ? 0.8f : 0.7f, 0.0f});
}

void LuaGamemodeLab::update(float dt, Camera& camera, const app::LabInput& input)
{
    if (input.toggle_walk && has_car_ && world_.vehicle(0)) {
        const float distance = glm::length(physics_.vehicle_position() - character_);
        if (driving_) {
            driving_ = false;
            const auto side = physics_.vehicle_transform()[0];
            physics_.warp_character(physics_.vehicle_position() + glm::vec3{side} * 1.8f + glm::vec3{0, 0.8f, 0});
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
        const glm::vec3 right = glm::normalize(glm::cross(forward, {0, 1, 0}));
        walk = right * input.move.x + forward * input.move.z;
        if (input.boost) walk *= 1.7f;
        if (glm::length(glm::vec2(walk.x, walk.z)) > 0.05f)
            character_yaw_ = glm::degrees(std::atan2(walk.x, walk.z));
    }
    if (driving_ && input.captured)
        physics_.set_vehicle_input(input.move.z, input.move.x, input.jump ? 1.0f : 0.0f);
    else
        physics_.set_vehicle_input(0, 0, 0);

    accumulator_ += dt;
    const float step = 1.0f / 60.0f;
    while (accumulator_ >= step) {
        physics_.tick(step, walk, input.jump && !driving_);
        accumulator_ -= step;
    }
    character_ = physics_.character_position();
    if (auto* player = world_.player(0)) {
        player->position = driving_ && has_car_ ? physics_.vehicle_position() : character_;
        player->yaw = driving_ && has_car_ ? physics_.vehicle_yaw() : character_yaw_;
    }
    if (has_car_) {
        if (auto* vehicle = world_.vehicle(0)) {
            vehicle->position = physics_.vehicle_position();
            vehicle->yaw = physics_.vehicle_yaw();
        }
    }
    if (!vm_.call("on_update", dt)) SDL_Log("on_update: %s", vm_.last_error().c_str());
    if (driving_ && has_car_) character_yaw_ = physics_.vehicle_yaw();
    sync_debug();
    follow_camera(camera);
}

rhi::FrameResult LuaGamemodeLab::draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain,
                                      Uint32 width, Uint32 height, Camera& camera, bool)
{
    if (!host.resize(width, height, host_config())) return rhi::FrameResult::failed;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    render::CameraUniforms camera_ubo{};
    camera_ubo.view_projection = camera.projection(aspect) * camera.view();

    SDL_GPUColorTargetInfo color{};
    color.texture = host.hdr();
    color.clear_color = {0.18f, 0.32f, 0.48f, 1.0f};
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
    draw_cube(cube_at({0.0f, -0.5f, 0.0f}, {80.0f, 1.0f, 80.0f}), {0.27f, 0.35f, 0.24f, 1.0f});
    draw_cube(cube_at({10.0f, 0.5f, -6.0f}, {3.0f, 1.0f, 3.0f}), {0.42f, 0.40f, 0.38f, 1.0f});

    for (int id = 0; id < static_cast<int>(world_.players().size()); ++id) {
        const auto* player = world_.player(id);
        if (!player) continue;
        if (id == 0 && driving_) continue;
        const glm::vec4 tint = id == 0 ? glm::vec4{0.95f, 0.45f, 0.18f, 1.0f} : glm::vec4{0.25f, 0.62f, 0.85f, 1.0f};
        draw_cube(cube_at(player->position + glm::vec3{0.0f, 0.15f, 0.0f}, {0.55f, 1.7f, 0.55f}, player->yaw), tint);
    }
    for (int id = 0; id < static_cast<int>(world_.vehicles().size()); ++id) {
        const auto* vehicle = world_.vehicle(id);
        if (!vehicle) continue;
        if (id == 0 && has_car_) {
            const glm::mat4 chassis = physics_.vehicle_transform() * glm::translate(glm::mat4(1.0f), {0.0f, 0.22f, 0.0f})
                * glm::scale(glm::mat4(1.0f), {1.7f, 0.44f, 3.6f});
            draw_cube(chassis, {0.72f, 0.16f, 0.14f, 1.0f});
            const glm::vec4 rubber{0.08f, 0.08f, 0.09f, 1.0f};
            for (int w = 0; w < 4; ++w)
                draw_cube(physics_.wheel_transform(w) * glm::scale(glm::mat4(1.0f), {0.18f, 0.64f, 0.64f}), rubber);
            continue;
        }
        draw_cube(cube_at(vehicle->position + glm::vec3{0.0f, 0.35f, 0.0f}, {1.7f, 0.55f, 3.4f}, vehicle->yaw),
                  {0.55f, 0.18f, 0.16f, 1.0f});
    }
    for (const auto& marker : world_.markers()) {
        if (!marker.alive) continue;
        draw_cube(cube_at(marker.position + glm::vec3{0.0f, marker.size * 0.5f, 0.0f},
                          glm::vec3{marker.size, marker.size, marker.size}),
                  marker.color);
    }
    SDL_EndGPURenderPass(pass);
    render::apply_tonemap(host, command, swapchain, debug_.exposure, 0.0f);
    return rhi::FrameResult::presented;
}

void LuaGamemodeLab::teardown(rhi::Host& host)
{
    vm_.close();
    cube_.destroy(host.device());
    if (pbr_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_);
    pbr_ = nullptr;
}

} // namespace forge::labs
