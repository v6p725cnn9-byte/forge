#include "engine/game/session/session.hpp"
#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
}
int main()
{
    using namespace forge::game;
    Actors world;
    Sim sim;
    sim.reset();
    const int id = world.spawn_player(sim.spawn_point(0));
    sim.ensure_pawn(id);
    auto& bag = sim.pawn(id)->inventory;
    auto move_to = [&](NodeKind kind) {
        for (const auto& node : sim.nodes()) {
            if (node.alive && node.kind == kind) {
                world.player(id)->position = node.position + glm::vec3{0,1,0};
                return;
            }
        }
        check(false,"resource nodes exhausted unexpectedly");
    };
    auto gather = [&](NodeKind kind, Item item, int target) {
        for (int i = 0; bag[item] < target && i < 100; ++i) {
            move_to(kind);
            sim.pawn(id)->stamina = 100;
            sim.tick(.61f,world);
            sim.harvest(id,world);
            check(sim.pawn(id)->feedback == Result::Ok,"gather should succeed");
        }
        check(bag[item] >= target,"gather target reached");
    };
    auto craft = [&](std::uint8_t recipe) { return sim.action(id,world,Action::Craft,recipe); };
    move_to(NodeKind::Tree); sim.harvest(id,world);
    check(bag[Item::Wood] == 0 && sim.pawn(id)->feedback == Result::NeedAxe,"hands cannot chop trees");
    move_to(NodeKind::Rock); sim.harvest(id,world);
    check(bag[Item::Stone] == 0 && sim.pawn(id)->feedback == Result::NeedPickaxe,"hands cannot mine boulders");
    move_to(NodeKind::IronOre); sim.harvest(id,world);
    check(bag[Item::IronOre] == 0,"hands cannot mine ore");
    const auto empty = bag.count;
    check(craft(1) == Result::Missing && bag.count == empty,"failed recipe consumes nothing");
    gather(NodeKind::Stick,Item::Stick,8);
    gather(NodeKind::Pebble,Item::Stone,4);
    gather(NodeKind::Flint,Item::Flint,8);
    gather(NodeKind::Fiber,Item::Fiber,20);
    check(craft(0) == Result::Ok && craft(1) == Result::Ok,"empty-start hand resources build an axe");
    check(bag.equipped == Item::StoneAxe,"crafted tool equips");
    move_to(NodeKind::Rock); sim.tick(.61f,world); sim.harvest(id,world);
    check(sim.pawn(id)->feedback == Result::NeedPickaxe,"axe cannot mine");
    gather(NodeKind::Tree,Item::Wood,12);
    check(craft(0) == Result::Ok && craft(0) == Result::Ok && craft(2) == Result::Ok,"craft pickaxe");
    gather(NodeKind::Rock,Item::Stone,22);
    const auto stone = bag[Item::Stone]; sim.harvest(id,world);
    check(bag[Item::Stone] == stone,"harvest cooldown blocks spam");
    bag.durability[index(Item::StonePickaxe)] = 1;
    gather(NodeKind::IronOre,Item::IronOre,2);
    sim.tick(.61f,world); sim.harvest(id,world);
    check(sim.pawn(id)->feedback == Result::Broken && bag[Item::IronOre] == 2,"broken pick cannot mine");
    check(craft(0) == Result::Ok,"repair rope");
    check(sim.action(id,world,Action::Repair,static_cast<std::uint8_t>(Item::StonePickaxe)) == Result::Ok,
          "repair consumes flint and rope");
    check(bag.durability[index(Item::StonePickaxe)] == 40,"repair restores durability");
    const auto before_station = bag.count;
    check(craft(8) == Result::NeedStation && bag.count == before_station,"smelting requires nearby furnace");
    gather(NodeKind::Fiber,Item::Fiber,28);
    for (int i=0;i<6;++i) check(craft(0)==Result::Ok,"station rope");
    check(craft(6)==Result::Ok,"craft furnace from mined stone and chopped wood");
    world.player(id)->position = {60,1,60}; world.player(id)->yaw = 0;
    check(sim.action(id,world,Action::Use,static_cast<std::uint8_t>(Item::Furnace)) == Result::Ok,"deploy furnace");
    check(sim.near_station(id,world,Station::Furnace),"deployed furnace enters station range");
    check(craft(8)==Result::Ok && bag[Item::IronIngot]==1,"ore and fuel smelt at furnace");
    world.player(id)->position = {80,1,80};
    check(craft(8)==Result::NeedStation,"leaving station blocks smelting");
    check(sim.action(id,world,Action::Equip,static_cast<std::uint8_t>(Item::StoneAxe))==Result::Ok,"equip axe again");
    gather(NodeKind::Tree,Item::Wood,22);
    check(sim.action(id,world,Action::Equip,static_cast<std::uint8_t>(Item::StonePickaxe))==Result::Ok,"equip pick again");
    gather(NodeKind::Rock,Item::Stone,6);
    gather(NodeKind::IronOre,Item::IronOre,18);
    gather(NodeKind::Stick,Item::Stick,8);
    check(craft(7)==Result::Ok,"craft workbench");
    gather(NodeKind::Fiber,Item::Fiber,16);
    for (int i=0;i<4;++i) check(craft(0)==Result::Ok,"iron tool rope");
    world.player(id)->position={63,1,62};
    check(sim.action(id,world,Action::Use,static_cast<std::uint8_t>(Item::Bench))==Result::Ok,"place bench beside furnace");
    for (int i=0;i<9;++i) check(craft(8)==Result::Ok,"smelt remaining iron");
    check(craft(9)==Result::Ok && craft(10)==Result::Ok,"full progression reaches both iron tools");
    const auto ore_before=bag[Item::IronOre];
    gather(NodeKind::IronOre,Item::IronOre,ore_before+4);
    check(bag[Item::IronOre]==ore_before+4 && bag.durability[index(Item::IronPickaxe)]==99,
          "iron pick doubles yield and consumes one durability");
    check(sim.action(id,world,Action::Equip,static_cast<std::uint8_t>(Item::IronAxe))==Result::Ok,"equip iron axe");
    const auto wood_before=bag[Item::Wood];
    gather(NodeKind::Tree,Item::Wood,wood_before+4);
    check(bag[Item::Wood]==wood_before+4,"iron axe doubles yield");

    Inventory full;
    check(full.add(Item::Stone,90),"45 kg fits exactly");
    check(!full.add(Item::Fiber,1) && full[Item::Fiber]==0,"overweight pickup rejected atomically");
    Inventory capped; capped.count[index(Item::Fiber)]=200;
    check(!capped.add(Item::Fiber,1),"stack cap enforced");
    check(sim.action(id,world,Action::Equip,255)==Result::Invalid,"invalid item id rejected");
    world.player(id)->position={0,1,-4};
    bag[Item::IronIngot]=0;
    sim.try_extract(id,world);
    check(!sim.pawn(id)->extracted,"extract blocked without iron cargo");
    bag[Item::IronIngot]=4;
    sim.try_extract(id,world);
    check(sim.pawn(id)->extracted && bag[Item::IronIngot]==0,"extract spends 4 ingots");
    const auto after_win=bag.count;
    check(craft(0)==Result::Invalid && bag.count==after_win,"no crafting after extraction");
    std::cout << "Economy progression, tools, capacity and station checks passed\n";
}
