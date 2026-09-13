#pragma once

#include <glm/glm.hpp>

namespace forge {

class Camera {
public:
    glm::vec3 position{9.0f, 7.0f, 13.0f};
    float yaw = -125.0f;
    float pitch = -22.0f;
    float speed = 6.0f;
    float sensitivity = 0.12f;
    float vertical_fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 500.0f;

    glm::vec3 forward() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
    void look(float dx, float dy);
    void look_at(const glm::vec3& target);
    void frame(const glm::vec3& bounds_min, const glm::vec3& bounds_max);
    void move(glm::vec3 local_direction, float delta_seconds, bool boost);
};

} // namespace forge
