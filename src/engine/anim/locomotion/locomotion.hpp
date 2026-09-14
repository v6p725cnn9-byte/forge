#pragma once

#include "engine/anim/skeleton/animator.hpp"
#include "engine/core/locomotion/motion.hpp"
#include <array>

namespace forge::anim {

enum class MoveState { Idle, Start, Walk, Run, Sprint, Stop, Pivot, Turn, Jump, Fall, Land, Recover,
                       Crouch, CrouchWalk, Crawl, Mantle, Reach, Push, Pull };
const char* movement_name(MoveState state);

struct LocomotionState {
    MoveState state = MoveState::Idle;
    Motion motion;
    float phase = 0;
    float clock = 0;
    float state_time = 0;
    float previous_yaw = 0;
    float speed = 0;
    float crouch = 0;
    float prone = 0;
    float air = 0;
    float landing = 0;
    float direction = 0;
    float reach = 0;
    float mantle_weight = 0;
    glm::vec2 lean{0};
    glm::vec2 ground{0};
    glm::vec3 previous_velocity{0};
    std::vector<glm::vec3> translation, scale;
    std::vector<glm::quat> rotation;
};

class Locomotion {
public:
    void bind(const assets::Scene& scene, float model_scale);
    void evaluate(const assets::Scene& scene, LocomotionState& state, const Motion& motion,
                  float actor_yaw, float dt, Palette& palette) const;
private:
    int idle_ = -1, walk_ = -1, run_ = -1;
    int hip_ = -1, spine_ = -1, chest_ = -1, head_ = -1;
    std::array<int, 2> thigh_{-1,-1}, shin_{-1,-1}, foot_{-1,-1}, arm_{-1,-1}, forearm_{-1,-1};
    glm::vec3 hip_origin_{0};
    float scale_ = 1;
    float units_ = 100;
    float walk_stride_ = 1.5f, run_stride_ = 3.5f;
};

} // namespace forge::anim
