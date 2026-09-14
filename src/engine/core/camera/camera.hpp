#pragma once

#include <glm/glm.hpp>

namespace forge {

enum class CameraPerson {
    Third,
    First,
};

class Camera {
public:
    glm::vec3 position{9.0f, 7.0f, 13.0f};
    CameraPerson person = CameraPerson::Third;
    float yaw = -125.0f;
    float pitch = -22.0f;
    float speed = 6.0f;
    float sensitivity = 0.12f;
    bool invert_y = false;
    float vertical_fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 500.0f;

    bool is_first_person() const { return person == CameraPerson::First; }

    // Compass yaw (degrees) of the horizontal view direction in the pawn
    // convention: forward = (sin(yaw), 0, cos(yaw)). Bodies use this so the
    // torso faces the camera while the legs strafe or backpedal.
    float facing_yaw() const;

    glm::vec3 forward() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
    void look(float dx, float dy);
    void look_at(const glm::vec3& target);
    void frame(const glm::vec3& bounds_min, const glm::vec3& bounds_max);
    void move(glm::vec3 local_direction, float delta_seconds, bool boost);
};

} // namespace forge
