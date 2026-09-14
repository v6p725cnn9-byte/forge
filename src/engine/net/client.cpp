#include "engine/net/client.hpp"

#include <utility>

namespace forge::net {

bool Client::connect(const Address& server)
{
    close();
    if (!socket_.open()) return false;
    if (!socket_.bind(any(0))) return false;
    server_ = server;
    connected_ = false;
    player_id_ = 255;
    last_hello_ = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto hello = pack_hello();
    return socket_.send(server_, hello.data(), hello.size());
}

bool Client::request(game::Action action, std::uint8_t argument)
{
    if (!connected_ || actions_.size() >= 8 || action == game::Action::None) return false;
    actions_.push_back({next_action_++, action, argument});
    if (next_action_ == 0) next_action_ = 1;
    return true;
}

void Client::close()
{
    socket_.close();
    actions_.clear();
    next_action_ = 1;
    connected_ = false;
    player_id_ = 255;
    seq_ = 1;
    ping_ms_ = 0;
    stream_radius_ = kDefaultStreamRadius;
    have_snapshot_ = false;
    snapshot_ = {};
    sent_.clear();
}

void Client::send_input(float move_x, float move_z, float yaw, bool boost, bool interact, bool place, bool jump)
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
    input.jump = jump;
    if (!actions_.empty()) {
        const auto& request = actions_.front();
        input.action_seq = request.seq;
        input.action = request.action;
        input.argument = request.argument;
    }
    const auto packet = pack_input(input);
    socket_.send(server_, packet.data(), packet.size());
    sent_[input.seq] = now;
    if (sent_.size() > 64) {
        const auto oldest = seq_ - 64;
        for (auto it = sent_.begin(); it != sent_.end();)
            if (sequence_newer(oldest, it->first)) it = sent_.erase(it);
            else ++it;
    }
}

void Client::poll()
{
    if (connected_ && std::chrono::steady_clock::now() - last_receive_ > std::chrono::seconds(3)) {
        connected_ = false;
        have_snapshot_ = false;
        player_id_ = 255;
        snapshot_ = {};
        sent_.clear();
        actions_.clear();
        ping_ms_ = 0;
    }
    std::uint8_t buffer[kMaxPacket + 1];
    Address from{};
    for (int received = 0; received < 256; ++received) {
        const int got = socket_.receive(from, buffer, sizeof(buffer));
        if (got == 0) break;
        if (got < 0) break;
        if (!(from == server_)) continue;
        Packet type{};
        if (!unpack_type(buffer, static_cast<std::size_t>(got), type)) continue;
        if (type == Packet::Welcome) {
            Welcome welcome{};
            if (!unpack_welcome(buffer, static_cast<std::size_t>(got), welcome)) continue;
            if (connected_) continue;
            last_receive_ = std::chrono::steady_clock::now();
            player_id_ = welcome.player_id;
            stream_radius_ = welcome.stream_radius;
            connected_ = true;
        } else if (type == Packet::Snapshot) {
            Snapshot snapshot{};
            if (!unpack_snapshot(buffer, static_cast<std::size_t>(got), snapshot)) continue;
            if (!connected_ || snapshot.self != player_id_
                || (have_snapshot_ && !sequence_newer(snapshot.tick, snapshot_.tick))) continue;
            have_snapshot_ = true;
            last_receive_ = std::chrono::steady_clock::now();
            snapshot_ = std::move(snapshot);
            if (!actions_.empty() && snapshot_.action_ack == actions_.front().seq) actions_.pop_front();
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
