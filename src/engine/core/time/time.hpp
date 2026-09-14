#pragma once

namespace forge::core {

// Simulation and replication are fixed. Rendering and animation follow the
// display; they must never step the authoritative world.
constexpr int kSimHz = 20;
constexpr int kNetHz = 20;
constexpr float kSimDt = 1.0f / static_cast<float>(kSimHz);

inline float clamp_frame_dt(float dt)
{
    if (!(dt > 0.0f)) return kSimDt;
    return dt > 0.1f ? 0.1f : dt;
}

} // namespace forge::core
