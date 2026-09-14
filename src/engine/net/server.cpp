#include "engine/net/server.hpp"

#include "engine/net/interest.hpp"
#include "engine/phys/als.hpp"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace forge::net {
namespace {
constexpr auto kTimeout = std::chrono::seconds(3);
constexpr float kCapsuleRadius = forge::phys::als::kCapsuleRadius;
constexpr float kCapsuleHalfHeight = forge::phys::als::kCylinderHalfHeight;
// The registry tracks each pawn by a center 1 m above the feet, while Jolt
// reports the capsule center, which rests half-height + radius above them.
constexpr float kCapsuleRest = kCapsuleHalfHeight + kCapsuleRadius;
constexpr float kCenterOffset = 1.0f - kCapsuleRest;
static_assert(kCenterOffset > 0.0f, "pawn center must sit above the capsule center");

bool valid_player(int player_id)
{
    return player_id >= 0 && player_id < script::kMaxPlayers;
}
} // namespace

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
        drop_body(peer.player_id);
    }
    if (physics_ready_) {
        for (int box : static_boxes_) physics_.remove_box(box);
    }
    static_boxes_.clear();
    static_synced_ = false;
    peers_.clear();
    tick_ = 0;
    accumulator_ = 0;
}

void Server::ensure_physics()
{
    if (physics_ready_) return;
    if (!physics_.init()) return;
    physics_.add_box({0.0f, -0.5f, 40.0f}, {90.0f, 0.5f, 100.0f});
    physics_ready_ = true;
}

void Server::sync_statics()
{
    if (!physics_ready_ || !sim_) return;
    const auto& nodes = sim_->nodes();
    std::size_t signature = nodes.size() + 1;
    for (const auto& node : nodes) {
        std::size_t part = static_cast<std::size_t>(static_cast<int>(node.kind) * 2 + (node.alive ? 1 : 0));
        const int qx = static_cast<int>(node.position.x * 4.0f);
        const int qz = static_cast<int>(node.position.z * 4.0f);
        part = part * 1315423911u + static_cast<std::size_t>(qx * 73 + qz * 193);
        signature ^= part + 0x9e3779b9u + (signature << 6) + (signature >> 2);
    }
    if (static_synced_ && signature == static_signature_) return;
    for (int box : static_boxes_) physics_.remove_box(box);
    static_boxes_.clear();
    for (const auto& node : nodes) {
        if (!node.alive) continue;
        const float x = node.position.x;
        const float z = node.position.z;
        switch (node.kind) {
        case game::NodeKind::Tree:
            static_boxes_.push_back(physics_.add_box({x, 2.0f, z}, {0.35f, 2.0f, 0.35f}));
            break;
        case game::NodeKind::Rock:
            static_boxes_.push_back(physics_.add_box({x, 0.6f, z}, {0.8f, 0.6f, 0.8f}));
            break;
        case game::NodeKind::IronOre:
            static_boxes_.push_back(physics_.add_box({x, 1.0f, z}, {0.8f, 1.0f, 0.8f}));
            break;
        case game::NodeKind::Campfire:
            static_boxes_.push_back(physics_.add_box({x, 0.3f, z}, {0.6f, 0.3f, 0.6f}));
            break;
        case game::NodeKind::Furnace:
        case game::NodeKind::Bench:
            static_boxes_.push_back(physics_.add_box({x, 0.5f, z}, {0.6f, 0.5f, 0.6f}));
            break;
        case game::NodeKind::Extract:
            static_boxes_.push_back(physics_.add_box({x, 1.6f, z}, {0.6f, 1.6f, 0.6f}));
            break;
        default:
            break;
        }
    }
    static_boxes_.erase(std::remove(static_boxes_.begin(), static_boxes_.end(), -1), static_boxes_.end());
    static_signature_ = signature;
    static_synced_ = true;
}

int Server::body_for(int player_id)
{
    if (!valid_player(player_id) || !physics_ready_ || !world_) return -1;
    int body = bodies_[static_cast<std::size_t>(player_id)];
    if (body >= 0) return body;
    const auto* player = world_->player(player_id);
    if (!player) return -1;
    body = physics_.spawn_character(player->position - glm::vec3{0.0f, kCenterOffset, 0.0f}, kCapsuleRadius,
                                    kCapsuleHalfHeight);
    bodies_[static_cast<std::size_t>(player_id)] = body;
    return body;
}

void Server::drop_body(int player_id)
{
    if (!valid_player(player_id)) return;
    const int body = bodies_[static_cast<std::size_t>(player_id)];
    if (body < 0) return;
    if (physics_ready_) physics_.remove_character(body);
    bodies_[static_cast<std::size_t>(player_id)] = -1;
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
    ensure_physics();
    drop_body(id);
    body_for(id);
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
        drop_body(peer.player_id);
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
        if (input.action != game::Action::None && sequence_newer(input.action_seq, peer->action_ack))
            peer->pending_action = input;
        if (input.interact) peer->pending_interact = true;
        if (input.place) peer->pending_place = true;
        peer->last_recv = std::chrono::steady_clock::now();
    }
}

void Server::simulate(float dt)
{
    if (!world_) return;
    ensure_physics();
    sync_statics();
    for (auto& peer : peers_) {
        if (!valid_player(peer.player_id)) continue;
        auto* player = world_->player(peer.player_id);
        if (!player) continue;
        if (peer.pending_action.action != game::Action::None) {
            if (sim_) sim_->action(peer.player_id, *world_, peer.pending_action.action, peer.pending_action.argument);
            peer.action_ack = peer.pending_action.action_seq;
            peer.pending_action = {};
        }
        const int body = body_for(peer.player_id);
        if (body >= 0) physics_.set_character_facing(body, peer.input.yaw);
        const auto* pawn = sim_ ? sim_->pawn(peer.player_id) : nullptr;
        const bool controllable =
            body >= 0 && (!sim_ || sim_->phase() == game::Phase::Play) && (!pawn || (!pawn->extracted && pawn->hp > 0.0f));
        if (!controllable) {
            if (body >= 0) physics_.set_character_input(body, glm::vec3{0.0f}, false, 0.0f);
            if (body >= 0) player->yaw = physics_.character_yaw(body);
            continue;
        }
        glm::vec3 move{peer.input.move_x, 0.0f, peer.input.move_z};
        const float length = glm::length(glm::vec2(move.x, move.z));
        if (length > 1.0f) move /= length;
        bool want_boost = peer.input.boost;
        if (sim_) {
            if (auto* pawn = sim_->pawn(peer.player_id)) {
                if (want_boost && length > 0.05f) pawn->stamina = std::max(0.0f, pawn->stamina - 12.0f * dt);
                else pawn->stamina = std::min(100.0f, pawn->stamina + 8.0f * dt);
                want_boost = want_boost && pawn->stamina > 1.0f;
            }
        }
        if (length <= 0.05f) move = glm::vec3{0.0f};
        float top = game::kRunGaitSpeed;
        if (length > 0.05f) {
            if (want_boost && game::sprint_cone(move, peer.input.yaw)) top = game::kSprintGaitSpeed;
            else if (pawn && pawn->stamina <= 1.0f) top = game::kWalkGaitSpeed;
        }
        const bool vaulted = peer.input.jump && length > 0.3f && physics_.try_mantle(body, move);
        physics_.set_character_input(body, move, peer.input.jump && !vaulted, top);
    }

    if (physics_ready_) {
        constexpr int kSubsteps = 2;
        const float sub = dt / static_cast<float>(kSubsteps);
        for (int i = 0; i < kSubsteps; ++i) physics_.tick(sub);
    }
    for (auto& peer : peers_) {
        if (!valid_player(peer.player_id)) continue;
        auto* player = world_->player(peer.player_id);
        if (!player) continue;
        const int body = bodies_[static_cast<std::size_t>(peer.player_id)];
        if (body >= 0 && physics_ready_) {
            player->position = physics_.character_position(body) + glm::vec3{0.0f, kCenterOffset, 0.0f};
            player->yaw = physics_.character_yaw(body);
            const glm::vec3 move{peer.input.move_x, 0.0f, peer.input.move_z};
            if (!physics_.character_supported(body) && glm::length(glm::vec2(move.x, move.z)) > 0.3f)
                physics_.try_mantle(body, move);
        }
        if (sim_ && peer.pending_interact) {
            sim_->harvest(peer.player_id, *world_);
            if (const auto* state = sim_->pawn(peer.player_id); state && state->feedback == game::Result::TooFar)
                sim_->try_extract(peer.player_id, *world_);
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
                case game::NodeKind::Stick: ghost.kind = Kind::Stick; break;
                case game::NodeKind::Pebble: ghost.kind = Kind::Pebble; break;
                case game::NodeKind::Flint: ghost.kind = Kind::Flint; break;
                case game::NodeKind::Fiber: ghost.kind = Kind::Fiber; break;
                case game::NodeKind::IronOre: ghost.kind = Kind::IronOre; break;
                case game::NodeKind::Furnace: ghost.kind = Kind::Furnace; break;
                case game::NodeKind::Bench: ghost.kind = Kind::Bench; break;
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
                snapshot.o2 = static_cast<std::uint8_t>(std::clamp(pawn->o2, 0.0f, 100.0f));
                snapshot.stamina = static_cast<std::uint8_t>(std::clamp(pawn->stamina, 0.0f, 100.0f));
                snapshot.radiation = static_cast<std::uint8_t>(std::clamp(pawn->radiation, 0.0f, 100.0f));
                snapshot.inventory = pawn->inventory;
                snapshot.wood = pawn->inventory[game::Item::Wood];
                snapshot.stone = pawn->inventory[game::Item::Stone];
                snapshot.feedback = pawn->feedback;
                if (pawn->extracted && sim_->phase() == game::Phase::Play)
                    snapshot.phase = static_cast<std::uint8_t>(game::Phase::Won);
            }
            snapshot.time_left = static_cast<std::uint16_t>(std::max(0.0f, sim_->time_left()));
            snapshot.night = sim_->night() ? 1 : 0;
        }
        snapshot.action_ack = peer.action_ack;
        if (sim_) {
            snapshot.stations = (sim_->near_station(peer.player_id, *world_, game::Station::Furnace) ? 1 : 0)
                | (sim_->near_station(peer.player_id, *world_, game::Station::Bench) ? 2 : 0);
            for (auto& entity : snapshot.entities)
                if (entity.kind == Kind::Player)
                    if (const auto* pawn = sim_->pawn(entity.id)) entity.equipped = pawn->inventory.equipped;
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
