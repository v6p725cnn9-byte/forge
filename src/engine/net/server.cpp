#include "engine/net/server.hpp"

#include "engine/net/interest.hpp"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace forge::net {
namespace {
constexpr auto kTimeout = std::chrono::seconds(3);
}

bool Server::listen(std::uint16_t port)
{
    if (!socket_.open()) return false;
    return socket_.bind(localhost(port));
}

void Server::close()
{
    socket_.close();
    peers_.clear();
    tick_ = 0;
    accumulator_ = 0;
}

void Server::attach(script::Registry& world) { world_ = &world; }

void Server::set_stream_radius(float meters) { stream_radius_ = std::max(meters, 8.0f); }

std::uint32_t Server::peers() const
{
    std::uint32_t n = 0;
    for (const auto& peer : peers_)
        if (peer.player_id >= 0) ++n;
    return n;
}

Server::Peer* Server::find_peer(const Address& address)
{
    for (auto& peer : peers_) {
        if (peer.address == address) return &peer;
    }
    return nullptr;
}

bool Server::accept(const Address& address)
{
    if (!world_) return false;
    if (peers() >= 8) return false;
    const int id = world_->spawn_player({static_cast<float>(world_->alive_players()) * 2.5f, 1.0f, -2.0f});
    if (id < 0) return false;
    if (auto* player = world_->player(id)) player->name = "Player" + std::to_string(id);
    Peer peer;
    peer.address = address;
    peer.player_id = id;
    peer.last_recv = std::chrono::steady_clock::now();
    peers_.push_back(peer);
    const Welcome welcome{static_cast<std::uint8_t>(id), static_cast<std::uint8_t>(kTickHz), stream_radius_};
    const auto packet = pack_welcome(welcome);
    socket_.send(address, packet.data(), packet.size());
    return true;
}

void Server::poll()
{
    std::uint8_t buffer[kMaxPacket];
    Address from{};
    for (;;) {
        const int got = socket_.receive(from, buffer, sizeof(buffer));
        if (got == 0) break;
        if (got < 0) break;
        Packet type{};
        if (!unpack_type(buffer, static_cast<std::size_t>(got), type)) continue;
        auto* peer = find_peer(from);
        if (type == Packet::Hello) {
            if (!unpack_hello(buffer, static_cast<std::size_t>(got))) continue;
            if (!peer) accept(from);
            else {
                const Welcome welcome{static_cast<std::uint8_t>(peer->player_id), static_cast<std::uint8_t>(kTickHz),
                                      stream_radius_};
                const auto packet = pack_welcome(welcome);
                socket_.send(from, packet.data(), packet.size());
                peer->last_recv = std::chrono::steady_clock::now();
            }
            continue;
        }
        if (!peer || type != Packet::Input) continue;
        Input input{};
        if (!unpack_input(buffer, static_cast<std::size_t>(got), input)) continue;
        if (input.seq < peer->last_seq && peer->last_seq - input.seq < 1024) continue;
        peer->last_seq = input.seq;
        peer->input = input;
        peer->last_recv = std::chrono::steady_clock::now();
    }
}

void Server::simulate(float dt)
{
    if (!world_) return;
    const auto now = std::chrono::steady_clock::now();
    for (auto& peer : peers_) {
        if (peer.player_id < 0) continue;
        if (now - peer.last_recv > kTimeout) {
            world_->destroy_player(peer.player_id);
            peer.player_id = -1;
            continue;
        }
        auto* player = world_->player(peer.player_id);
        if (!player) continue;
        glm::vec3 move{peer.input.move_x, 0.0f, peer.input.move_z};
        const float length = glm::length(glm::vec2(move.x, move.z));
        if (length > 1.0f) move /= length;
        if (length > 0.05f) {
            const float speed = peer.input.boost ? 9.0f : 5.5f;
            player->position += move * speed * dt;
            player->yaw = peer.input.yaw;
        }
        player->position.y = 1.0f;
    }

    for (int id = 0; id < static_cast<int>(world_->players().size()); ++id) {
        auto* player = world_->player(id);
        if (!player) continue;
        bool driven = false;
        for (const auto& peer : peers_)
            if (peer.player_id == id) driven = true;
        if (driven) continue;
        const float phase = static_cast<float>(tick_) * 0.05f + static_cast<float>(id);
        player->position.x = player->home.x + std::cos(phase) * 6.0f;
        player->position.z = player->home.z + std::sin(phase) * 6.0f;
        player->position.y = player->home.y;
        player->yaw = glm::degrees(phase);
    }
}

void Server::broadcast()
{
    if (!world_) return;
    for (auto& peer : peers_) {
        if (peer.player_id < 0) continue;
        const auto* player = world_->player(peer.player_id);
        if (!player) continue;
        Snapshot snapshot;
        snapshot.tick = tick_;
        snapshot.self = static_cast<std::uint8_t>(peer.player_id);
        snapshot.ack = peer.last_seq;
        snapshot.entities = collect_stream(*world_, player->position, stream_radius_, peer.player_id);
        const auto packet = pack_snapshot(snapshot);
        if (packet.size() <= kMaxPacket) socket_.send(peer.address, packet.data(), packet.size());
    }
}

void Server::update(float dt)
{
    poll();
    accumulator_ += dt;
    const float step = 1.0f / static_cast<float>(kTickHz);
    while (accumulator_ >= step) {
        simulate(step);
        ++tick_;
        broadcast();
        accumulator_ -= step;
    }
}

} // namespace forge::net
