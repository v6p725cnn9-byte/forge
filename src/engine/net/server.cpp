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

bool Server::listen(std::uint16_t port, bool lan)
{
    if (!socket_.open()) return false;
    return socket_.bind(lan ? any(port) : localhost(port));
}

void Server::close()
{
    socket_.close();
    for (const auto& peer : peers_) {
        if (world_) world_->destroy_player(peer.player_id);
        if (sim_) sim_->remove_pawn(peer.player_id);
    }
    peers_.clear();
    tick_ = 0;
    accumulator_ = 0;
}

void Server::attach(script::Registry& world) { world_ = &world; }

void Server::attach_sim(game::Sim& sim) { sim_ = &sim; }

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
    const glm::vec3 spawn = sim_ ? sim_->spawn_point(static_cast<int>(world_->alive_players()))
                                 : glm::vec3{static_cast<float>(world_->alive_players()) * 2.5f, 1.0f, -2.0f};
    const int id = world_->spawn_player(spawn);
    if (id < 0) return false;
    if (auto* player = world_->player(id)) player->name = "Player" + std::to_string(id);
    if (sim_) sim_->ensure_pawn(id);
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
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(peers_, [&](const Peer& peer) {
        if (now - peer.last_recv <= kTimeout) return false;
        if (world_) world_->destroy_player(peer.player_id);
        if (sim_) sim_->remove_pawn(peer.player_id);
        return true;
    });
    std::uint8_t buffer[kMaxPacket + 1];
    Address from{};
    for (int received = 0; received < 256; ++received) {
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
        if (!sequence_newer(input.seq, peer->last_seq)) continue;
        peer->last_seq = input.seq;
        peer->input = input;
        if (input.interact) peer->pending_interact = true;
        if (input.place) peer->pending_place = true;
        peer->last_recv = std::chrono::steady_clock::now();
    }
}

void Server::simulate(float dt)
{
    if (!world_) return;
    for (auto& peer : peers_) {
        if (peer.player_id < 0) continue;
        auto* player = world_->player(peer.player_id);
        if (!player) continue;
        const auto* pawn = sim_ ? sim_->pawn(peer.player_id) : nullptr;
        if (sim_ && sim_->phase() != game::Phase::Play) continue;
        if (pawn && (pawn->extracted || pawn->hp <= 0.0f)) continue;
        glm::vec3 move{peer.input.move_x, 0.0f, peer.input.move_z};
        const float length = glm::length(glm::vec2(move.x, move.z));
        if (length > 1.0f) move /= length;
        if (length > 0.05f) {
            const float speed = peer.input.boost ? 9.0f : 5.5f;
            player->position += move * speed * dt;
            player->yaw = peer.input.yaw;
        }
        // Plain projectile gravity; the pawn center rests 1 m above the feet.
        constexpr float kGroundY = 1.0f;
        constexpr float kGravity = 16.0f;
        constexpr float kJumpSpeed = 6.0f;
        const bool grounded = player->position.y <= kGroundY + 1e-4f;
        if (grounded && peer.input.jump) peer.vertical_velocity = kJumpSpeed;
        if (!grounded || peer.vertical_velocity != 0.0f) {
            peer.vertical_velocity -= kGravity * dt;
            player->position.y += peer.vertical_velocity * dt;
            if (player->position.y <= kGroundY) {
                player->position.y = kGroundY;
                peer.vertical_velocity = 0.0f;
            }
        } else {
            player->position.y = kGroundY;
        }
        if (sim_ && peer.pending_interact) {
            sim_->try_extract(peer.player_id, *world_);
            sim_->harvest(peer.player_id, *world_);
            peer.pending_interact = false;
        }
        if (sim_ && peer.pending_place) {
            sim_->place_fire(peer.player_id, *world_);
            peer.pending_place = false;
        }
    }

    if (sim_) sim_->tick(dt, *world_);
    else {
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
        if (sim_) {
            for (int i = 0; i < static_cast<int>(sim_->nodes().size()); ++i) {
                const auto& node = sim_->nodes()[static_cast<std::size_t>(i)];
                if (!node.alive) continue;
                if (xz_distance(player->position, node.position) > stream_radius_) continue;
                Ghost ghost;
                ghost.id = static_cast<std::uint8_t>(i);
                ghost.position = node.position;
                switch (node.kind) {
                case game::NodeKind::Tree:
                    ghost.kind = Kind::Tree;
                    ghost.size = 1.4f;
                    break;
                case game::NodeKind::Rock:
                    ghost.kind = Kind::Rock;
                    ghost.size = 0.9f;
                    break;
                case game::NodeKind::Campfire:
                    ghost.kind = Kind::Campfire;
                    ghost.size = 0.7f;
                    break;
                case game::NodeKind::Extract:
                    ghost.kind = Kind::Extract;
                    ghost.size = 2.2f;
                    break;
                }
                snapshot.entities.push_back(ghost);
            }
            std::sort(snapshot.entities.begin(), snapshot.entities.end(), [&](const Ghost& a, const Ghost& b) {
                const bool a_self = a.kind == Kind::Player && a.id == peer.player_id;
                const bool b_self = b.kind == Kind::Player && b.id == peer.player_id;
                if (a_self != b_self) return a_self;
                return xz_distance(player->position, a.position) < xz_distance(player->position, b.position);
            });
            if (snapshot.entities.size() > static_cast<std::size_t>(kMaxSnapshotEntities))
                snapshot.entities.resize(static_cast<std::size_t>(kMaxSnapshotEntities));
            snapshot.phase = static_cast<std::uint8_t>(sim_->phase());
            if (const auto* pawn = sim_->pawn(peer.player_id)) {
                snapshot.hp = static_cast<std::uint8_t>(std::clamp(pawn->hp, 0.0f, 100.0f));
                snapshot.cold = static_cast<std::uint8_t>(std::clamp(pawn->cold, 0.0f, 100.0f));
                snapshot.wood = pawn->wood;
                snapshot.stone = pawn->stone;
                if (pawn->extracted && sim_->phase() == game::Phase::Play)
                    snapshot.phase = static_cast<std::uint8_t>(game::Phase::Won);
            }
            snapshot.time_left = static_cast<std::uint16_t>(std::max(0.0f, sim_->time_left()));
            snapshot.night = sim_->night() ? 1 : 0;
        }
        const auto packet = pack_snapshot(snapshot);
        if (packet.size() <= kMaxPacket) socket_.send(peer.address, packet.data(), packet.size());
    }
}

void Server::update(float dt)
{
    poll();
    if (!std::isfinite(dt) || dt <= 0) return;
    accumulator_ += std::min(dt, 0.25f);
    const float step = 1.0f / static_cast<float>(kTickHz);
    while (accumulator_ >= step) {
        simulate(step);
        ++tick_;
        broadcast();
        accumulator_ -= step;
    }
}

} // namespace forge::net
