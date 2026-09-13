#include "engine/net/interest.hpp"

#include <algorithm>
#include <cmath>

namespace forge::net {

float xz_distance(const glm::vec3& a, const glm::vec3& b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

std::vector<Ghost> collect_stream(const script::Registry& world, const glm::vec3& observer, float radius,
                                  int observer_id, int cap)
{
    struct Ranked {
        float distance;
        Ghost ghost;
    };
    std::vector<Ranked> ranked;
    const float limit = std::max(radius, 0.0f);

    auto push = [&](float distance, Ghost ghost) {
        if (distance > limit) return;
        ranked.push_back({distance, std::move(ghost)});
    };

    for (int id = 0; id < static_cast<int>(world.players().size()); ++id) {
        const auto* player = world.player(id);
        if (!player) continue;
        Ghost ghost;
        ghost.kind = Kind::Player;
        ghost.id = static_cast<std::uint8_t>(id);
        ghost.position = player->position;
        ghost.yaw = player->yaw;
        ghost.name = player->name;
        const float distance = (id == observer_id) ? 0.0f : xz_distance(observer, player->position);
        push(distance, std::move(ghost));
    }
    for (int id = 0; id < static_cast<int>(world.vehicles().size()); ++id) {
        const auto* vehicle = world.vehicle(id);
        if (!vehicle) continue;
        Ghost ghost;
        ghost.kind = Kind::Vehicle;
        ghost.id = static_cast<std::uint8_t>(id);
        ghost.position = vehicle->position;
        ghost.yaw = vehicle->yaw;
        push(xz_distance(observer, vehicle->position), std::move(ghost));
    }
    for (int id = 0; id < static_cast<int>(world.markers().size()); ++id) {
        const auto* marker = world.marker(id);
        if (!marker) continue;
        Ghost ghost;
        ghost.kind = Kind::Marker;
        ghost.id = static_cast<std::uint8_t>(id);
        ghost.position = marker->position;
        ghost.size = marker->size;
        ghost.color = marker->color;
        push(xz_distance(observer, marker->position), std::move(ghost));
    }
    for (int id = 0; id < static_cast<int>(world.labels().size()); ++id) {
        const auto* label = world.label(id);
        if (!label) continue;
        const float distance = xz_distance(observer, label->position);
        if (distance > label->draw_distance) continue;
        Ghost ghost;
        ghost.kind = Kind::Label;
        ghost.id = static_cast<std::uint8_t>(id);
        ghost.position = label->position;
        ghost.text = label->text;
        push(distance, std::move(ghost));
    }

    std::sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) { return a.distance < b.distance; });
    if (static_cast<int>(ranked.size()) > cap) ranked.resize(static_cast<std::size_t>(cap));
    std::vector<Ghost> out;
    out.reserve(ranked.size());
    for (auto& item : ranked) out.push_back(std::move(item.ghost));
    return out;
}

} // namespace forge::net
