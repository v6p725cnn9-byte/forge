#pragma once

#include "engine/game_net/prediction/prediction.hpp"

#include <glm/glm.hpp>

namespace forge::net {

inline void correct(Prediction& prediction, glm::vec3 server_position, float server_yaw, std::uint32_t ack_seq)
{
    prediction.reconcile(server_position, server_yaw, ack_seq);
}

} // namespace forge::net
