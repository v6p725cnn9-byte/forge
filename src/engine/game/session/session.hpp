#pragma once

#include "engine/game/actors/actors.hpp"
#include "engine/game/inventory/items.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

namespace forge::game {

// ALS-Refactored gaits from FAlsMovementGaitSettings / MS_Als_Normal (cm -> m).
enum class Gait : std::uint8_t { Walk, Run, Sprint };

constexpr float kWalkGaitSpeed = 1.75f;
constexpr float kRunGaitSpeed = 3.75f;
constexpr float kSprintGaitSpeed = 6.50f;

// AAlsCharacter::CanSprint: input must sit inside a 50-degree yaw cone of the view.
inline bool sprint_cone(glm::vec3 move, float yaw_degrees)
{
    const glm::vec2 flat{move.x, move.z};
    const float length = glm::length(flat);
    if (length <= 0.05f) return false;
    const float yaw = glm::radians(yaw_degrees);
    const glm::vec2 forward{std::sin(yaw), std::cos(yaw)};
    return glm::dot(flat / length, forward) > std::cos(glm::radians(50.0f));
}

enum class NodeKind : std::uint8_t {
    Tree = 1, Rock = 2, Campfire = 3, Extract = 4, Stick, Pebble, Flint, Fiber, IronOre, Furnace, Bench, Loot
};

constexpr std::uint16_t kExtractIngots = 4;
constexpr float kRespawnSeconds = 8.0f;
constexpr float kCampfireBurn = 80.0f;

enum class Phase : std::uint8_t { Play = 0, Won = 1, Failed = 2 };

struct Node {
    bool alive = true;
    NodeKind kind = NodeKind::Tree;
    glm::vec3 position{0};
    float radius = 1.1f;
    int hits = 3;
    float respawn = 0;
    float fuel = 0;
    Inventory loot{};
};

struct Pawn {
    bool used = false;
    bool extracted = false;
    float hp = 100;
    float cold = 0;
    float o2 = 100;
    float stamina = 100;
    float radiation = 0;
    Inventory inventory;
    float harvest_cooldown = 0;
    float respawn = 0;
    bool pending_spawn = false;
    Result feedback = Result::None;
};

class Sim {
public:
    static constexpr float kSessionSeconds = 360.0f;

    void reset();
    void ensure_pawn(int player_id);
    void remove_pawn(int player_id);
    glm::vec3 spawn_point(int player_id) const;
    void tick(float dt, Actors& world);
    void harvest(int player_id, Actors& world);
    Result action(int player_id, Actors& world, Action action, std::uint8_t argument);
    bool near_station(int player_id, const Actors& world, Station station) const;
    void place_fire(int player_id, Actors& world);
    void try_extract(int player_id, Actors& world);

    const std::vector<Node>& nodes() const { return nodes_; }
    const Pawn* pawn(int id) const;
    Pawn* pawn(int id);
    Phase phase() const { return phase_; }
    float time_left() const { return time_left_; }
    bool night() const;
    float day01() const;

private:
    int nearest(const glm::vec3& pos, NodeKind kind, float max_dist) const;
    bool near_fire(const glm::vec3& pos) const;
    void down(int player_id, Actors& world);

    std::vector<Node> nodes_;
    Pawn pawns_[kMaxPlayers]{};
    Phase phase_ = Phase::Play;
    float time_left_ = kSessionSeconds;
    float clock_ = 0;
};

} // namespace forge::game
