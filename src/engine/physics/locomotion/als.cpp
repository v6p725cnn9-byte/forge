#include "engine/physics/locomotion/als.hpp"

#include <algorithm>
#include <cmath>

namespace forge::phys::als {
namespace {

constexpr float kLn2 = 0.69314718056f;
constexpr float kCcwThreshold = 5.0f;

glm::vec2 clamp_length(glm::vec2 v, float max_length)
{
    const float length = glm::length(v);
    if (length > max_length && length > 0.0f) return v * (max_length / length);
    return v;
}

bool exceeding_max(glm::vec2 velocity, float max_speed, float percent)
{
    max_speed = std::max(0.0f, max_speed);
    return glm::dot(velocity, velocity) > (max_speed * max_speed) * (percent * percent);
}

void apply_velocity_braking(glm::vec2& velocity, float friction, float braking_deceleration, float dt,
                            const Settings& settings)
{
    if (glm::dot(velocity, velocity) < 1e-12f || dt < settings.min_tick) return;
    friction = std::max(0.0f, friction);
    braking_deceleration = std::max(0.0f, braking_deceleration);
    if (friction <= 0.0f && braking_deceleration <= 0.0f) return;
    const glm::vec2 old = velocity;
    float remaining = dt;
    while (remaining >= settings.min_tick) {
        const float step = (remaining > settings.braking_substep && braking_deceleration > 0.0f)
            ? std::min(settings.braking_substep, remaining * 0.5f)
            : remaining;
        remaining -= step;
        const float speed = glm::length(velocity);
        const glm::vec2 dir = speed > 1e-8f ? velocity / speed : glm::vec2{0.0f};
        velocity += (-friction * velocity - braking_deceleration * dir) * step;
        if (glm::dot(velocity, old) <= 0.0f) {
            velocity = {0.0f, 0.0f};
            return;
        }
    }
    if (glm::dot(velocity, velocity) <= settings.brake_to_stop * settings.brake_to_stop) velocity = {0.0f, 0.0f};
}

glm::vec2 calc_velocity(glm::vec2 velocity, glm::vec2 acceleration, float max_acceleration, float max_speed,
                        float friction, float braking_deceleration, float braking_friction_factor, float dt,
                        const Settings& settings)
{
    if (dt < settings.min_tick) return velocity;
    friction = std::max(0.0f, friction);
    const float analog = glm::length(acceleration);
    const float analog_modifier =
        analog > 1e-8f && max_acceleration > 1e-8f ? std::clamp(analog / max_acceleration, 0.0f, 1.0f) : 0.0f;
    const float max_input_speed = analog_modifier > 0.0f ? std::max(max_speed * analog_modifier, settings.min_analog)
                                                         : 0.0f;
    const bool zero_accel = analog <= 1e-8f;
    if (zero_accel) {
        apply_velocity_braking(velocity, friction * braking_friction_factor, braking_deceleration, dt, settings);
        return velocity;
    }
    if (!exceeding_max(velocity, max_speed, settings.exceeding)) {
        const glm::vec2 accel_dir = acceleration / analog;
        const float vel_size = glm::length(velocity);
        velocity -= (velocity - accel_dir * vel_size) * std::min(dt * friction, 1.0f);
    }
    const float new_max = exceeding_max(velocity, max_input_speed, settings.exceeding) ? glm::length(velocity)
                                                                                       : max_input_speed;
    velocity += acceleration * dt;
    return clamp_length(velocity, new_max);
}

float yaw_from_dir(glm::vec2 dir)
{
    if (glm::dot(dir, dir) < 1e-12f) return 0.0f;
    return glm::degrees(std::atan2(dir.x, dir.y));
}

void set_target_yaw(State& state, float yaw)
{
    state.target_yaw = unwind_degrees(yaw);
    state.smooth_target_yaw = state.target_yaw;
    state.target_yaw_view_space = unwind_degrees(state.view_yaw - state.target_yaw);
}

void set_target_yaw_smooth(State& state, float yaw, float dt, float speed)
{
    state.target_yaw = unwind_degrees(yaw);
    state.smooth_target_yaw = interpolate_angle_constant(state.smooth_target_yaw, state.target_yaw, dt, speed);
    state.target_yaw_view_space = unwind_degrees(state.view_yaw - state.target_yaw);
}

void apply_actor_yaw(State& state, float dt, float half_life)
{
    state.actor_yaw = damper_exact_angle(unwind_degrees(state.actor_yaw), state.smooth_target_yaw, dt, half_life);
}

void rotation_extra_smooth(State& state, float target, float dt, float half_life, float target_speed)
{
    set_target_yaw_smooth(state, target, dt, target_speed);
    apply_actor_yaw(state, dt, half_life);
}

void rotation_smooth(State& state, float target, float dt, float half_life)
{
    set_target_yaw(state, target);
    apply_actor_yaw(state, dt, half_life);
}

void refresh_target_from_actor(State& state) { set_target_yaw(state, state.actor_yaw); }

void refresh_grounded_rotation(State& state, const Settings& settings, float dt)
{
    if (!state.grounded) return;
    if (!state.moving) {
        if (settings.rotation_mode == RotationMode::VelocityDirection) {
            const float target = state.rotation_blocked ? state.target_yaw
                : (settings.rotate_to_desired_velocity ? state.desired_velocity_yaw : state.velocity_yaw);
            rotation_extra_smooth(state, target, dt, settings.idle_velocity_half_life, settings.extra_smooth_velocity);
            return;
        }
        if (settings.rotation_mode == RotationMode::ViewDirection) {
            if ((!state.has_input && state.rotation_blocked) || !settings.auto_rotate_idle_input) {
                refresh_target_from_actor(state);
                return;
            }
            const float target = state.has_input ? state.view_yaw : state.target_yaw;
            const float half = rotation_half_life(state.gait_amount, settings)
                * (1.0f + (settings.min_half_life_scale - 1.0f)
                    * std::clamp(state.view_yaw_speed / settings.view_yaw_speed_ref, 0.0f, 1.0f));
            rotation_extra_smooth(state, target, dt, half, settings.extra_smooth_view);
            return;
        }
        refresh_target_from_actor(state);
        return;
    }

    if (settings.rotation_mode == RotationMode::VelocityDirection
        && (state.has_input || !state.rotation_blocked)) {
        state.rotation_blocked = false;
        const float target = settings.rotate_to_desired_velocity ? state.desired_velocity_yaw : state.velocity_yaw;
        const float half = rotation_half_life(state.gait_amount, settings)
            * (1.0f + (settings.min_half_life_scale - 1.0f)
                * std::clamp(state.view_yaw_speed / settings.view_yaw_speed_ref, 0.0f, 1.0f));
        rotation_extra_smooth(state, target, dt, half, settings.extra_smooth_velocity);
        return;
    }
    if (settings.rotation_mode == RotationMode::ViewDirection
        && (state.has_input || !state.rotation_blocked)) {
        state.rotation_blocked = false;
        const bool sprinting = state.gait_amount >= 2.5f;
        const float target = sprinting ? state.velocity_yaw : state.view_yaw;
        const float half = rotation_half_life(state.gait_amount, settings)
            * (1.0f + (settings.min_half_life_scale - 1.0f)
                * std::clamp(state.view_yaw_speed / settings.view_yaw_speed_ref, 0.0f, 1.0f));
        rotation_extra_smooth(state, target, dt, half, sprinting ? settings.extra_smooth_velocity
                                                                : settings.extra_smooth_view);
        return;
    }
    refresh_target_from_actor(state);
}

void refresh_in_air_rotation(State& state, const Settings& settings, float dt)
{
    if (state.grounded) return;
    if (settings.rotation_mode == RotationMode::VelocityDirection
        || settings.rotation_mode == RotationMode::ViewDirection) {
        if (settings.in_air_rotation == InAirRotation::KeepViewSpaceRotation) {
            rotation_smooth(state, unwind_degrees(state.view_yaw - state.target_yaw_view_space), dt,
                            settings.in_air_half_life);
            return;
        }
        if (settings.in_air_rotation == InAirRotation::RotateToVelocity && state.moving) {
            rotation_smooth(state, state.velocity_yaw, dt, settings.in_air_half_life);
            return;
        }
    }
    refresh_target_from_actor(state);
}

} // namespace

float unwind_degrees(float angle)
{
    angle = std::fmod(angle + 180.0f, 360.0f);
    if (angle < 0.0f) angle += 360.0f;
    return angle - 180.0f;
}

float remap_ccw(float delta)
{
    if (delta > 180.0f - kCcwThreshold) return delta - 360.0f;
    return delta;
}

float damper_exact_alpha(float dt, float half_life)
{
    if (dt <= 0.0f) return 0.0f;
    return 1.0f - std::exp(-kLn2 / (half_life + 1e-8f) * dt);
}

float damper_exact_angle(float current, float target, float dt, float half_life)
{
    float delta = unwind_degrees(target - current);
    if (std::abs(delta) < 1e-4f) return unwind_degrees(target);
    delta = remap_ccw(delta);
    return unwind_degrees(current + delta * damper_exact_alpha(dt, half_life));
}

float interpolate_angle_constant(float current, float target, float dt, float speed)
{
    float delta = unwind_degrees(target - current);
    const float max_delta = speed * dt;
    if (speed <= 0.0f || std::abs(delta) <= max_delta) return unwind_degrees(target);
    delta = remap_ccw(delta);
    return unwind_degrees(current + (delta >= 0.0f ? 1.0f : -1.0f) * max_delta);
}

glm::vec3 friction_curve(float gait_amount)
{
    gait_amount = std::clamp(gait_amount, 0.0f, 3.0f);
    const glm::vec3 keys[] = {
        {20.00f, 15.00f, 5.00f},
        {20.00f, 15.00f, 5.00f},
        {20.00f, 12.50f, 4.00f},
        {7.50f, 5.00f, 0.50f},
    };
    const int index = std::min(2, static_cast<int>(gait_amount));
    const float t = gait_amount - static_cast<float>(index);
    return glm::mix(keys[index], keys[index + 1], t);
}

float gait_amount(float speed, const Settings& settings)
{
    if (speed > settings.run_forward) {
        const float span = std::max(settings.sprint - settings.run_forward, 1e-4f);
        return std::clamp(2.0f + (speed - settings.run_forward) / span, 2.0f, 3.0f);
    }
    if (speed > settings.walk_forward) {
        const float span = std::max(settings.run_forward - settings.walk_forward, 1e-4f);
        return std::clamp(1.0f + (speed - settings.walk_forward) / span, 1.0f, 2.0f);
    }
    return std::clamp(speed / std::max(settings.walk_forward, 1e-4f), 0.0f, 1.0f);
}

float rotation_half_life(float gait_amount, const Settings& settings)
{
    // CF_Als_RotationInterpolationSpeed_Normal: 1..2 = 0.15 s, 3 = 0.05 s.
    const float amount = std::max(1.0f, gait_amount);
    if (amount <= 2.0f) return settings.rotation_half_life_walk;
    return glm::mix(settings.rotation_half_life_walk, settings.rotation_half_life_sprint, amount - 2.0f);
}

Gait max_allowed_gait(float max_speed, const Settings& settings)
{
    if (max_speed + 0.05f < settings.run_forward) return Gait::Walking;
    if (max_speed + 0.05f < settings.sprint) return Gait::Running;
    return Gait::Sprinting;
}

float max_walk_speed(Gait gait, const Settings& settings)
{
    switch (gait) {
    case Gait::Walking: return settings.walk_forward;
    case Gait::Sprinting: return settings.sprint;
    case Gait::Running:
    default: return settings.run_forward;
    }
}

State tick(State state, glm::vec3 walk_xz, float view_yaw_degrees, bool jump, bool grounded, float max_speed,
           float dt, const Settings& settings)
{
    if (dt < settings.min_tick) return state;
    glm::vec2 wish{walk_xz.x, walk_xz.z};
    float wish_len = glm::length(wish);
    if (wish_len > 1.0f) {
        wish /= wish_len;
        wish_len = 1.0f;
    }
    const float cap = std::max(0.0f, max_speed);
    if (cap <= 0.0f) {
        wish = {0.0f, 0.0f};
        wish_len = 0.0f;
    }
    const float input = wish_len;
    const glm::vec2 wish_dir = input > 1e-6f ? wish / std::max(wish_len, 1e-6f) : glm::vec2{0.0f};

    if (!state.yaw_initialized) {
        state.actor_yaw = view_yaw_degrees;
        state.target_yaw = view_yaw_degrees;
        state.smooth_target_yaw = view_yaw_degrees;
        state.previous_view_yaw = view_yaw_degrees;
        state.yaw_initialized = true;
    }

    const float view = unwind_degrees(view_yaw_degrees);
    state.view_yaw_speed = std::abs(unwind_degrees(view - state.previous_view_yaw)) / dt;
    state.previous_view_yaw = view;
    state.view_yaw = view;

    state.has_input = input > 1e-4f;
    if (state.has_input) state.input_yaw = yaw_from_dir(wish_dir);

    const bool landed = grounded && !state.was_grounded;
    state.was_grounded = state.grounded;
    state.grounded = grounded;
    if (landed) {
        state.braking_friction_factor =
            state.has_input ? settings.land_friction_with_input : settings.land_friction_no_input;
        state.land_friction_left = settings.land_friction_seconds;
        state.rotation_blocked = true;
    }
    if (state.land_friction_left > 0.0f) {
        state.land_friction_left = std::max(0.0f, state.land_friction_left - dt);
        if (state.land_friction_left <= 0.0f) state.braking_friction_factor = settings.braking_friction_factor;
    }

    const float speed = glm::length(state.planar);
    state.gait_amount = gait_amount(speed, settings);
    const glm::vec3 curve = friction_curve(state.gait_amount);
    const float max_accel_walk = curve.x;
    const float braking = curve.y;
    const float friction = curve.z;
    const float friction_factor = state.braking_friction_factor;

    if (grounded) {
        const glm::vec2 acceleration = wish_dir * (input * max_accel_walk);
        state.planar = calc_velocity(state.planar, acceleration, max_accel_walk, cap, friction, braking,
                                     friction_factor, dt, settings);
        state.vertical = 0.0f;
        if (jump) state.vertical = settings.jump_z;
    } else {
        state.vertical += -settings.gravity * dt;
        if (state.vertical < settings.terminal_velocity) state.vertical = settings.terminal_velocity;
        float air = settings.air_control;
        if (air > 0.0f && glm::length(state.planar) < settings.air_boost_threshold)
            air = std::min(1.0f, air * settings.air_boost_multiplier);
        const glm::vec2 fall_accel = wish_dir * (input * settings.max_acceleration_air * air);
        const glm::vec2 previous = state.planar;
        glm::vec2 next = previous + fall_accel * dt;
        const float prev_speed = glm::length(previous);
        const float next_speed = glm::length(next);
        if (prev_speed >= cap - 1e-4f && next_speed > prev_speed && prev_speed > 1e-6f)
            next *= prev_speed / next_speed;
        else if (next_speed > cap && cap > 0.0f) next *= cap / next_speed;
        state.planar = next;
    }

    const float planar_speed = glm::length(state.planar);
    const bool had_velocity = state.has_velocity;
    state.has_velocity = planar_speed >= settings.has_speed;
    if (state.has_velocity) state.velocity_yaw = yaw_from_dir(state.planar);
    if (state.has_velocity && !had_velocity) state.desired_velocity_yaw = state.velocity_yaw;
    else if (state.has_velocity) state.desired_velocity_yaw = state.velocity_yaw;
    state.moving = (state.has_input && state.has_velocity) || planar_speed > settings.moving_speed;

    refresh_grounded_rotation(state, settings, dt);
    refresh_in_air_rotation(state, settings, dt);
    return state;
}

} // namespace forge::phys::als
