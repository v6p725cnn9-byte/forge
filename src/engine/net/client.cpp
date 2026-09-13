#include "engine/net/client.hpp"

#include <utility>

namespace forge::net {

bool Client::connect(const Address& server)
{
    close();
    if (!socket_.open()) return false;
    if (!socket_.bind(localhost(0))) return false;
    server_ = server;
    connected_ = false;
    player_id_ = 255;
    last_hello_ = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto hello = pack_hello();
    return socket_.send(server_, hello.data(), hello.size());
}

void Client::close()
{
    socket_.close();
    connected_ = false;
    snapshot_ = {};
    sent_.clear();
}

void Client::send_input(float move_x, float move_z, float yaw, bool boost, bool interact, bool place)
{
    if (!socket_.valid()) return;
    const auto now = std::chrono::steady_clock::now();
    if (!connected_) {
        if (now - last_hello_ > std::chrono::milliseconds(200)) {
            const auto hello = pack_hello();
            socket_.send(server_, hello.data(), hello.size());
            last_hello_ = now;
        }
        return;
    }
    Input input;
    input.seq = seq_++;
    input.move_x = move_x;
    input.move_z = move_z;
    input.yaw = yaw;
    input.boost = boost;
    input.interact = interact;
    input.place = place;
    const auto packet = pack_input(input);
    socket_.send(server_, packet.data(), packet.size());
    sent_[input.seq] = now;
    if (sent_.size() > 64) {
        const auto oldest = seq_ - 64;
        for (auto it = sent_.begin(); it != sent_.end();)
            if (it->first < oldest) it = sent_.erase(it);
            else ++it;
    }
}

void Client::poll()
{
    std::uint8_t buffer[kMaxPacket];
    Address from{};
    for (;;) {
        const int got = socket_.receive(from, buffer, sizeof(buffer));
        if (got == 0) break;
        if (got < 0) break;
        Packet type{};
        if (!unpack_type(buffer, static_cast<std::size_t>(got), type)) continue;
        if (type == Packet::Welcome) {
            Welcome welcome{};
            if (!unpack_welcome(buffer, static_cast<std::size_t>(got), welcome)) continue;
            player_id_ = welcome.player_id;
            stream_radius_ = welcome.stream_radius;
            connected_ = true;
        } else if (type == Packet::Snapshot) {
            Snapshot snapshot{};
            if (!unpack_snapshot(buffer, static_cast<std::size_t>(got), snapshot)) continue;
            snapshot_ = std::move(snapshot);
            const auto found = sent_.find(snapshot_.ack);
            if (found != sent_.end()) {
                const auto dt = std::chrono::steady_clock::now() - found->second;
                ping_ms_ = std::chrono::duration<float, std::milli>(dt).count();
                sent_.erase(found);
            }
        }
    }
}

} // namespace forge::net
