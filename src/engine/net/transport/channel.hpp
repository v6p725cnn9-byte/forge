#pragma once

#include "engine/net/connection/connection.hpp"
#include "engine/net/transport/socket.hpp"

#include <cstdint>
#include <vector>

namespace forge::net {

constexpr std::uint32_t kTransportMagic = 0x31544E46u; // FNT1
constexpr std::size_t kEnvelopeBytes = 17;
constexpr std::uint8_t kFlagReliable = 1;

// Lane is the protocol contract. Implementation today uses 0 and 1 only.
// Ordered reliable (2) and a dedicated snapshot lane (3) are reserved —
// do not silently promote Hello/Event onto ordered without a new Channel.
enum class Lane : std::uint8_t {
    Unreliable = 0,
    ReliableUnordered = 1,
    ReliableOrdered = 2,
    Snapshot = 3,
};

enum class Reliability : std::uint8_t { Unreliable, Reliable };

inline Lane lane_of(Reliability reliability)
{
    return reliability == Reliability::Reliable ? Lane::ReliableUnordered : Lane::Unreliable;
}

struct Datagram {
    std::uint32_t seq = 0;
    std::uint32_t ack = 0;
    std::uint32_t ack_bits = 0;
    std::uint8_t flags = 0;
    const std::uint8_t* payload = nullptr;
    std::size_t payload_size = 0;
    bool enveloped = false;
};

bool parse_datagram(const std::uint8_t* data, std::size_t size, Datagram& out);
std::vector<std::uint8_t> pack_datagram(std::uint32_t seq, std::uint32_t ack, std::uint32_t ack_bits, std::uint8_t flags,
                                        const std::uint8_t* payload, std::size_t payload_size);

// Reliable resend + unreliable fire-and-forget on top of one Connection.
class Channel {
public:
    explicit Channel(Address remote = {});

    Connection& connection() { return conn_; }
    const Connection& connection() const { return conn_; }

    bool send(Udp& socket, Reliability reliability, const std::uint8_t* payload, std::size_t size, double time_s);
    bool send(Udp& socket, Reliability reliability, const std::vector<std::uint8_t>& payload, double time_s);
    // Returns true if `out` holds a game payload. Naked (legacy) packets also succeed.
    bool ingest(const std::uint8_t* data, std::size_t size, double time_s, std::vector<std::uint8_t>& out);
    void update(Udp& socket, double time_s);

private:
    struct Pending {
        std::uint32_t seq = 0;
        std::vector<std::uint8_t> payload;
        double last_send = 0;
        int tries = 0;
    };

    Connection conn_;
    std::vector<Pending> reliable_;
};

} // namespace forge::net
