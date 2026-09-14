#pragma once

#include <glm/glm.hpp>

namespace forge::world {

// World units are 32-bit meters, stored sector-relative (see kSectorSize).
// When the observer leaves a cluster, recentre Origin::offset so Jolt and
// the GPU stay inside a comfortable float range. Double is reserved for
// serialization of absolute coordinates, not the sim/render hot path.
struct Origin {
    glm::vec3 offset{0};

    glm::vec3 to_local(const glm::vec3& world) const { return world - offset; }
    glm::vec3 to_world(const glm::vec3& local) const { return local + offset; }
};

} // namespace forge::world
