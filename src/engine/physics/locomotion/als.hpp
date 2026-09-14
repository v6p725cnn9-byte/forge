#pragma once

#include <glm/glm.hpp>

namespace forge::phys::als {

// Inspired by / based on Sixze/ALS-Refactored (MIT) concepts:
// gait, CMC velocity, actor rotation, mantling traces.
// Standalone C++20 implementation. No Unreal dependency, no AnimBP, no
// SKM_Als. Collision stays Jolt CharacterVirtual. Mixamo mannequin and
// clips are separate assets with their own licenses — MIT on the ALS
// sources does not cover those files.

enum class Gait { Walking, Running, Sprinting };
enum class RotationMode { ViewDirection, VelocityDirection, Aiming };
enum class InAirRotation { RotateToVelocity, KeepViewSpaceRotation };
enum class MantleType { Low, High, InAir };

struct Settings {
    float walk_forward = 1.75f;
    float run_forward = 3.75f;
    float sprint = 6.50f;
    float min_analog = 0.25f;
    float max_acceleration_air = 20.0f;
    float air_control = 0.15f;
    float air_boost_multiplier = 2.0f;
    float air_boost_threshold = 0.25f;
    float jump_z = 4.20f;
    float gravity = 9.80f;
    float terminal_velocity = -40.0f;
    float braking_friction_factor = 0.0f;
    float brake_to_stop = 0.10f;
    float braking_substep = 1.0f / 33.0f;
    float min_tick = 1e-6f;
    float exceeding = 1.01f;
    float moving_speed = 0.50f;
    float has_speed = 0.01f;
    float land_friction_with_input = 0.5f;
    float land_friction_no_input = 3.0f;
    float land_friction_seconds = 0.5f;
    float rotation_half_life_walk = 0.15f;
    float rotation_half_life_sprint = 0.05f;
    float view_yaw_speed_ref = 300.0f;
    float min_half_life_scale = 1.0f / 3.0f;
    float extra_smooth_view = 500.0f;
    float extra_smooth_velocity = 800.0f;
    float in_air_half_life = 0.20f;
    float idle_velocity_half_life = 0.10f;
    RotationMode rotation_mode = RotationMode::ViewDirection;
    InAirRotation in_air_rotation = InAirRotation::RotateToVelocity;
    bool auto_rotate_idle_input = true;
    bool rotate_to_desired_velocity = true;
};

struct State {
    glm::vec2 planar{0.0f};
    float vertical = 0.0f;
    float gait_amount = 0.0f;
    float actor_yaw = 0.0f;
    float view_yaw = 0.0f;
    float previous_view_yaw = 0.0f;
    float view_yaw_speed = 0.0f;
    float target_yaw = 0.0f;
    float smooth_target_yaw = 0.0f;
    float target_yaw_view_space = 0.0f;
    float velocity_yaw = 0.0f;
    float input_yaw = 0.0f;
    float desired_velocity_yaw = 0.0f;
    float braking_friction_factor = 0.0f;
    float land_friction_left = 0.0f;
    bool has_input = false;
    bool has_velocity = false;
    bool moving = false;
    bool grounded = true;
    bool was_grounded = true;
    bool rotation_blocked = false;
    bool yaw_initialized = false;
};

glm::vec3 friction_curve(float gait_amount);
float gait_amount(float speed, const Settings& settings);
Gait max_allowed_gait(float max_speed, const Settings& settings);
float max_walk_speed(Gait gait, const Settings& settings);
float rotation_half_life(float gait_amount, const Settings& settings = {});

float unwind_degrees(float angle);
float remap_ccw(float delta);
float damper_exact_alpha(float dt, float half_life);
float damper_exact_angle(float current, float target, float dt, float half_life);
float interpolate_angle_constant(float current, float target, float dt, float speed);

State tick(State state, glm::vec3 walk_xz, float view_yaw_degrees, bool jump, bool grounded, float max_speed,
           float dt, const Settings& settings = {});

constexpr float kMantleMinRise = 0.50f;
constexpr float kMantleMaxRise = 2.25f;
constexpr float kMantleMaxRiseAir = 1.50f;
constexpr float kMantleReach = 0.75f;
constexpr float kMantleReachAir = 0.70f;
constexpr float kMantleTargetOffset = 0.15f;
constexpr float kMantleStartOffset = 0.55f;
constexpr float kMantleHigh = 1.25f;
constexpr float kMantleTraceAngle = 110.0f;
constexpr float kMantleMaxReachAngle = 50.0f;
constexpr float kMantleSlopeCos = 0.8191520443f; // cos 35 deg
constexpr float kMantleCooldown = 0.60f;
constexpr float kMantleLowSeconds = 0.45f;
constexpr float kMantleHighSeconds = 0.70f;
constexpr float kMantleAirSeconds = 0.55f;

constexpr float kCapsuleRadius = 0.30f;
constexpr float kCapsuleHalfHeight = 0.90f;
constexpr float kCylinderHalfHeight = 0.60f;
constexpr float kMass = 100.0f;
constexpr float kMaxSlopeDegrees = 44.773f;
constexpr float kStepUp = 0.45f;

} // namespace forge::phys::als
