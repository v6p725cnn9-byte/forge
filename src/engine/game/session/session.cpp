#include "engine/game/session/session.hpp"

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
    // A renewable hand-gathering layer prevents a broken-tool resource deadlock.
    for (int ring = 0; ring < 4; ++ring) {
        for (int i = 0; i < 24; ++i) {
            Node pickup;
            pickup.kind = static_cast<NodeKind>(static_cast<int>(NodeKind::Stick) + i % 4);
            const float angle = glm::radians(i * 15.0f + ring * 7.5f);
            const float radius = 5.0f + ring * 6.0f;
            pickup.position = {std::cos(angle) * radius, 0, 4 + std::sin(angle) * radius};
            pickup.radius = .35f;
            pickup.hits = 1;
            nodes_.push_back(pickup);
        }
    }
    for (int i = 0; i < 6; ++i) {
        Node ore;
        ore.kind = NodeKind::IronOre;
        const float angle = glm::radians(i * 60.0f);
        ore.position = {std::cos(angle) * 32, 0, 4 + std::sin(angle) * 32};
        ore.hits = 12;
        nodes_.push_back(ore);
    }
}

glm::vec3 Sim::spawn_point(int player_id) const
{
    const float x = (player_id % 2 == 0 ? -1.0f : 1.0f) * (4.0f + static_cast<float>(player_id) * 1.6f);
    return {x, 1.0f, 2.0f};
}

void Sim::remove_pawn(int player_id)
{
    if (player_id < 0 || player_id >= kMaxPlayers) return;
    pawns_[player_id] = {};
    bool any = false, all = true;
    for (const auto& pawn : pawns_) {
        if (!pawn.used) continue;
        any = true;
        all = all && pawn.extracted;
    }
    if (phase_ == Phase::Play && any && all) phase_ = Phase::Won;
}

void Sim::ensure_pawn(int player_id)
{
    if (player_id < 0 || player_id >= kMaxPlayers) return;
    auto& pawn = pawns_[player_id];
    if (pawn.used) return;
    pawn = {};
    pawn.used = true;
}

Pawn* Sim::pawn(int id)
{
    if (id < 0 || id >= kMaxPlayers || !pawns_[id].used) return nullptr;
    return &pawns_[id];
}

const Pawn* Sim::pawn(int id) const
{
    if (id < 0 || id >= kMaxPlayers || !pawns_[id].used) return nullptr;
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

void Sim::tick(float dt, Actors& world)
{
    if (!std::isfinite(dt) || dt <= 0) return;
    for (auto& pawn : pawns_) pawn.harvest_cooldown = std::max(0.0f, pawn.harvest_cooldown - dt);
    for (auto& node : nodes_) {
        if (node.alive && node.kind == NodeKind::Campfire) {
            node.fuel = std::max(0.0f, node.fuel - dt);
            if (node.fuel <= 0.0f) node.alive = false;
        }
        if (!node.alive && node.respawn > 0) {
            node.respawn = std::max(0.0f, node.respawn - dt);
            if (node.respawn == 0) {
                node.alive = true;
                node.hits = node.kind == NodeKind::Tree ? 3
                    : node.kind == NodeKind::Rock ? 4
                    : node.kind == NodeKind::IronOre ? 12 : 1;
            }
        }
    }
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
    for (int id = 0; id < kMaxPlayers; ++id) {
        auto& pawn = pawns_[id];
        if (!pawn.used || pawn.extracted) continue;
        if (pawn.hp <= 0.0f) {
            pawn.respawn = std::max(0.0f, pawn.respawn - dt);
            if (pawn.respawn <= 0.0f) {
                pawn.hp = 100.0f;
                pawn.cold = 0;
                pawn.o2 = 100.0f;
                pawn.radiation = 0;
                pawn.stamina = 50.0f;
                pawn.pending_spawn = true;
            }
            continue;
        }
        const auto* player = world.player(id);
        if (!player) continue;
        if (dark && !near_fire(player->position)) pawn.cold = std::min(100.0f, pawn.cold + 14.0f * dt);
        else pawn.cold = std::max(0.0f, pawn.cold - (near_fire(player->position) ? 22.0f : 6.0f) * dt);
        if (pawn.cold > 75.0f) pawn.hp -= 7.0f * dt;
        // Suit oxygen drains over the session; the extract beacon doubles as a
        // resupply cache, so standing in its radius refills the tank.
        if (nearest(player->position, NodeKind::Extract, 6.0f) >= 0)
            pawn.o2 = std::min(100.0f, pawn.o2 + 15.0f * dt);
        else
            pawn.o2 = std::max(0.0f, pawn.o2 - 0.33f * dt);
        if (pawn.o2 <= 0.0f) pawn.hp -= 2.0f * dt;
        // Cosmic radiation bites at night away from shelter; campfire EM
        // shielding and daylight bleed it back off.
        if (dark && !near_fire(player->position)) pawn.radiation = std::min(100.0f, pawn.radiation + 1.2f * dt);
        else pawn.radiation = std::max(0.0f, pawn.radiation - (near_fire(player->position) ? 3.0f : 1.0f) * dt);
        if (pawn.radiation > 75.0f) pawn.hp -= 5.0f * dt;
        if (pawn.hp <= 0.0f) down(id, world);
    }
}

bool Sim::near_station(int id, const Actors& world, Station station) const
{
    const auto* player = world.player(id);
    if (!player) return false;
    if (station == Station::Hand) return true;
    return nearest(player->position, station == Station::Furnace ? NodeKind::Furnace : NodeKind::Bench, 4) >= 0;
}

Result Sim::action(int id, Actors& world, Action operation, std::uint8_t argument)
{
    auto* pawn = this->pawn(id);
    const auto* player = world.player(id);
    if (!pawn || !player) return Result::Invalid;
    auto perform = [&]() -> Result {
        if (phase_ != Phase::Play || pawn->extracted || pawn->hp <= 0) return Result::Invalid;
        auto& bag = pawn->inventory;
        const auto item = static_cast<Item>(argument);
        if (operation == Action::Craft) {
            if (argument >= kRecipes.size()) return Result::Invalid;
            if (!near_station(id, world, kRecipes[argument].station)) return Result::NeedStation;
            return craft_inventory(bag, argument);
        }
        if (!valid(item)) return Result::Invalid;
        if (operation == Action::Equip && item == Item::None) { bag.equipped = item; return Result::Ok; }
        if (item == Item::None || bag[item] == 0) return Result::Missing;
        const auto& def = definition(item);
        if (operation == Action::Equip) {
            if (def.tool == Tool::None) return Result::Invalid;
            bag.equipped = item;
            return Result::Ok;
        }
        if (operation == Action::Repair) {
            if (!def.durability || bag.durability[index(item)] == def.durability) return Result::Invalid;
            const Item material = item == Item::IronAxe || item == Item::IronPickaxe ? Item::IronIngot : Item::Flint;
            if (bag[material] < 2 || bag[Item::Rope] < 1) return Result::Missing;
            bag[material] -= 2;
            --bag[Item::Rope];
            bag.durability[index(item)] = def.durability;
            return Result::Ok;
        }
        if (operation == Action::Discard) {
            --bag[item];
            if (!bag[item]) {
                bag.durability[index(item)] = 0;
                if (bag.equipped == item) bag.equipped = Item::None;
            }
            return Result::Ok;
        }
        if (operation != Action::Use) return Result::Invalid;
        if (item == Item::Bandage) {
            if (pawn->hp >= 100) return Result::Invalid;
            --bag[item];
            pawn->hp = std::min(100.0f, pawn->hp + 25);
            return Result::Ok;
        }
        if (item != Item::Campfire && item != Item::Furnace && item != Item::Bench) return Result::Invalid;
        int placed = 0;
        for (const auto& node : nodes_)
            if (node.alive && (node.kind == NodeKind::Campfire || node.kind == NodeKind::Furnace || node.kind == NodeKind::Bench)) ++placed;
        if (placed >= 16 || nodes_.size() >= 255) return Result::Full;
        const float angle = glm::radians(player->yaw);
        const auto position = glm::vec3{player->position.x, 0, player->position.z}
            + glm::vec3{std::sin(angle), 0, std::cos(angle)} * 2.2f;
        for (const auto& node : nodes_) {
            if (!node.alive || node.kind == NodeKind::Stick || node.kind == NodeKind::Pebble
                || node.kind == NodeKind::Flint || node.kind == NodeKind::Fiber) continue;
            if (glm::length(glm::vec2(position.x - node.position.x, position.z - node.position.z)) < 1.6f)
                return Result::Invalid;
        }
        Node placed_node;
        placed_node.kind = item == Item::Campfire ? NodeKind::Campfire : item == Item::Furnace ? NodeKind::Furnace : NodeKind::Bench;
        placed_node.position = position;
        placed_node.hits = 0;
        if (placed_node.kind == NodeKind::Campfire) placed_node.fuel = kCampfireBurn;
        nodes_.push_back(placed_node);
        --bag[item];
        return Result::Ok;
    };
    pawn->feedback = perform();
    return pawn->feedback;
}

void Sim::down(int id, Actors& world)
{
    auto* pawn = this->pawn(id);
    const auto* player = world.player(id);
    if (!pawn || pawn->hp > 0.0f) return;
    pawn->hp = 0;
    pawn->respawn = kRespawnSeconds;
    bool any = false;
    for (std::size_t i = 1; i < kItemCount; ++i)
        if (pawn->inventory.count[i]) any = true;
    if (any && player && nodes_.size() < 255) {
        Node cache;
        cache.kind = NodeKind::Loot;
        cache.position = {player->position.x, 0.0f, player->position.z};
        cache.radius = 0.9f;
        cache.hits = 1;
        cache.loot = pawn->inventory;
        nodes_.push_back(cache);
    }
    pawn->inventory = {};
}

void Sim::harvest(int id, Actors& world)
{
    auto* pawn = this->pawn(id);
    const auto* player = world.player(id);
    if (!pawn || !player || pawn->extracted || phase_ != Phase::Play || pawn->hp <= 0) return;
    if (pawn->harvest_cooldown > 0) { pawn->feedback = Result::Cooldown; return; }

    int victim = -1;
    float victim_d = 2.4f;
    for (int other = 0; other < kMaxPlayers; ++other) {
        if (other == id || !pawns_[other].used || pawns_[other].extracted || pawns_[other].hp <= 0) continue;
        const auto* body = world.player(other);
        if (!body) continue;
        const float d = glm::length(glm::vec2(body->position.x - player->position.x, body->position.z - player->position.z));
        if (d < victim_d) {
            victim_d = d;
            victim = other;
        }
    }
    if (victim >= 0) {
        const auto& tool = definition(pawn->inventory.equipped);
        const float damage = tool.tool == Tool::None ? 12.0f
            : (pawn->inventory.equipped == Item::IronAxe || pawn->inventory.equipped == Item::IronPickaxe ? 34.0f : 22.0f);
        pawns_[victim].hp -= damage;
        pawn->stamina = std::max(0.0f, pawn->stamina - 10.0f);
        pawn->harvest_cooldown = 0.55f;
        pawn->feedback = Result::Ok;
        if (pawns_[victim].hp <= 0.0f) down(victim, world);
        return;
    }

    int target = -1;
    float distance = 2.8f;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        if (!node.alive || node.kind == NodeKind::Campfire || node.kind == NodeKind::Extract
            || node.kind == NodeKind::Furnace || node.kind == NodeKind::Bench) continue;
        const auto delta = node.position - player->position;
        const float d = glm::length(glm::vec2(delta.x, delta.z));
        if (d < distance) { target = static_cast<int>(i); distance = d; }
    }
    if (target < 0) { pawn->feedback = Result::TooFar; return; }
    auto& node = nodes_[static_cast<std::size_t>(target)];
    auto& bag = pawn->inventory;
    if (node.kind == NodeKind::Loot) {
        bool took = false;
        for (std::size_t i = 1; i < kItemCount; ++i) {
            const auto item = static_cast<Item>(i);
            while (node.loot[item] > 0 && bag.add(item, 1)) {
                --node.loot[item];
                took = true;
            }
            if (node.loot[item] == 0) node.loot.durability[i] = 0;
        }
        if (node.loot.weight() <= 0.001f) node.alive = false;
        pawn->harvest_cooldown = 0.25f;
        pawn->feedback = took ? Result::Ok : Result::Full;
        return;
    }
    const auto& tool = definition(bag.equipped);
    const Tool required = node.kind == NodeKind::Tree ? Tool::Axe
        : node.kind == NodeKind::Rock || node.kind == NodeKind::IronOre ? Tool::Pickaxe : Tool::None;
    if (required != Tool::None && pawn->stamina < 8.0f) { pawn->feedback = Result::Cooldown; return; }
    if (required != Tool::None && (tool.tool != required || bag[bag.equipped] == 0)) {
        pawn->feedback = required == Tool::Axe ? Result::NeedAxe : Result::NeedPickaxe;
        return;
    }
    if (required != Tool::None && !bag.durability[index(bag.equipped)]) { pawn->feedback = Result::Broken; return; }
    Item material = Item::Wood;
    switch (node.kind) {
    case NodeKind::Tree: material = Item::Wood; break;
    case NodeKind::Rock: case NodeKind::Pebble: material = Item::Stone; break;
    case NodeKind::Stick: material = Item::Stick; break;
    case NodeKind::Flint: material = Item::Flint; break;
    case NodeKind::Fiber: material = Item::Fiber; break;
    case NodeKind::IronOre: material = Item::IronOre; break;
    default: return;
    }
    const bool iron = bag.equipped == Item::IronAxe || bag.equipped == Item::IronPickaxe;
    const std::uint16_t yield = required != Tool::None ? (iron ? 4 : 2) : (material == Item::Fiber ? 6 : 3);
    if (!bag.add(material, yield)) { pawn->feedback = Result::Full; return; }
    if (required != Tool::None) {
        --bag.durability[index(bag.equipped)];
        pawn->stamina = std::max(0.0f, pawn->stamina - 8.0f);
    }
    if (--node.hits <= 0) {
        node.alive = false;
        node.respawn = required == Tool::None ? 45.0f
            : node.kind == NodeKind::Tree ? 90.0f
            : node.kind == NodeKind::Rock ? 120.0f : 180.0f;
    }
    pawn->harvest_cooldown = required == Tool::None ? .25f : .6f;
    pawn->feedback = Result::Ok;
}

void Sim::place_fire(int id, Actors& world)
{
    action(id, world, Action::Use, static_cast<std::uint8_t>(Item::Campfire));
}

void Sim::try_extract(int player_id, Actors& world)
{
    auto* pawn = this->pawn(player_id);
    const auto* player = world.player(player_id);
    if (!pawn || !player || pawn->extracted || phase_ != Phase::Play || pawn->hp <= 0) return;
    const int idx = nearest(player->position, NodeKind::Extract, 3.5f);
    if (idx < 0) return;
    if (pawn->inventory[Item::IronIngot] < kExtractIngots) {
        pawn->feedback = Result::NeedCargo;
        return;
    }
    pawn->inventory[Item::IronIngot] -= kExtractIngots;
    pawn->extracted = true;
    bool all = true;
    bool any = false;
    for (int id = 0; id < kMaxPlayers; ++id) {
        if (!pawns_[id].used) continue;
        any = true;
        if (!pawns_[id].extracted) all = false;
    }
    if (any && all) phase_ = Phase::Won;
}

} // namespace forge::game
