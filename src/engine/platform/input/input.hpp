#pragma once

#include <glm/glm.hpp>
#include <algorithm>

namespace forge::platform {

struct Input {
    bool captured = false;
    bool jump = false;
    bool toggle_walk = false;
    bool toggle_person = false;
    bool boost = false;
    bool interact = false;
    bool place = false;
    glm::vec3 move{0};
};

inline glm::vec3 compose_walk(const glm::vec3& flat_forward, float strafe, float push)
{
    if (glm::length(flat_forward) < 1e-6f) return glm::vec3{0};
    const glm::vec3 right = glm::normalize(glm::cross(flat_forward, glm::vec3{0.0f, 1.0f, 0.0f}));
    glm::vec3 walk = right * strafe + flat_forward * push;
    const float length = glm::length(glm::vec2(walk.x, walk.z));
    if (length > 1.0f) walk *= 1.0f / length;
    walk.x = std::clamp(walk.x, -1.0f, 1.0f);
    walk.z = std::clamp(walk.z, -1.0f, 1.0f);
    return walk;
}

} // namespace forge::platform
