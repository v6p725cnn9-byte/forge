#pragma once

#include "engine/game/actors/actors.hpp"
#include "engine/game_net/snapshots/packets.hpp"

namespace forge::net {

float xz_distance(const glm::vec3& a, const glm::vec3& b);
std::vector<Ghost> collect_stream(const game::Actors& world, const glm::vec3& observer, float radius,
                                  int observer_id = -1, int cap = kDefaultSnapshotEntities);

} // namespace forge::net
