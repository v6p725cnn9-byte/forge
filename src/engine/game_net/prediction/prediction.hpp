#pragma once

#include "engine/game_net/snapshots/packets.hpp"

#include <deque>

namespace forge::net {

// Kinematic client prediction matching ALS gait speeds. Collision stays on
// the server; the client replays unacked input after a snapshot correction.
class Prediction {
public:
    void reset(glm::vec3 position, float yaw);
    void record(const Input& input, float dt);
    void reconcile(glm::vec3 server_position, float server_yaw, std::uint32_t ack_seq);
    glm::vec3 position() const { return position_; }
    float yaw() const { return yaw_; }

private:
    struct Sample {
        Input input;
        float dt = 0;
    };

    void integrate(const Input& input, float dt);

    std::deque<Sample> history_;
    glm::vec3 position_{0};
    float yaw_ = 0;
    bool seeded_ = false;
};

} // namespace forge::net
