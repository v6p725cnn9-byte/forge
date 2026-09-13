#include "engine/core/camera.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace forge {

glm::vec3 Camera::forward() const
{
    const float y = glm::radians(yaw);
    const float p = glm::radians(pitch);
    return {std::cos(y) * std::cos(p), std::sin(p), std::sin(y) * std::cos(p)};
}

glm::mat4 Camera::view() const
{
    return glm::lookAtRH(position, position + forward(), glm::vec3{0.0f, 1.0f, 0.0f});
}

glm::mat4 Camera::projection(float aspect) const
{
    // SDL GPU uses [0, 1] clip depth on every backend. World is right-handed, Y-up.
    return glm::perspectiveRH_ZO(glm::radians(vertical_fov), aspect, near_plane, far_plane);
}

void Camera::look(float dx, float dy)
{
    yaw = std::remainder(yaw + dx * sensitivity, 360.0f);
    pitch = std::clamp(pitch - dy * sensitivity, -89.0f, 89.0f);
}

void Camera::look_at(const glm::vec3& target)
{
    const glm::vec3 delta = target - position;
    const float length = glm::length(delta);
    if (length < 1e-8f) return;
    const glm::vec3 direction = delta / length;
    pitch = std::clamp(glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f))), -89.0f, 89.0f);
    yaw = glm::degrees(std::atan2(direction.z, direction.x));
}

void Camera::frame(const glm::vec3& bounds_min, const glm::vec3& bounds_max)
{
    const glm::vec3 center = (bounds_min + bounds_max) * 0.5f;
    const float radius = std::max(glm::length(bounds_max - bounds_min) * 0.5f, 0.05f);
    position = center + glm::normalize(glm::vec3{0.9f, 0.45f, 1.15f}) * radius * 3.4f;
    look_at(center);
    near_plane = std::max(radius * 0.02f, 0.01f);
    far_plane = std::max(radius * 40.0f, 25.0f);
    speed = std::max(radius * 1.4f, 0.35f);
}

void Camera::move(glm::vec3 local_direction, float delta_seconds, bool boost)
{
    const float length = glm::length(local_direction);
    if (length == 0.0f || delta_seconds <= 0.0f) {
        return;
    }
    const glm::vec3 front = forward();
    const glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3{0.0f, 1.0f, 0.0f}));
    const glm::vec3 direction = right * local_direction.x + glm::vec3{0.0f, local_direction.y, 0.0f}
                               + front * local_direction.z;
    position += glm::normalize(direction) * std::min(length, 1.0f)
                * speed * (boost ? 4.0f : 1.0f) * delta_seconds;
}

} // namespace forge
