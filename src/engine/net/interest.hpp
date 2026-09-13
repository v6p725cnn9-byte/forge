#pragma once

#include "engine/net/protocol.hpp"
#include "engine/script/registry.hpp"

namespace forge::net {

float xz_distance(const glm::vec3& a, const glm::vec3& b);
std::vector<Ghost> collect_stream(const script::Registry& world, const glm::vec3& observer, float radius,
                                  int observer_id = -1, int cap = kMaxSnapshotEntities);

} // namespace forge::net
