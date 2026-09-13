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

const net::Ghost* NetLab::self() const
{
    for (const auto& ghost : client_.snapshot().entities) {
        if (ghost.kind == net::Kind::Player && ghost.id == client_.player_id()) return &ghost;
    }
    return nullptr;
}

bool NetLab::setup(rhi::Host& host, Camera& camera)
{
    std::string error;
    if (!cube_.ingest(host.device(), assets::make_unit_cube(), "cube", error)) {
        SDL_Log("Cube ingest failed: %s", error.c_str());
        return false;
    }
    pbr_ = render::make_pbr_pipeline(host, false);
    if (!pbr_) return false;
    if (!vm_.open(world_)) {
        SDL_Log("Lua VM failed: %s", vm_.last_error().c_str());
        return false;
    }
    const auto path = std::getenv("FORGE_GAMEMODE")
        ? std::filesystem::path(std::getenv("FORGE_GAMEMODE"))
        : forge::scripts_directory() / "net.lua";
    gamemode_name_ = path.filename().string();
    if (!vm_.run_file(path) || !vm_.call("on_init")) {
        SDL_Log("Gamemode failed: %s", vm_.last_error().c_str());
        return false;
    }
    unsigned port = 27015;
    if (const char* env = std::getenv("FORGE_PORT")) {
        port = static_cast<unsigned>(std::strtoul(env, nullptr, 10));
        if (port == 0 || port > 65535) port = 27015;
    }
    if (!server_.listen(static_cast<std::uint16_t>(port))) {
        if (!server_.listen(0)) {
            SDL_Log("UDP bind failed");
            return false;
        }
    }
    server_.attach(world_);
    debug_.stream_radius = net::kDefaultStreamRadius;
    server_.set_stream_radius(debug_.stream_radius);
    if (!client_.connect(net::localhost(server_.port()))) {
        SDL_Log("UDP connect failed");
        return false;
    }
    for (int i = 0; i < 40 && !client_.connected(); ++i) {
        client_.send_input(0, 0, yaw_, false);
        server_.update(0.05f);
        client_.poll();
    }
    if (!client_.connected()) {
        SDL_Log("UDP handshake failed on port %u", server_.port());
        return false;
    }

    debug_.lab = "m7-net";
    debug_.model = "udp";
    debug_.gamemode = gamemode_name_.c_str();
    debug_.net_role = "listen";
    debug_.help = "M7 UDP | WASD walk | Shift boost | RMB look | far bots stream in/out";
    camera.position = {-6.0f, 3.0f, -8.0f};
    camera.look_at({0, 1, 0});
    camera.speed = 14.0f;
    camera.near_plane = 0.08f;
    camera.far_plane = 200.0f;
    debug_.home_position = camera.position;
    debug_.home_yaw = camera.yaw;
    debug_.home_pitch = camera.pitch;
    sync_debug();
    SDL_Log("M7 listen-server :%u  player %u  stream %.0f m  lua %s", server_.port(), client_.player_id(),
            debug_.stream_radius, gamemode_name_.c_str());
    return true;
}

void NetLab::sync_debug()
{
    debug_.net_tick = client_.tick();
    debug_.net_streamed = static_cast<std::uint32_t>(client_.snapshot().entities.size());
    debug_.net_peers = server_.peers();
    debug_.net_ping_ms = client_.ping_ms();
    debug_.script_players = world_.alive_players();
    debug_.script_vehicles = world_.alive_vehicles();
    debug_.script_markers = world_.alive_markers();
    debug_.script_labels = world_.alive_labels();
    debug_.gamemode = gamemode_name_.c_str();
    debug_.net_role = "listen";
    debug_.world_label_count = 0;
    for (const auto& ghost : client_.snapshot().entities) {
        if (ghost.kind != net::Kind::Label && ghost.kind != net::Kind::Player) continue;
        if (debug_.world_label_count >= render::DebugState::kMaxWorldLabels) break;
        const auto i = debug_.world_label_count++;
        debug_.world_label_pos[i] = ghost.position + glm::vec3{0.0f, ghost.kind == net::Kind::Player ? 1.4f : 0.0f, 0.0f};
        debug_.world_label_text[i] =
            ghost.kind == net::Kind::Label ? ghost.text.c_str() : ghost.name.c_str();
    }
}

void NetLab::follow_camera(Camera& camera) const
{
    const auto* player = self();
    if (!player) return;
    glm::vec3 flat{camera.forward().x, 0.0f, camera.forward().z};
    if (glm::length(flat) < 1e-4f) flat = {0.0f, 0.0f, 1.0f};
    flat = glm::normalize(flat);
    camera.position = player->position - flat * 6.0f + glm::vec3{0.0f, 2.2f, 0.0f};
    camera.look_at(player->position + glm::vec3{0.0f, 0.7f, 0.0f});
}

void NetLab::update(float dt, Camera& camera, const app::LabInput& input)
{
    server_.set_stream_radius(debug_.stream_radius > 1.0f ? debug_.stream_radius : net::kDefaultStreamRadius);
    glm::vec3 walk{0};
    if (input.captured) {
        const glm::vec3 forward = glm::normalize(glm::vec3{camera.forward().x, 0.0f, camera.forward().z}
                                                 + glm::vec3{0.0001f, 0, 0});
        const glm::vec3 right = glm::normalize(glm::cross(forward, {0, 1, 0}));
        walk = right * input.move.x + forward * input.move.z;
        if (glm::length(glm::vec2(walk.x, walk.z)) > 0.05f)
            yaw_ = glm::degrees(std::atan2(walk.x, walk.z));
    }
    client_.send_input(walk.x, walk.z, yaw_, input.boost);
    server_.update(dt);
    client_.poll();
    vm_.call("on_update", dt);
    sync_debug();
    follow_camera(camera);
}

rhi::FrameResult NetLab::draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                              Uint32 height, Camera& camera, bool)
{
    if (!host.resize(width, height, host_config())) return rhi::FrameResult::failed;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    render::CameraUniforms camera_ubo{};
    camera_ubo.view_projection = camera.projection(aspect) * camera.view();

    SDL_GPUColorTargetInfo color{};
    color.texture = host.hdr();
    color.clear_color = {0.16f, 0.28f, 0.42f, 1.0f};
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
    draw_cube(cube_at({0.0f, -0.5f, 0.0f}, {160.0f, 1.0f, 160.0f}), {0.26f, 0.34f, 0.24f, 1.0f});

    for (const auto& ghost : client_.snapshot().entities) {
        if (ghost.kind == net::Kind::Player) {
            const glm::vec4 tint = ghost.id == client_.player_id() ? glm::vec4{0.95f, 0.45f, 0.18f, 1.0f}
                                                                   : glm::vec4{0.25f, 0.62f, 0.85f, 1.0f};
            draw_cube(cube_at(ghost.position + glm::vec3{0.0f, 0.15f, 0.0f}, {0.55f, 1.7f, 0.55f}, ghost.yaw), tint);
        } else if (ghost.kind == net::Kind::Vehicle) {
            draw_cube(cube_at(ghost.position + glm::vec3{0.0f, 0.2f, 0.0f}, {1.7f, 0.55f, 3.4f}, ghost.yaw),
                      {0.62f, 0.16f, 0.14f, 1.0f});
        } else if (ghost.kind == net::Kind::Marker) {
            draw_cube(cube_at(ghost.position + glm::vec3{0.0f, ghost.size * 0.5f, 0.0f}, glm::vec3{ghost.size}),
                      ghost.color);
        }
    }
    SDL_EndGPURenderPass(pass);
    render::apply_tonemap(host, command, swapchain, debug_.exposure, 0.0f);
    return rhi::FrameResult::presented;
}

void NetLab::teardown(rhi::Host& host)
{
    client_.close();
    server_.close();
    vm_.close();
    cube_.destroy(host.device());
    if (pbr_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_);
    pbr_ = nullptr;
}

} // namespace forge::labs
