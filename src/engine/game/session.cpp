#include "engine/game/session.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace forge::game {
namespace {

constexpr float kDayLength = 90.0f;
constexpr float kNightStart = 0.62f;

} // namespace

void Sim::reset()
{
    nodes_.clear();
    for (auto& pawn : pawns_) pawn = {};
    phase_ = Phase::Play;
    time_left_ = kSessionSeconds;
    clock_ = 12.0f;

    Node extract;
    extract.kind = NodeKind::Extract;
    extract.position = {0.0f, 0.0f, -4.0f};
    extract.radius = 3.2f;
    extract.hits = 0;
    nodes_.push_back(extract);

    for (int i = 0; i < 12; ++i) {
        const float angle = glm::radians(static_cast<float>(i) * 30.0f);
        const float ring = 11.0f + static_cast<float>(i % 3) * 3.5f;
        Node tree;
        tree.kind = NodeKind::Tree;
        tree.position = {std::cos(angle) * ring, 0.0f, 10.0f + std::sin(angle) * ring};
        tree.radius = 1.3f;
        tree.hits = 3;
        nodes_.push_back(tree);
    }
    for (int i = 0; i < 6; ++i) {
        const float angle = glm::radians(40.0f + static_cast<float>(i) * 55.0f);
        Node rock;
        rock.kind = NodeKind::Rock;
        rock.position = {std::cos(angle) * 7.5f, 0.0f, 6.0f + std::sin(angle) * 7.5f};
        rock.radius = 1.0f;
        rock.hits = 4;
        nodes_.push_back(rock);
    }
    Node far_tree;
    far_tree.kind = NodeKind::Tree;
    far_tree.position = {0.0f, 0.0f, 92.0f};
    far_tree.hits = 3;
    nodes_.push_back(far_tree);
}

glm::vec3 Sim::spawn_point(int player_id) const
{
    const float x = (player_id % 2 == 0 ? -1.0f : 1.0f) * (4.0f + static_cast<float>(player_id) * 1.6f);
    return {x, 1.0f, 2.0f};
}

void Sim::ensure_pawn(int player_id)
{
    if (player_id < 0 || player_id >= script::kMaxPlayers) return;
    auto& pawn = pawns_[player_id];
    if (pawn.used) return;
    pawn = {};
    pawn.used = true;
}

Pawn* Sim::pawn(int id)
{
    if (id < 0 || id >= script::kMaxPlayers || !pawns_[id].used) return nullptr;
    return &pawns_[id];
}

const Pawn* Sim::pawn(int id) const
{
    if (id < 0 || id >= script::kMaxPlayers || !pawns_[id].used) return nullptr;
    return &pawns_[id];
}

bool Sim::night() const
{
    const float cycle = std::fmod(clock_, kDayLength) / kDayLength;
    return cycle >= kNightStart;
}

float Sim::day01() const { return std::fmod(clock_, kDayLength) / kDayLength; }

int Sim::nearest(const glm::vec3& pos, NodeKind kind, float max_dist) const
{
    int best = -1;
    float best_d = max_dist;
    for (int i = 0; i < static_cast<int>(nodes_.size()); ++i) {
        const auto& node = nodes_[static_cast<std::size_t>(i)];
        if (!node.alive || node.kind != kind) continue;
        const float d = glm::length(glm::vec2(pos.x - node.position.x, pos.z - node.position.z));
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

bool Sim::near_fire(const glm::vec3& pos) const
{
    for (const auto& node : nodes_) {
        if (!node.alive || node.kind != NodeKind::Campfire) continue;
        if (glm::length(glm::vec2(pos.x - node.position.x, pos.z - node.position.z)) < 4.5f) return true;
    }
    return false;
}

void Sim::tick(float dt, script::Registry& world)
{
    if (phase_ != Phase::Play) return;
    clock_ += dt;
    time_left_ -= dt;
    if (time_left_ <= 0.0f) {
        time_left_ = 0;
        bool anyone = false;
        for (const auto& pawn : pawns_)
            if (pawn.used && pawn.extracted) anyone = true;
        phase_ = anyone ? Phase::Won : Phase::Failed;
        return;
    }

    const bool dark = night();
    for (int id = 0; id < script::kMaxPlayers; ++id) {
        auto& pawn = pawns_[id];
        if (!pawn.used || pawn.extracted) continue;
        const auto* player = world.player(id);
        if (!player) continue;
        if (dark && !near_fire(player->position)) pawn.cold = std::min(100.0f, pawn.cold + 14.0f * dt);
        else pawn.cold = std::max(0.0f, pawn.cold - (near_fire(player->position) ? 22.0f : 6.0f) * dt);
        if (pawn.cold > 75.0f) pawn.hp -= 7.0f * dt;
        if (pawn.hp <= 0.0f) {
            pawn.hp = 0;
            phase_ = Phase::Failed;
        }
    }
}

void Sim::harvest(int player_id, script::Registry& world)
{
    auto* pawn = this->pawn(player_id);
    const auto* player = world.player(player_id);
    if (!pawn || !player || pawn->extracted || phase_ != Phase::Play) return;
    int idx = nearest(player->position, NodeKind::Tree, 2.8f);
    if (idx < 0) idx = nearest(player->position, NodeKind::Rock, 2.8f);
    if (idx < 0) return;
    auto& node = nodes_[static_cast<std::size_t>(idx)];
    node.hits -= 1;
    if (node.hits > 0) return;
    node.alive = false;
    if (node.kind == NodeKind::Tree) pawn->wood = static_cast<std::uint16_t>(std::min(99, pawn->wood + 2));
    if (node.kind == NodeKind::Rock) pawn->stone = static_cast<std::uint16_t>(std::min(99, pawn->stone + 2));
}

void Sim::place_fire(int player_id, script::Registry& world)
{
    auto* pawn = this->pawn(player_id);
    const auto* player = world.player(player_id);
    if (!pawn || !player || pawn->extracted || phase_ != Phase::Play) return;
    if (pawn->wood < kFireWood || pawn->stone < kFireStone) return;
    int fires = 0;
    for (const auto& node : nodes_)
        if (node.alive && node.kind == NodeKind::Campfire) ++fires;
    if (fires >= 8) return;
    pawn->wood = static_cast<std::uint16_t>(pawn->wood - kFireWood);
    pawn->stone = static_cast<std::uint16_t>(pawn->stone - kFireStone);
    const float yaw = glm::radians(player->yaw);
    Node fire;
    fire.kind = NodeKind::Campfire;
    fire.position = player->position + glm::vec3{std::sin(yaw), 0.0f, std::cos(yaw)} * 2.2f;
    fire.position.y = 0;
    fire.radius = 1.0f;
    fire.hits = 0;
    nodes_.push_back(fire);
}

void Sim::try_extract(int player_id, script::Registry& world)
{
    auto* pawn = this->pawn(player_id);
    const auto* player = world.player(player_id);
    if (!pawn || !player || pawn->extracted || phase_ != Phase::Play) return;
    const int idx = nearest(player->position, NodeKind::Extract, 3.5f);
    if (idx < 0) return;
    pawn->extracted = true;
    bool all = true;
    bool any = false;
    for (int id = 0; id < script::kMaxPlayers; ++id) {
        if (!pawns_[id].used) continue;
        any = true;
        if (!pawns_[id].extracted) all = false;
    }
    if (any && all) phase_ = Phase::Won;
}

} // namespace forge::game
