#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace forge {

enum class Stance : std::uint8_t { Standing, Crouched, Prone };

// Authoritative motion samples; positions remain feet + one metre in every stance.
struct Motion {
    glm::vec3 velocity{0};
    float view_yaw = 0;
    float pitch = 0;
    float left_ground = 0;
    float right_ground = 0;
    float mantle = 0;
    float impact = 0;
    Stance stance = Stance::Standing;
    bool grounded = true;
    bool mantling = false;
    bool interacting = false;
    bool pushing = false;
    bool pulling = false;
    std::uint8_t stamina = 100;
    std::uint8_t health = 100;
};

inline float stance_height(Stance stance)
{
    return stance == Stance::Prone ? 0.6f : stance == Stance::Crouched ? 1.2f : 1.8f;
}
inline float stance_speed(Stance stance)
{
    return stance == Stance::Prone ? 0.65f : stance == Stance::Crouched ? 1.35f : 3.75f;
}

} // namespace forge
