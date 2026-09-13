#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace forge::phys {

class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    bool init();
    void add_box(glm::vec3 center, glm::vec3 half_extents);
    void spawn_character(glm::vec3 position, float radius, float half_height);
    bool spawn_vehicle(glm::vec3 position, float yaw_degrees = 0.0f);
    void set_character_enabled(bool enabled);
    void warp_character(glm::vec3 position);
    void set_vehicle_input(float forward, float steer, float brake);
    void tick(float dt, glm::vec3 walk_xz, bool jump);
    glm::vec3 character_position() const;
    bool character_supported() const;
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
