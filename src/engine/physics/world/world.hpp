#pragma once

#include <glm/glm.hpp>
#include "engine/core/locomotion/motion.hpp"
#include <cstdint>
#include <memory>

namespace forge::phys {

class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    bool init();
    int add_box(glm::vec3 center, glm::vec3 half_extents, float pitch_degrees = 0);
    int add_crate(glm::vec3 center);
    glm::vec3 box_position(int box) const;
    bool pull_box(int box, int character);
    void remove_box(int box);
    int spawn_character(glm::vec3 position, float radius, float half_height);
    bool set_character_stance(int character, Stance stance);
    Motion character_motion(int character) const;
    void remove_character(int character);
    bool spawn_vehicle(glm::vec3 position, float yaw_degrees = 0.0f);
    void set_character_enabled(bool enabled);
    void set_character_enabled(int character, bool enabled);
    void warp_character(glm::vec3 position);
    void warp_character(int character, glm::vec3 position);
    // ALS StartMantling traces. Starts a short locked climb; the capsule
    // reaches the ledge over kMantleLowSeconds / High / Air.
    bool try_mantle(int character, glm::vec3 wish_dir_xz);
    void set_character_input(int character, glm::vec3 walk_xz, bool jump, float speed = 3.75f);
    // View yaw (camera). Actor yaw is owned by ALS rotation.
    void set_character_facing(int character, float yaw_degrees);
    void set_vehicle_input(float forward, float steer, float brake);
    void tick(float dt, glm::vec3 walk_xz, bool jump);
    void tick(float dt);
    glm::vec3 character_position() const;
    glm::vec3 character_position(int character) const;
    float character_yaw() const;
    float character_yaw(int character) const;
    bool character_supported() const;
    bool character_supported(int character) const;
    glm::mat4 vehicle_transform() const;
    glm::mat4 wheel_transform(int index) const;
    glm::vec3 vehicle_position() const;
    float vehicle_speed() const;
    float vehicle_yaw() const;
    bool has_vehicle() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace forge::phys
