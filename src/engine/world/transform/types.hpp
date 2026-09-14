#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdint>

namespace forge::world {

using Entity = std::uint32_t;
constexpr Entity kInvalid = ~0u;

constexpr float kSectorSize = 32.0f;
constexpr std::uint32_t kAlive = 1u;
constexpr std::uint32_t kStreamed = 2u;

struct Aabb {
    glm::vec3 min{0};
    glm::vec3 max{0};
};

struct SectorCoord {
    std::int16_t x = 0;
    std::int16_t z = 0;
    bool operator==(const SectorCoord&) const = default;
};

using SectorKey = std::uint32_t;

struct GpuInstance {
    glm::mat4 world{1.0f};
    glm::vec4 color{1.0f};
};
static_assert(sizeof(GpuInstance) == 80);

struct StreamStats {
    std::uint32_t sectors_loaded = 0;
    std::uint32_t entities_streamed = 0;
    std::uint32_t instances_drawn = 0;
    std::uint32_t instances_culled = 0;
};

inline SectorCoord sector_of(const glm::vec3& position)
{
    return {static_cast<std::int16_t>(std::floor(position.x / kSectorSize)),
            static_cast<std::int16_t>(std::floor(position.z / kSectorSize))};
}

inline SectorKey pack_sector(SectorCoord coord)
{
    return (static_cast<SectorKey>(static_cast<std::uint16_t>(coord.x)) << 16)
        | static_cast<SectorKey>(static_cast<std::uint16_t>(coord.z));
}

inline SectorCoord unpack_sector(SectorKey key)
{
    return {static_cast<std::int16_t>(key >> 16), static_cast<std::int16_t>(key & 0xFFFFu)};
}

inline glm::vec2 sector_min(SectorCoord coord)
{
    return {static_cast<float>(coord.x) * kSectorSize, static_cast<float>(coord.z) * kSectorSize};
}

inline float sector_distance_xz(SectorCoord coord, const glm::vec3& camera)
{
    const glm::vec2 mn = sector_min(coord);
    const glm::vec2 mx = mn + glm::vec2{kSectorSize, kSectorSize};
    float dx = 0.0f;
    float dz = 0.0f;
    if (camera.x < mn.x) dx = mn.x - camera.x;
    else if (camera.x > mx.x) dx = camera.x - mx.x;
    if (camera.z < mn.y) dz = mn.y - camera.z;
    else if (camera.z > mx.y) dz = camera.z - mx.y;
    return std::sqrt(dx * dx + dz * dz);
}

inline glm::mat4 entity_world(const glm::vec3& position, float yaw_degrees, const glm::vec3& scale)
{
    glm::mat4 world{1.0f};
    world = glm::translate(world, position);
    world = glm::rotate(world, glm::radians(yaw_degrees), glm::vec3{0.0f, 1.0f, 0.0f});
    world = glm::scale(world, scale);
    return world;
}

inline Aabb transform_aabb(const Aabb& local, const glm::mat4& world)
{
    Aabb out{{1.0e9f, 1.0e9f, 1.0e9f}, {-1.0e9f, -1.0e9f, -1.0e9f}};
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner{
            (i & 1) ? local.max.x : local.min.x,
            (i & 2) ? local.max.y : local.min.y,
            (i & 4) ? local.max.z : local.min.z,
        };
        const glm::vec3 transformed = glm::vec3(world * glm::vec4(corner, 1.0f));
        out.min = glm::min(out.min, transformed);
        out.max = glm::max(out.max, transformed);
    }
    return out;
}

constexpr Aabb kUnitCubeAabb{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};

} // namespace forge::world
