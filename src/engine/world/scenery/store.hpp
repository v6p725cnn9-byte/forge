#pragma once

#include "engine/world/transform/types.hpp"

#include <cstddef>
#include <vector>

namespace forge::world {

// Scenery SoA (props, city kit). Gameplay actors live in game::World.
class Store {
public:
    Entity spawn(const glm::vec3& position, float yaw_degrees, const glm::vec3& scale, std::uint16_t mesh,
                 const glm::vec4& color, const Aabb& local_aabb = kUnitCubeAabb);
    void clear();
    std::size_t size() const { return positions.size(); }

    std::vector<glm::vec3> positions;
    std::vector<float> yaws;
    std::vector<glm::vec3> scales;
    std::vector<std::uint16_t> meshes;
    std::vector<SectorKey> sectors;
    std::vector<std::uint32_t> flags;
    std::vector<Aabb> world_aabbs;
    std::vector<glm::vec4> colors;
};

} // namespace forge::world
