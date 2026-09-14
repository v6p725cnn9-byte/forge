#include "engine/net/transport/channel.hpp"

#include "engine/net/transport/stream.hpp"

#include <algorithm>
#include <cstring>

namespace forge::net {

bool parse_datagram(const std::uint8_t* data, std::size_t size, Datagram& out)
{
    out = {};
    if (!data || size == 0) return false;
    ByteReader r(data, size);
    std::uint32_t magic = 0;
    if (!r.u32(magic)) return false;
    if (magic != kTransportMagic) {
        out.payload = data;
        out.payload_size = size;
        out.enveloped = false;
        return true;
    }
    if (!r.u32(out.seq) || !r.u32(out.ack) || !r.u32(out.ack_bits) || !r.u8(out.flags)) return false;
    out.payload = data + r.offset();
    out.payload_size = size - r.offset();
    out.enveloped = true;
    return true;
}

std::vector<std::uint8_t> pack_datagram(std::uint32_t seq, std::uint32_t ack, std::uint32_t ack_bits, std::uint8_t flags,
                                        const std::uint8_t* payload, std::size_t payload_size)
{
    ByteWriter w;
    w.u32(kTransportMagic);
    w.u32(seq);
    w.u32(ack);
    w.u32(ack_bits);
    w.u8(flags);
    if (payload && payload_size) w.bytes(payload, payload_size);
    return w.take();
}

Channel::Channel(Address remote) : conn_(remote) {}

bool Channel::send(Udp& socket, Reliability reliability, const std::uint8_t* payload, std::size_t size, double time_s)
{
    const bool reliable = reliability == Reliability::Reliable;
    const std::uint32_t seq = conn_.next_seq();
    conn_.note_send(seq, time_s, reliable);
    const auto datagram = pack_datagram(seq, conn_.last_in(), conn_.ack_bits(),
                                        reliable ? kFlagReliable : 0, payload, size);
    if (reliable) {
        Pending pending;
        pending.seq = seq;
        pending.payload.assign(payload, payload + size);
        pending.last_send = time_s;
        pending.tries = 1;
        reliable_.push_back(std::move(pending));
        if (reliable_.size() > 32) reliable_.erase(reliable_.begin());
    }
    return socket.send(conn_.remote(), datagram.data(), datagram.size());
}

bool Channel::send(Udp& socket, Reliability reliability, const std::vector<std::uint8_t>& payload, double time_s)
{
    return send(socket, reliability, payload.data(), payload.size(), time_s);
}

bool Channel::ingest(const std::uint8_t* data, std::size_t size, double time_s, std::vector<std::uint8_t>& out)
{
    Datagram datagram;
    if (!parse_datagram(data, size, datagram) || !datagram.payload) return false;
    if (datagram.enveloped) {
        conn_.note_acks(datagram.ack, datagram.ack_bits, time_s);
        reliable_.erase(std::remove_if(reliable_.begin(), reliable_.end(),
                                       [&](const Pending& pending) { return conn_.acked_seq(pending.seq); }),
                        reliable_.end());
        if (datagram.seq != 0 && !conn_.accept_incoming(datagram.seq)) return false;
    }
    out.assign(datagram.payload, datagram.payload + datagram.payload_size);
    return !out.empty();
}

void Channel::update(Udp& socket, double time_s)
{
    const float rtt = std::max(0.05f, conn_.rtt_ms() / 1000.0f);
    const double resend = static_cast<double>(std::max(0.08f, rtt * 1.25f));
    for (auto& pending : reliable_) {
        if (time_s - pending.last_send < resend) continue;
        if (pending.tries >= 12) continue;
        ++pending.tries;
        pending.last_send = time_s;
        const auto datagram = pack_datagram(pending.seq, conn_.last_in(), conn_.ack_bits(), kFlagReliable,
                                            pending.payload.data(), pending.payload.size());
        socket.send(conn_.remote(), datagram.data(), datagram.size());
    }
}

} // namespace forge::net
