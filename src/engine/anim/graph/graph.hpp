#pragma once

#include "engine/anim/skeleton/animator.hpp"

namespace forge::anim {

enum class Locomotion { Idle, Walk, Run };

inline Locomotion select_locomotion(float speed_mps, float walk_threshold = 0.4f, float run_threshold = 2.6f)
{
    if (speed_mps < walk_threshold) return Locomotion::Idle;
    if (speed_mps < run_threshold) return Locomotion::Walk;
    return Locomotion::Run;
}

} // namespace forge::anim
