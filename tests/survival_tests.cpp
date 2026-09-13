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

    player->position = tree_pos;
    sim.harvest(id, world);
    sim.harvest(id, world);
    sim.harvest(id, world);
    check(sim.pawn(id)->wood >= 2, "chopping a tree grants wood");

    sim.pawn(id)->wood = 1;
    sim.pawn(id)->stone = 1;
    sim.place_fire(id, world);
    int blocked = 0;
    for (const auto& node : sim.nodes())
        if (node.alive && node.kind == forge::game::NodeKind::Campfire) ++blocked;
    check(blocked == 0, "campfire needs 3 wood and 2 stone");
    sim.pawn(id)->wood = 10;
    sim.pawn(id)->stone = 10;
    const auto fires_before = 0;
    sim.place_fire(id, world);
    int fires = 0;
    for (const auto& node : sim.nodes())
        if (node.alive && node.kind == forge::game::NodeKind::Campfire) ++fires;
    check(fires == fires_before + 1, "campfire recipe consumes mats and places");
    check(sim.pawn(id)->wood == 7 && sim.pawn(id)->stone == 8, "3 wood 2 stone");

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

    check(forge::net::xz_distance({0, 0, 0}, {0, 5, 92}) > 70.0f, "far tree is outside default stream");

    std::cout << "Survival checks passed\n";
}
