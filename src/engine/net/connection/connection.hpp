#pragma once

#include "engine/net/transport/socket.hpp"

#include <cstdint>

namespace forge::net {

constexpr bool sequence_newer(std::uint32_t candidate, std::uint32_t previous)
{
    const auto distance = candidate - previous;
    return distance != 0 && distance < 0x80000000u;
}

// Per-peer transport state. Knows seq/ack/RTT/loss, not game types.
class Connection {
public:
    explicit Connection(Address remote = {});

    Address remote() const { return remote_; }
    void set_remote(Address remote) { remote_ = remote; }

    std::uint32_t next_seq();
    std::uint32_t peek_seq() const { return next_out_; }
    std::uint32_t last_in() const { return last_in_; }
    std::uint32_t ack_bits() const { return ack_bits_; }

    // True if seq is new (not duplicate/old). Updates last_in / ack_bits.
    bool accept_incoming(std::uint32_t seq);
    void note_send(std::uint32_t seq, double time_s, bool reliable);
    void note_acks(std::uint32_t ack, std::uint32_t bits, double time_s);

    float rtt_ms() const { return rtt_ms_; }
    float loss() const { return loss_; }
    std::uint32_t sent() const { return sent_; }
    std::uint32_t acked() const { return acked_; }
    std::uint32_t dropped() const { return dropped_; }

    bool acked_seq(std::uint32_t seq) const;

private:
    struct Stamp {
        std::uint32_t seq = 0;
        double time = 0;
        bool used = false;
        bool acked = false;
        bool reliable = false;
    };

    Address remote_{};
    std::uint32_t next_out_ = 1;
    std::uint32_t last_in_ = 0;
    std::uint32_t ack_bits_ = 0;
    float rtt_ms_ = 50.0f;
    float loss_ = 0;
    std::uint32_t sent_ = 0;
    std::uint32_t acked_ = 0;
    std::uint32_t dropped_ = 0;
    Stamp stamps_[64]{};
};

} // namespace forge::net
