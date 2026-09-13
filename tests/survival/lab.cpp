#include "lab.hpp"

#include "engine/render/pbr_pass.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace forge::labs {
namespace {

glm::mat4 cube_at(const glm::vec3& center, const glm::vec3& size, float yaw_degrees = 0.0f)
{
    return glm::translate(glm::mat4(1.0f), center)
        * glm::rotate(glm::mat4(1.0f), glm::radians(yaw_degrees), glm::vec3{0, 1, 0})
        * glm::scale(glm::mat4(1.0f), size);
}

unsigned env_port()
{
    unsigned port = 27015;
    if (const char* env = std::getenv("FORGE_PORT")) {
        port = static_cast<unsigned>(std::strtoul(env, nullptr, 10));
        if (port == 0 || port > 65535) port = 27015;
    }
    return port;
}

} // namespace

const net::Ghost* SurvivalLab::self() const
{
    for (const auto& ghost : client_.snapshot().entities) {
        if (ghost.kind == net::Kind::Player && ghost.id == client_.player_id()) return &ghost;
    }
    return nullptr;
}

bool SurvivalLab::setup(rhi::Host& host, Camera& camera)
{
    std::string error;
    if (!cube_.ingest(host.device(), assets::make_unit_cube(), "cube", error)) {
        SDL_Log("Cube ingest failed: %s", error.c_str());
        return false;
    }
    pbr_ = render::make_pbr_pipeline(host, false);
    if (!pbr_) return false;

    unsigned port = env_port();
    net::Address connect{};
    hosting_ = true;
    bool lan = std::getenv("FORGE_OFFLINE") == nullptr;
    if (configured_) {
        port = static_cast<unsigned>(std::clamp(launch_.port, 1, 65535));
        hosting_ = launch_.hosting;
        lan = launch_.lan;
        if (!launch_.connect.empty()) {
            hosting_ = false;
            if (!net::parse_address(launch_.connect, connect)) {
                SDL_Log("Join address must be ip:port, got %s", launch_.connect.c_str());
                return false;
            }
        }
    } else if (const char* env = std::getenv("FORGE_CONNECT")) {
        if (!net::parse_address(env, connect)) {
            SDL_Log("FORGE_CONNECT must be ip:port, got %s", env);
            return false;
        }
        hosting_ = false;
    }

    if (hosting_) {
        sim_.reset();
        if (!server_.listen(static_cast<std::uint16_t>(port), lan)) {
            if (!server_.listen(0, lan)) {
                SDL_Log("UDP bind failed");
                return false;
            }
        }
        server_.attach(world_);
        server_.attach_sim(sim_);
        server_.set_stream_radius(70.0f);
        connect = net::localhost(server_.port());
        if (lan) {
            join_line_ = "FORGE_CONNECT=";
            const auto ips = net::ipv4_addresses();
            join_line_ += ips.empty() ? "127.0.0.1" : ips.front();
            join_line_ += ":" + std::to_string(server_.port());
        } else {
            join_line_ = "solo";
        }
    }

    if (!client_.connect(connect)) {
        SDL_Log("UDP connect failed");
        return false;
    }
    for (int i = 0; i < 50 && !client_.connected(); ++i) {
        client_.send_input(0, 0, yaw_, false);
        if (hosting_) server_.update(0.05f);
        client_.poll();
    }
    if (!client_.connected()) {
        SDL_Log("UDP handshake failed");
        return false;
    }

    debug_.lab = "survival";
    debug_.model = "session";
    debug_.survival = true;
    debug_.stream_radius = 70.0f;
    debug_.net_role = hosting_ ? "host" : "client";
    debug_.help = "WASD walk | E chop/extract | C campfire | Shift run | RMB look";
    std::snprintf(debug_.join_hint, sizeof(debug_.join_hint), "%s", join_line_.c_str());
    camera.position = {-8.0f, 4.0f, -6.0f};
    camera.look_at({0, 1, 4});
    camera.speed = 12.0f;
    camera.near_plane = 0.08f;
    camera.far_plane = 220.0f;
    debug_.home_position = camera.position;
    debug_.home_yaw = camera.yaw;
    debug_.home_pitch = camera.pitch;
    sync_debug();
    SDL_Log("Survival %s player %u  %s", debug_.net_role, client_.player_id(), debug_.join_hint);
    return true;
}

void SurvivalLab::sync_debug()
{
    const auto& snap = client_.snapshot();
    debug_.survival = true;
    debug_.hp = snap.hp;
    debug_.cold = snap.cold;
    debug_.wood = snap.wood;
    debug_.stone = snap.stone;
    debug_.time_left = snap.time_left;
    debug_.phase = snap.phase;
    debug_.night = snap.night != 0;
    debug_.net_tick = snap.tick;
    debug_.net_streamed = static_cast<std::uint32_t>(snap.entities.size());
    debug_.net_peers = hosting_ ? server_.peers() : 1;
    debug_.net_ping_ms = client_.ping_ms();
    debug_.net_role = hosting_ ? "host" : "client";
    debug_.world_label_count = 0;
    auto push_label = [&](glm::vec3 pos, const char* text) {
        if (!text || !text[0] || debug_.world_label_count >= render::DebugState::kMaxWorldLabels) return;
        const auto i = debug_.world_label_count++;
        debug_.world_label_pos[i] = pos;
        debug_.world_label_text[i] = text;
    };
    for (const auto& ghost : snap.entities) {
        if (ghost.kind == net::Kind::Player) push_label(ghost.position + glm::vec3{0, 1.5f, 0}, ghost.name.c_str());
        if (ghost.kind == net::Kind::Extract) push_label(ghost.position + glm::vec3{0, 3.2f, 0}, "EXTRACT");
        if (ghost.kind == net::Kind::Campfire) push_label(ghost.position + glm::vec3{0, 1.2f, 0}, "fire");
    }
    std::snprintf(debug_.join_hint, sizeof(debug_.join_hint), "%s", join_line_.c_str());
}

void SurvivalLab::follow_camera(Camera& camera) const
{
    const auto* player = self();
    if (!player) return;
    glm::vec3 flat{camera.forward().x, 0.0f, camera.forward().z};
    if (glm::length(flat) < 1e-4f) flat = {0.0f, 0.0f, 1.0f};
    flat = glm::normalize(flat);
    camera.position = player->position - flat * 7.0f + glm::vec3{0.0f, 2.6f, 0.0f};
    camera.look_at(player->position + glm::vec3{0.0f, 0.8f, 0.0f});
}

void SurvivalLab::update(float dt, Camera& camera, const app::LabInput& input)
{
    glm::vec3 walk{0};
    if (input.captured) {
        const glm::vec3 forward = glm::normalize(glm::vec3{camera.forward().x, 0.0f, camera.forward().z}
                                                 + glm::vec3{0.0001f, 0, 0});
        const glm::vec3 right = glm::normalize(glm::cross(forward, {0, 1, 0}));
        walk = right * input.move.x + forward * input.move.z;
        if (glm::length(glm::vec2(walk.x, walk.z)) > 0.05f)
            yaw_ = glm::degrees(std::atan2(walk.x, walk.z));
    }
    client_.send_input(walk.x, walk.z, yaw_, input.boost, input.interact, input.place);
    if (hosting_) server_.update(dt);
    client_.poll();
    sync_debug();
    follow_camera(camera);
}

rhi::FrameResult SurvivalLab::draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, Uint32 width,
                                   Uint32 height, Camera& camera, bool)
{
    if (!host.resize(width, height, host_config())) return rhi::FrameResult::failed;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    render::CameraUniforms camera_ubo{};
    camera_ubo.view_projection = camera.projection(aspect) * camera.view();

    const bool night = client_.snapshot().night != 0;
    SDL_GPUColorTargetInfo color{};
    color.texture = host.hdr();
    color.clear_color = night ? SDL_FColor{0.02f, 0.03f, 0.06f, 1.0f} : SDL_FColor{0.22f, 0.40f, 0.52f, 1.0f};
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
    draw_cube(cube_at({0.0f, -0.5f, 40.0f}, {180.0f, 1.0f, 200.0f}), {0.22f, 0.32f, 0.18f, 1.0f});

    for (const auto& ghost : client_.snapshot().entities) {
        if (ghost.kind == net::Kind::Player) {
            const glm::vec4 tint = ghost.id == client_.player_id() ? glm::vec4{0.95f, 0.48f, 0.18f, 1.0f}
                                                                   : glm::vec4{0.35f, 0.72f, 0.40f, 1.0f};
            draw_cube(cube_at(ghost.position + glm::vec3{0.0f, 0.15f, 0.0f}, {0.55f, 1.7f, 0.55f}, ghost.yaw), tint);
        } else if (ghost.kind == net::Kind::Tree) {
            draw_cube(cube_at(ghost.position + glm::vec3{0, 1.1f, 0}, {0.45f, 2.2f, 0.45f}), {0.32f, 0.18f, 0.08f, 1});
            draw_cube(cube_at(ghost.position + glm::vec3{0, 2.6f, 0}, {2.2f, 1.8f, 2.2f}), {0.12f, 0.32f, 0.12f, 1});
        } else if (ghost.kind == net::Kind::Rock) {
            draw_cube(cube_at(ghost.position + glm::vec3{0, 0.35f, 0}, {1.3f, 0.7f, 1.1f}), {0.38f, 0.36f, 0.34f, 1});
        } else if (ghost.kind == net::Kind::Campfire) {
            draw_cube(cube_at(ghost.position + glm::vec3{0, 0.25f, 0}, {0.8f, 0.5f, 0.8f}), {0.95f, 0.35f, 0.08f, 1});
        } else if (ghost.kind == net::Kind::Extract) {
            draw_cube(cube_at(ghost.position + glm::vec3{0, 1.6f, 0}, {1.2f, 3.2f, 1.2f}), {0.25f, 0.75f, 0.95f, 1});
        }
    }
    SDL_EndGPURenderPass(pass);
    render::apply_tonemap(host, command, swapchain, night ? 0.7f : debug_.exposure, 0.0f);
    return rhi::FrameResult::presented;
}

void SurvivalLab::teardown(rhi::Host& host)
{
    client_.close();
    if (hosting_) server_.close();
    cube_.destroy(host.device());
    if (pbr_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_);
    pbr_ = nullptr;
}

} // namespace forge::labs
