#pragma once

#include "engine/script/registry.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace forge::game {

enum class NodeKind : std::uint8_t { Tree = 1, Rock = 2, Campfire = 3, Extract = 4 };

enum class Phase : std::uint8_t { Play = 0, Won = 1, Failed = 2 };

struct Node {
    bool alive = true;
    NodeKind kind = NodeKind::Tree;
    glm::vec3 position{0};
    float radius = 1.1f;
    int hits = 3;
};

struct Pawn {
    bool used = false;
    bool extracted = false;
    float hp = 100;
    float cold = 0;
    std::uint16_t wood = 0;
    std::uint16_t stone = 0;
};

class Sim {
public:
    static constexpr float kSessionSeconds = 360.0f;
    static constexpr int kFireWood = 3;
    static constexpr int kFireStone = 2;

    void reset();
    void ensure_pawn(int player_id);
    glm::vec3 spawn_point(int player_id) const;
    void tick(float dt, script::Registry& world);
    void harvest(int player_id, script::Registry& world);
    void place_fire(int player_id, script::Registry& world);
    void try_extract(int player_id, script::Registry& world);

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

    std::vector<Node> nodes_;
    Pawn pawns_[script::kMaxPlayers]{};
    Phase phase_ = Phase::Play;
    float time_left_ = kSessionSeconds;
    float clock_ = 0;
};

} // namespace forge::game
