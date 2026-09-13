#pragma once

#include "engine/world/types.hpp"

namespace forge::world {

struct Frustum {
    glm::vec4 planes[6]{};
};

// Extract planes from a clip matrix that uses [0, 1] depth (SDL GPU / RH_ZO).
Frustum frustum_from_clip(const glm::mat4& clip);
bool aabb_visible(const Frustum& frustum, const Aabb& aabb);

} // namespace forge::world
