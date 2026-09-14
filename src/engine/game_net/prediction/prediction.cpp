#include "engine/game_net/prediction/prediction.hpp"

#include "engine/game/session/session.hpp"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace forge::net {

void Prediction::reset(glm::vec3 position, float yaw)
{
    history_.clear();
    position_ = position;
    yaw_ = yaw;
    seeded_ = true;
}

void Prediction::integrate(const Input& input, float dt)
{
    if (!(dt > 0.0f) || dt > 0.25f) return;
    glm::vec3 move{input.move_x, 0.0f, input.move_z};
    const float length = glm::length(glm::vec2(move.x, move.z));
    if (length > 1.0f) move /= length;
    float speed = 0.0f;
    if (length > 0.05f) {
        speed = game::kRunGaitSpeed;
        if (input.boost && game::sprint_cone(move, input.yaw)) speed = game::kSprintGaitSpeed;
    }
    if (input.walking) speed = std::min(speed, game::kWalkGaitSpeed);
    if (input.stance != Stance::Standing) speed = std::min(speed, stance_speed(input.stance));
    position_ += move * speed * dt;
    yaw_ = input.yaw;
}

void Prediction::record(const Input& input, float dt)
{
    if (!seeded_) {
        seeded_ = true;
    }
    integrate(input, dt);
    history_.push_back({input, dt});
    while (history_.size() > 64) history_.pop_front();
}

void Prediction::reconcile(glm::vec3 server_position, float server_yaw, std::uint32_t ack_seq)
{
    while (!history_.empty() && !sequence_newer(history_.front().input.seq, ack_seq)
           && history_.front().input.seq != ack_seq) {
        history_.pop_front();
    }
    if (!history_.empty() && history_.front().input.seq == ack_seq) history_.pop_front();
    position_ = server_position;
    yaw_ = server_yaw;
    for (const auto& sample : history_) integrate(sample.input, sample.dt);
}

} // namespace forge::net
