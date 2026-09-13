#include "engine/script/registry.hpp"

#include <utility>

namespace forge::script {
namespace {

template <typename T>
int spawn_slot(std::vector<T>& items, int cap, T value)
{
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (!items[static_cast<std::size_t>(i)].alive) {
            items[static_cast<std::size_t>(i)] = std::move(value);
            return i;
        }
    }
    if (static_cast<int>(items.size()) >= cap) return -1;
    items.push_back(std::move(value));
    return static_cast<int>(items.size()) - 1;
}

template <typename T>
T* slot(std::vector<T>& items, int id)
{
    if (id < 0 || id >= static_cast<int>(items.size())) return nullptr;
    auto& item = items[static_cast<std::size_t>(id)];
    return item.alive ? &item : nullptr;
}

template <typename T>
const T* slot(const std::vector<T>& items, int id)
{
    if (id < 0 || id >= static_cast<int>(items.size())) return nullptr;
    const auto& item = items[static_cast<std::size_t>(id)];
    return item.alive ? &item : nullptr;
}

template <typename T>
bool destroy_slot(std::vector<T>& items, int id)
{
    auto* item = slot(items, id);
    if (!item) return false;
    item->alive = false;
    return true;
}

template <typename T>
std::uint32_t count_alive(const std::vector<T>& items)
{
    std::uint32_t n = 0;
    for (const auto& item : items)
        if (item.alive) ++n;
    return n;
}

} // namespace

int Registry::spawn_player(glm::vec3 position)
{
    Player player;
    player.alive = true;
    player.position = position;
    player.home = position;
    return spawn_slot(players_, kMaxPlayers, std::move(player));
}

int Registry::spawn_vehicle(glm::vec3 position, float yaw_degrees)
{
    Vehicle vehicle;
    vehicle.alive = true;
    vehicle.position = position;
    vehicle.yaw = yaw_degrees;
    return spawn_slot(vehicles_, kMaxVehicles, std::move(vehicle));
}

int Registry::create_marker(glm::vec3 position, float size, glm::vec4 color)
{
    Marker marker;
    marker.alive = true;
    marker.position = position;
    marker.size = size > 0.05f ? size : 1.0f;
    marker.color = color;
    return spawn_slot(markers_, kMaxMarkers, std::move(marker));
}

int Registry::create_label(std::string text, glm::vec3 position, float draw_distance)
{
    Label label;
    label.alive = true;
    label.text = std::move(text);
    label.position = position;
    label.draw_distance = draw_distance > 0.0f ? draw_distance : 20.0f;
    return spawn_slot(labels_, kMaxLabels, std::move(label));
}

bool Registry::destroy_player(int id) { return destroy_slot(players_, id); }
bool Registry::destroy_vehicle(int id) { return destroy_slot(vehicles_, id); }
bool Registry::destroy_marker(int id) { return destroy_slot(markers_, id); }
bool Registry::destroy_label(int id) { return destroy_slot(labels_, id); }

Player* Registry::player(int id) { return slot(players_, id); }
const Player* Registry::player(int id) const { return slot(players_, id); }
Vehicle* Registry::vehicle(int id) { return slot(vehicles_, id); }
const Vehicle* Registry::vehicle(int id) const { return slot(vehicles_, id); }
Marker* Registry::marker(int id) { return slot(markers_, id); }
const Marker* Registry::marker(int id) const { return slot(markers_, id); }
Label* Registry::label(int id) { return slot(labels_, id); }
const Label* Registry::label(int id) const { return slot(labels_, id); }

std::uint32_t Registry::alive_players() const { return count_alive(players_); }
std::uint32_t Registry::alive_vehicles() const { return count_alive(vehicles_); }
std::uint32_t Registry::alive_markers() const { return count_alive(markers_); }
std::uint32_t Registry::alive_labels() const { return count_alive(labels_); }

} // namespace forge::script
