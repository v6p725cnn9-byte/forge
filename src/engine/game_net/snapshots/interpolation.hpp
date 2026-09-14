#pragma once

#include "engine/game_net/snapshots/packets.hpp"

#include <deque>

namespace forge::net {

// Snapshot buffer with a fixed interpolation delay so render (variable Hz)
// samples between two sim ticks instead of snapping to the latest packet.
class Interpolation {
public:
    void push(const Snapshot& snapshot, double time_s);
    bool sample(double render_time, Snapshot& out) const;
    void set_delay(float seconds) { delay_ = seconds; }
    float delay() const { return delay_; }
    void clear() { frames_.clear(); }

private:
    struct Frame {
        double time = 0;
        Snapshot snapshot;
    };
    std::deque<Frame> frames_;
    float delay_ = 0.10f;
};

} // namespace forge::net
