#pragma once

#include "engine/net/protocol.hpp"
#include "engine/render/pbr_scene.hpp"
#include "engine/render/pbr_pass.hpp"

#include <array>

namespace forge::render {

class SurvivalVisuals {
public:
    bool create(rhi::Host& host);
    void destroy(rhi::Host& host);
    void update(const net::Snapshot& snapshot, std::uint8_t local_player, float dt, bool interact);
    void draw(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, const net::Snapshot& snapshot,
              const glm::mat4& view_projection, const DebugState& debug, const glm::vec3& camera_position,
              int headless_player = -1);
    glm::vec3 player_position(std::uint8_t id, const glm::vec3& fallback) const;
    std::uint32_t triangles() const { return triangles_; }

private:
    struct PlayerVisual {
        bool active = false;
        std::uint32_t tick = 0;
        glm::vec3 target{0};
        glm::vec3 position{0};
        float yaw = 0;
        float speed = 0;
        int clip = -1;
        int previous_clip = -1;
        float time = 0;
        float previous_time = 0;
        float blend = 1;
        float action_left = 0;
        anim::Palette palette{};
    };

    void animate(PlayerVisual& player, float dt);
    void remove_root_motion(anim::Palette& palette) const;
    int hip_joint_ = -1;
    int head_joint_ = -1;
    glm::mat4 hip_bind_{1};
    glm::vec3 hip_origin_{0};
    PbrScene astronaut_;
    std::array<PbrScene, 3> trees_;
    PbrScene rock_;
    PbrScene campfire_;
    PbrScene flame_;
    glm::mat4 astronaut_transform_{1};
    std::array<glm::mat4, 3> tree_transforms_{};
    glm::mat4 rock_transform_{1};
    glm::mat4 fire_transform_{1};
    std::array<PlayerVisual, 256> players_{};
    SDL_GPUGraphicsPipeline* pbr_ = nullptr;
    SDL_GPUGraphicsPipeline* double_sided_ = nullptr;
    SDL_GPUGraphicsPipeline* skinned_ = nullptr;
    int idle_ = -1;
    int walk_ = -1;
    int run_ = -1;
    int interact_ = -1;
    float clock_ = 0;
    std::uint32_t triangles_ = 0;
};

} // namespace forge::render
