#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>

namespace forge::render {

inline glm::vec3 sun_direction(float azimuth_deg, float elevation_deg)
{
    const float azimuth = glm::radians(azimuth_deg);
    const float elevation = glm::radians(elevation_deg);
    return glm::normalize(glm::vec3{
        std::cos(elevation) * std::cos(azimuth),
        std::sin(elevation),
        std::cos(elevation) * std::sin(azimuth),
    });
}

struct DebugState {
    float exposure = 1.0f;
    float ibl_intensity = 1.0f;
    float light_intensity = 3.2f;
    float light_azimuth = 35.0f;
    float light_elevation = 52.0f;
    glm::vec3 light_color{1.0f, 0.96f, 0.90f};
    glm::vec3 fog_color{0.55f, 0.45f, 0.38f};
    float fog_density = 0.0f;
    float bloom = 0.0f;
    float shadow_strength = 1.0f;
    float stream_radius = 0.0f;
    bool walk_mode = false;
    bool frustum_cull = true;
    bool has_vehicle = false;
    bool driving = false;
    float speed = 0.0f;
    const char* clip = "";
    std::uint32_t materials = 0;
    std::uint32_t textures = 0;
    std::uint32_t sectors_loaded = 0;
    std::uint32_t entities_streamed = 0;
    std::uint32_t instances_drawn = 0;
    std::uint32_t instances_culled = 0;
    glm::vec3 home_position{0.4f, 0.25f, 0.55f};
    float home_yaw = -125.0f;
    float home_pitch = -22.0f;
    const char* model = "";
    const char* lab = "";
    const char* language = "ru";
    const char* help = "Hold RMB: look + fly | WASD: move | Q/E: down/up";
    const char* gamemode = "";
    std::uint32_t script_players = 0;
    std::uint32_t script_vehicles = 0;
    std::uint32_t script_markers = 0;
    std::uint32_t script_labels = 0;
    static constexpr std::uint32_t kMaxWorldLabels = 16;
    glm::vec3 world_label_pos[kMaxWorldLabels]{};
    const char* world_label_text[kMaxWorldLabels]{};
    std::uint32_t world_label_count = 0;
    const char* net_role = "";
    std::uint32_t net_tick = 0;
    std::uint32_t net_streamed = 0;
    std::uint32_t net_peers = 0;
    float net_ping_ms = 0;
    bool survival = false;
    float hp = 100;
    float cold = 0;
    float o2 = 100;
    float stamina = 100;
    float radiation = 0;
    bool near_fire = false;
    bool boosting = false;
    std::uint32_t wood = 0;
    std::uint32_t stone = 0;
    std::uint32_t time_left = 0;
    std::uint8_t phase = 0;
    bool night = false;
    char join_hint[96]{};
};

struct ShadowInputs {
    bool enabled = false;
    SDL_GPUTexture* map = nullptr;
    SDL_GPUSampler* sampler = nullptr;
    glm::mat4 light_vp[3]{};
    glm::vec4 splits{8.0f, 22.0f, 55.0f, 0.0f};
    float texel = 1.0f / 1024.0f;
    float bias = 0.003f;
    float strength = 1.0f;
};

} // namespace forge::render
