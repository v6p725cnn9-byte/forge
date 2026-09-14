#include "engine/physics/locomotion/als.hpp"
#include "engine/physics/world/world.hpp"

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

constexpr float kStep = 1.0f / 60.0f;
constexpr float kRadius = forge::phys::als::kCapsuleRadius;
constexpr float kCylinder = forge::phys::als::kCylinderHalfHeight;
constexpr float kRestY = kCylinder + kRadius;
constexpr float kRun = 3.75f;
constexpr float kSprint = 6.50f;
} // namespace

int main()
{
    using forge::phys::als::friction_curve;
    using forge::phys::als::gait_amount;
    forge::phys::als::Settings settings;
    check(std::abs(friction_curve(0.0f).x - 20.0f) < 0.01f, "ALS idle accel 2000 cm/s^2");
    check(std::abs(friction_curve(3.0f).x - 7.5f) < 0.01f, "ALS sprint accel 750 cm/s^2");
    check(std::abs(friction_curve(3.0f).z - 0.5f) < 0.01f, "ALS sprint friction 0.5");
    check(std::abs(gait_amount(0.0f, settings) - 0.0f) < 0.01f, "gait 0 at rest");
    check(std::abs(gait_amount(1.75f, settings) - 1.0f) < 0.01f, "gait 1 at walk");
    check(std::abs(gait_amount(3.75f, settings) - 2.0f) < 0.01f, "gait 2 at run");
    check(std::abs(gait_amount(6.50f, settings) - 3.0f) < 0.01f, "gait 3 at sprint");

    forge::phys::World world;
    check(world.init(), "physics init");
    check(world.add_box({0.0f, -0.5f, 0.0f}, {200.0f, 0.5f, 200.0f}) == 0, "ground handle");
    const int hero = world.spawn_character({0.0f, 2.0f, 0.0f}, kRadius, kCylinder);
    check(hero == 0, "first character handle");

    for (int i = 0; i < 120; ++i) world.tick(kStep, {0.0f, 0.0f, 0.0f}, false);
    const glm::vec3 rest = world.character_position(hero);
    check(std::abs(rest.x) < 0.05f && std::abs(rest.z) < 0.05f, "no drift without input");
    check(std::abs(rest.y - kRestY) < 0.1f, "capsule settles on the ground plane");
    check(world.character_supported(), "grounded character reports support");

    world.tick(kStep, {0.0f, 0.0f, 0.0f}, true);
    float peak = rest.y;
    for (int i = 0; i < 150; ++i) {
        world.tick(kStep, {0.0f, 0.0f, 0.0f}, false);
        peak = std::max(peak, world.character_position(hero).y);
    }
    check(peak > rest.y + 0.6f, "ALS jump (4.2 m/s) gains height");
    const float landed = world.character_position(hero).y;
    check(std::abs(landed - rest.y) < 0.25f, "character lands back on the ground");

    check(world.add_box({5.0f, 2.0f, 0.0f}, {0.5f, 2.0f, 4.0f}) == 1, "wall handle");
    for (int i = 0; i < 240; ++i) world.tick(kStep, {1.0f, 0.0f, 0.0f}, false);
    const glm::vec3 blocked = world.character_position(hero);
    check(blocked.x > 2.0f, "character advances until the wall");
    check(blocked.x < 4.5f - kRadius + 0.12f, "wall blocks the character");
    check(std::abs(blocked.z) < 0.1f, "wall does not deflect sideways");
    check(std::abs(blocked.y - rest.y) < 0.25f, "wall does not lift the character");

    world.tick(kStep, {0.0f, 0.0f, 0.0f}, true);
    float wall_peak = blocked.y;
    for (int i = 0; i < 150; ++i) {
        world.tick(kStep, {0.0f, 0.0f, 0.0f}, false);
        wall_peak = std::max(wall_peak, world.character_position(hero).y);
    }
    check(wall_peak > blocked.y + 0.3f, "jump arcs even when hugging the wall");
    check(std::abs(world.character_position(hero).y - blocked.y) < 0.25f, "wall jump slides back down");

    const int friend_ = world.spawn_character({-8.0f, 1.0f, -8.0f}, kRadius, kCylinder);
    check(friend_ == 1, "second character handle");
    world.set_character_input(hero, {0.0f, 0.0f, 0.0f}, false);
    world.set_character_input(friend_, {0.0f, 0.0f, 0.0f}, false);
    for (int i = 0; i < 60; ++i) world.tick(kStep);
    world.set_character_input(friend_, {-1.0f, 0.0f, 0.0f}, false, kRun);
    for (int i = 0; i < 60; ++i) world.tick(kStep);
    const glm::vec3 hero_stays = world.character_position(hero);
    const glm::vec3 friend_moves = world.character_position(friend_);
    check(std::abs(hero_stays.x - blocked.x) < 0.1f, "idle character stays while another walks");
    check(friend_moves.x < -10.0f, "second character walks on its own input");
    world.remove_character(friend_);
    const glm::vec3 gone = world.character_position(friend_);
    check(gone == glm::vec3{0.0f}, "removed character reads as zero");

    const int runner = world.spawn_character({-10.0f, 2.0f, -60.0f}, kRadius, kCylinder);
    check(runner == 2, "third character handle");
    world.set_character_facing(runner, 0.0f);
    world.set_character_input(runner, {0.0f, 0.0f, 0.0f}, false);
    for (int i = 0; i < 120; ++i) world.tick(kStep);
    auto steady_speed = [&](glm::vec3 dir, float top) {
        world.set_character_input(runner, {0.0f, 0.0f, 0.0f}, false);
        for (int i = 0; i < 60; ++i) world.tick(kStep);
        world.set_character_input(runner, dir, false, top);
        for (int i = 0; i < 180; ++i) world.tick(kStep);
        const glm::vec3 a = world.character_position(runner);
        for (int i = 0; i < 120; ++i) world.tick(kStep);
        const glm::vec3 b = world.character_position(runner);
        return glm::length(glm::vec2(b.x - a.x, b.z - a.z)) / 2.0f;
    };
    const float forward = steady_speed({0.0f, 0.0f, 1.0f}, kRun);
    check(std::abs(forward - kRun) < 0.40f, "ALS run tops at 3.75 m/s");
    const float back = steady_speed({0.0f, 0.0f, -1.0f}, kRun);
    check(std::abs(back - kRun) < 0.40f, "ALS default has no backpedal penalty");
    const float strafe = steady_speed({1.0f, 0.0f, 0.0f}, kRun);
    check(std::abs(strafe - kRun) < 0.40f, "ALS default has no strafe penalty");
    const float sprint = steady_speed({0.0f, 0.0f, 1.0f}, kSprint);
    check(std::abs(sprint - kSprint) < 0.55f, "ALS sprint tops at 6.50 m/s");

    world.set_character_input(runner, {0.0f, 0.0f, 0.0f}, false);
    world.set_character_facing(runner, 0.0f);
    for (int i = 0; i < 60; ++i) world.tick(kStep);
    world.set_character_facing(runner, 90.0f);
    for (int i = 0; i < 30; ++i) world.tick(kStep);
    check(std::abs(world.character_yaw(runner)) < 20.0f, "idle ViewDirection does not follow the camera");
    world.set_character_input(runner, {0.0f, 0.0f, 1.0f}, false, kRun);
    for (int i = 0; i < 90; ++i) world.tick(kStep);
    check(world.character_yaw(runner) > 50.0f, "moving ViewDirection damps the actor toward the camera");
    world.set_character_input(runner, {0.0f, 0.0f, 0.0f}, false);
    world.set_character_facing(runner, 0.0f);
    for (int i = 0; i < 120; ++i) world.tick(kStep);

    world.set_character_input(runner, {0.0f, 0.0f, 0.0f}, false);
    for (int i = 0; i < 60; ++i) world.tick(kStep);
    const glm::vec3 still = world.character_position(runner);
    world.set_character_input(runner, {0.0f, 0.0f, 1.0f}, false, kRun);
    for (int i = 0; i < 12; ++i) world.tick(kStep);
    const glm::vec3 early = world.character_position(runner);
    const float start_dist = glm::length(glm::vec2(early.x - still.x, early.z - still.z));
    check(start_dist > 0.28f, "ALS 20 m/s^2 launch covers distance in 0.2 s");

    for (int i = 0; i < 180; ++i) world.tick(kStep);
    world.set_character_input(runner, {0.0f, 0.0f, 0.0f}, false);
    const glm::vec3 fast = world.character_position(runner);
    for (int i = 0; i < 18; ++i) world.tick(kStep);
    const glm::vec3 slowing = world.character_position(runner);
    const float stop_dist = glm::length(glm::vec2(slowing.x - fast.x, slowing.z - fast.z));
    check(stop_dist > 0.20f && stop_dist < 1.8f, "ALS braking deceleration spans several steps");

    for (int i = 0; i < 180; ++i) world.tick(kStep);
    world.set_character_input(runner, {0.0f, 0.0f, 1.0f}, false, kRun);
    for (int i = 0; i < 180; ++i) world.tick(kStep);
    world.set_character_input(runner, {0.0f, 0.0f, -1.0f}, true, kRun);
    world.tick(kStep);
    const float takeoff = world.character_position(runner).z;
    world.set_character_input(runner, {0.0f, 0.0f, -1.0f}, false, kRun);
    for (int i = 0; i < 36; ++i) world.tick(kStep);
    const float flight = world.character_position(runner).z;
    for (int i = 0; i < 48; ++i) world.tick(kStep);
    check(flight - takeoff > 0.8f, "ALS 0.15 air control cannot reverse a run in one jump");
    check(world.character_supported(), "flight ends back on the ground");

    const int climber = world.spawn_character({-40.0f, 2.0f, 20.0f}, kRadius, kCylinder);
    world.set_character_facing(climber, 0.0f);
    world.set_character_input(climber, {0.0f, 0.0f, 0.0f}, false);
    for (int i = 0; i < 120; ++i) world.tick(kStep);
    check(world.add_box({-40.0f, 0.5f, 25.0f}, {2.0f, 0.5f, 1.5f}) == 2, "ledge handle");
    world.set_character_input(climber, {0.0f, 0.0f, 1.0f}, false, kRun);
    for (int i = 0; i < 240; ++i) world.tick(kStep);
    const glm::vec3 stuck = world.character_position(climber);
    check(stuck.z < 23.5f - kRadius + 0.12f, "ledge blocks like a wall");
    check(world.try_mantle(climber, {0.0f, 0.0f, 1.0f}), "vaultable ledge mantles");
    world.warp_character(climber, stuck);
    check(!world.try_mantle(climber, {0.0f, 0.0f, 1.0f}), "mantle cooldown blocks repeats");
    for (int i = 0; i < 60; ++i) world.tick(kStep);
    check(world.try_mantle(climber, {0.0f, 0.0f, 1.0f}), "mantle works again after cooldown");
    world.set_character_input(climber, {0.0f, 0.0f, 0.0f}, false);
    for (int i = 0; i < 45; ++i) world.tick(kStep);
    const float mantled = world.character_position(climber).y;
    check(std::abs(mantled - (1.0f + kRestY)) < 0.15f, "mantle lands on top of the ledge");
    check(std::abs(world.character_position(climber).y - mantled) < 0.2f, "mantled capsule settles on top");

    const int lowland = world.spawn_character({-40.0f, 2.0f, 34.0f}, kRadius, kCylinder);
    world.set_character_facing(lowland, 0.0f);
    world.set_character_input(lowland, {0.0f, 0.0f, 0.0f}, false);
    for (int i = 0; i < 120; ++i) world.tick(kStep);
    check(world.add_box({-40.0f, 1.5f, 40.0f}, {2.0f, 1.5f, 0.5f}) == 3, "tower handle");
    world.set_character_input(lowland, {0.0f, 0.0f, 1.0f}, false, kRun);
    for (int i = 0; i < 240; ++i) world.tick(kStep);
    check(!world.try_mantle(lowland, {0.0f, 0.0f, 1.0f}), "a 3 m wall refuses the vault");
    check(!world.try_mantle(lowland, {0.0f, 0.0f, -1.0f}), "open ground refuses the vault");
    check(!world.try_mantle(99, {0.0f, 0.0f, 1.0f}), "missing body refuses the vault");

    std::cout << "Phys checks passed\n";
}
