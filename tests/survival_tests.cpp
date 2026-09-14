#include "engine/game/session.hpp"
#include "engine/net/interest.hpp"
#include "engine/script/registry.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    forge::script::Registry world;
    forge::game::Sim sim;
    sim.reset();
    const int id = world.spawn_player(sim.spawn_point(0));
    sim.ensure_pawn(id);
    auto* player = world.player(id);
    check(player && sim.pawn(id), "pawn spawn");

    int trees = 0;
    glm::vec3 tree_pos{};
    for (const auto& node : sim.nodes()) {
        if (node.alive && node.kind == forge::game::NodeKind::Tree) {
            ++trees;
            tree_pos = node.position;
        }
    }
    check(trees >= 10, "forest must have trees");

    sim.pawn(id)->inventory.add(forge::game::Item::StoneAxe,1);
    sim.pawn(id)->inventory.equipped = forge::game::Item::StoneAxe;
    player->position = tree_pos;
    sim.tick(.61f,world);
    sim.harvest(id, world);
    sim.tick(.61f,world);
    sim.harvest(id, world);
    sim.tick(.61f,world);
    sim.harvest(id, world);
    check(sim.pawn(id)->inventory[forge::game::Item::Wood] >= 2, "chopping a tree grants wood");

    sim.pawn(id)->inventory[forge::game::Item::Wood] = 1;
    sim.pawn(id)->inventory[forge::game::Item::Stone] = 1;
    sim.place_fire(id, world);
    int blocked = 0;
    for (const auto& node : sim.nodes())
        if (node.alive && node.kind == forge::game::NodeKind::Campfire) ++blocked;
    check(blocked == 0, "campfire needs 3 wood and 2 stone");
    sim.pawn(id)->inventory[forge::game::Item::Wood] = 10;
    sim.pawn(id)->inventory[forge::game::Item::Stone] = 10;
    check(sim.action(id,world,forge::game::Action::Craft,3)==forge::game::Result::Ok,"craft campfire item");
    player->position={50,1,50};
    const auto fires_before = 0;
    sim.place_fire(id, world);
    int fires = 0;
    for (const auto& node : sim.nodes())
        if (node.alive && node.kind == forge::game::NodeKind::Campfire) ++fires;
    check(fires == fires_before + 1, "campfire recipe consumes mats and places");
    check(sim.pawn(id)->inventory[forge::game::Item::Wood] == 4 && sim.pawn(id)->inventory[forge::game::Item::Stone] == 2, "6 wood 8 stone consumed on craft");

    player->position = {0, 1, -4};
    sim.try_extract(id, world);
    check(sim.pawn(id)->extracted, "extract at beacon");
    check(sim.phase() == forge::game::Phase::Won, "solo extract wins the session");

    forge::game::Sim cold;
    cold.reset();
    const int b = world.spawn_player({0, 1, 0});
    cold.ensure_pawn(b);
    world.player(b)->position = {0, 1, 0};
    for (int i = 0; i < 50; ++i) cold.tick(2.0f, world);
    check(cold.night() || cold.pawn(b)->cold > 0 || cold.time_left() < forge::game::Sim::kSessionSeconds,
          "session clock moves");

    forge::game::Sim vitals;
    vitals.reset();
    const int v = world.spawn_player(vitals.spawn_point(2));
    vitals.ensure_pawn(v);
    check(vitals.pawn(v)->o2 == 100.0f && vitals.pawn(v)->stamina == 100.0f && vitals.pawn(v)->radiation == 0.0f,
          "vitals start full");
    world.player(v)->position = {20, 1, 20};
    for (int i = 0; i < 10; ++i) vitals.tick(1.0f, world);
    check(vitals.pawn(v)->o2 < 100.0f, "suit oxygen drains over time");
    check(vitals.pawn(v)->stamina == 100.0f, "stamina only drains on sprint");
    world.player(v)->position = {0, 1, -4};
    vitals.pawn(v)->o2 = 50.0f;
    vitals.tick(1.0f, world);
    check(vitals.pawn(v)->o2 > 50.0f, "extract beacon refills oxygen");

    check(forge::net::xz_distance({0, 0, 0}, {0, 5, 92}) > 70.0f, "far tree is outside default stream");

    std::cout << "Survival checks passed\n";
}
