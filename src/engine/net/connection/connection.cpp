#include "engine/net/connection/connection.hpp"

#include <algorithm>
#include <cmath>

namespace forge::net {

Connection::Connection(Address remote) : remote_(remote) {}

std::uint32_t Connection::next_seq()
{
    const std::uint32_t seq = next_out_++;
    if (next_out_ == 0) next_out_ = 1;
    return seq;
}

bool Connection::accept_incoming(std::uint32_t seq)
{
    if (seq == 0) return true;
    if (last_in_ == 0) {
        last_in_ = seq;
        ack_bits_ = 0;
        return true;
    }
    if (seq == last_in_) return false;
    if (sequence_newer(seq, last_in_)) {
        const std::uint32_t gap = seq - last_in_;
        if (gap >= 32) ack_bits_ = 0;
        else ack_bits_ = (ack_bits_ << gap) | (1u << (gap - 1));
        last_in_ = seq;
        return true;
    }
    const std::uint32_t behind = last_in_ - seq;
    if (behind == 0 || behind > 32) return false;
    const std::uint32_t bit = 1u << (behind - 1);
    if (ack_bits_ & bit) return false;
    ack_bits_ |= bit;
    return true;
}

void Connection::note_send(std::uint32_t seq, double time_s, bool reliable)
{
    ++sent_;
    stamps_[seq & 63] = Stamp{seq, time_s, true, false, reliable};
}

void Connection::note_acks(std::uint32_t ack, std::uint32_t bits, double time_s)
{
    if (ack == 0) return;
    auto consider = [&](std::uint32_t seq) {
        if (seq == 0) return;
        Stamp& stamp = stamps_[seq & 63];
        if (!stamp.used || stamp.seq != seq || stamp.acked) return;
        stamp.acked = true;
        ++acked_;
        const float sample = static_cast<float>((time_s - stamp.time) * 1000.0);
        if (sample > 0.0f && sample < 2000.0f)
            rtt_ms_ = rtt_ms_ * 0.875f + sample * 0.125f;
    };
    consider(ack);
    for (int i = 0; i < 32; ++i)
        if (bits & (1u << i)) consider(ack - static_cast<std::uint32_t>(i + 1));

    std::uint32_t lost = 0;
    for (const auto& stamp : stamps_) {
        if (!stamp.used || stamp.acked) continue;
        if (ack != 0 && sequence_newer(ack, stamp.seq + 32)) ++lost;
    }
    dropped_ += lost;
    const float total = static_cast<float>(std::max(1u, acked_ + dropped_));
    loss_ = static_cast<float>(dropped_) / total;
}

bool Connection::acked_seq(std::uint32_t seq) const
{
    const Stamp& stamp = stamps_[seq & 63];
    return stamp.used && stamp.seq == seq && stamp.acked;
}

} // namespace forge::net
