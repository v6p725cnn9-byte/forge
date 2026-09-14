#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace forge::game {

constexpr int kMaxPlayers = 32;
constexpr int kMaxVehicles = 32;
constexpr int kMaxMarkers = 32;
constexpr int kMaxLabels = 32;

struct Player {
    bool alive = false;
    glm::vec3 position{0, 1, 0};
    glm::vec3 home{0, 1, 0};
    float yaw = 0;
    std::string name = "Player";
};

struct Vehicle {
    bool alive = false;
    glm::vec3 position{0, 1, 0};
    float yaw = 0;
};

struct Marker {
    bool alive = false;
    glm::vec3 position{0};
    float size = 1;
    glm::vec4 color{0.2f, 0.8f, 1.0f, 1.0f};
};

struct Label {
    bool alive = false;
    glm::vec3 position{0};
    float draw_distance = 20;
    std::string text;
};

// Authoritative networked actors. Lua and the net layer both talk to this;
// they do not own a second copy of positions.
class Actors {
public:
    int spawn_player(glm::vec3 position);
    int spawn_vehicle(glm::vec3 position, float yaw_degrees);
    int create_marker(glm::vec3 position, float size, glm::vec4 color);
    int create_label(std::string text, glm::vec3 position, float draw_distance);
    bool destroy_player(int id);
    bool destroy_vehicle(int id);
    bool destroy_marker(int id);
    bool destroy_label(int id);

    Player* player(int id);
    const Player* player(int id) const;
    Vehicle* vehicle(int id);
    const Vehicle* vehicle(int id) const;
    Marker* marker(int id);
    const Marker* marker(int id) const;
    Label* label(int id);
    const Label* label(int id) const;

    const std::vector<Player>& players() const { return players_; }
    const std::vector<Vehicle>& vehicles() const { return vehicles_; }
    const std::vector<Marker>& markers() const { return markers_; }
    const std::vector<Label>& labels() const { return labels_; }

    std::uint32_t alive_players() const;
    std::uint32_t alive_vehicles() const;
    std::uint32_t alive_markers() const;
    std::uint32_t alive_labels() const;

private:
    std::vector<Player> players_;
    std::vector<Vehicle> vehicles_;
    std::vector<Marker> markers_;
    std::vector<Label> labels_;
};

} // namespace forge::game
