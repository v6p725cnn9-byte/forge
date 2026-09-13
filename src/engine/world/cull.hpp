#pragma once

#include "engine/world/frustum.hpp"
#include "engine/world/store.hpp"

#include <vector>

namespace forge::world {

// Packs streamed entities of `mesh` into GPU instances. A null frustum disables AABB tests.
void collect_instances(const Store& store, const Frustum* frustum, std::uint16_t mesh,
                       std::vector<GpuInstance>& out, StreamStats& stats);

} // namespace forge::world
