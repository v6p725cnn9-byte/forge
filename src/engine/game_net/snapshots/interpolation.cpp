#include "engine/game_net/snapshots/interpolation.hpp"

#include <algorithm>
#include <cmath>

namespace forge::net {
namespace {

Ghost lerp_ghost(const Ghost& a, const Ghost& b, float t)
{
    Ghost out = b;
    out.position = a.position + (b.position - a.position) * t;
    float dyaw = b.yaw - a.yaw;
    while (dyaw > 180.0f) dyaw -= 360.0f;
    while (dyaw < -180.0f) dyaw += 360.0f;
    out.yaw = a.yaw + dyaw * t;
    return out;
}

} // namespace

void Interpolation::push(const Snapshot& snapshot, double time_s)
{
    frames_.push_back({time_s, snapshot});
    while (frames_.size() > 8) frames_.pop_front();
}

bool Interpolation::sample(double render_time, Snapshot& out) const
{
    if (frames_.empty()) return false;
    const double target = render_time - static_cast<double>(delay_);
    if (frames_.size() == 1 || target >= frames_.back().time) {
        out = frames_.back().snapshot;
        return true;
    }
    if (target <= frames_.front().time) {
        out = frames_.front().snapshot;
        return true;
    }
    std::size_t later = 1;
    while (later < frames_.size() && frames_[later].time < target) ++later;
    const auto& a = frames_[later - 1];
    const auto& b = frames_[later];
    const double span = b.time - a.time;
    const float t = span > 1e-6 ? static_cast<float>((target - a.time) / span) : 1.0f;
    out = b.snapshot;
    out.entities.clear();
    for (const auto& ghost : b.snapshot.entities) {
        const Ghost* prev = nullptr;
        for (const auto& candidate : a.snapshot.entities) {
            if (candidate.kind == ghost.kind && candidate.id == ghost.id) {
                prev = &candidate;
                break;
            }
        }
        out.entities.push_back(prev ? lerp_ghost(*prev, ghost, t) : ghost);
    }
    return true;
}

} // namespace forge::net
