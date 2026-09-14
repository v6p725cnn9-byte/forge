#pragma once

namespace forge::core {

// Sim tick and net send tick are independent knobs that currently share a
// value. Rendering and animation follow the display and never step the world.
constexpr int kSimHz = 20;
constexpr int kNetHz = 20;
constexpr float kSimDt = 1.0f / static_cast<float>(kSimHz);

inline float clamp_frame_dt(float dt)
{
    if (!(dt > 0.0f)) return kSimDt;
    return dt > 0.1f ? 0.1f : dt;
}

} // namespace forge::core
