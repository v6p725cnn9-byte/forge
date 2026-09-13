#include "engine/world/store.hpp"

namespace forge::world {

Entity Store::spawn(const glm::vec3& position, float yaw_degrees, const glm::vec3& scale, std::uint16_t mesh,
                    const glm::vec4& color, const Aabb& local_aabb)
{
    const auto id = static_cast<Entity>(positions.size());
    const glm::mat4 world = entity_world(position, yaw_degrees, scale);
    positions.push_back(position);
    yaws.push_back(yaw_degrees);
    scales.push_back(scale);
    meshes.push_back(mesh);
    sectors.push_back(pack_sector(sector_of(position)));
    flags.push_back(kAlive);
    world_aabbs.push_back(transform_aabb(local_aabb, world));
    colors.push_back(color);
    return id;
}

void Store::clear()
{
    positions.clear();
    yaws.clear();
    scales.clear();
    meshes.clear();
    sectors.clear();
    flags.clear();
    world_aabbs.clear();
    colors.clear();
}

} // namespace forge::world
