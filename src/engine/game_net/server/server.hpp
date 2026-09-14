#pragma once

#include "engine/game/world/world.hpp"
#include "engine/game_net/snapshots/packets.hpp"
#include "engine/net/transport/channel.hpp"
#include "engine/net/transport/socket.hpp"
#include "engine/physics/world/world.hpp"
#include "engine/script/bindings/registry.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace forge::net {

class Server {
public:
    bool listen(std::uint16_t port = 27015, bool lan = false);
    void close();
    void attach(game::World& world); // production: one aggregate root
    void attach(game::Actors& actors); // M7 lab without Sim
    void attach_sim(game::Sim& sim);
    void set_stream_radius(float meters);
    float stream_radius() const { return stream_radius_; }
    void update(float dt);
    std::uint16_t port() const { return socket_.port(); }
    std::uint32_t tick() const { return tick_; }
    std::uint32_t peers() const;
    bool ready() const { return socket_.valid(); }

private:
    struct Peer {
        Address address{};
        Channel channel{};
        int player_id = -1;
        Input input{};
        std::uint32_t last_seq = 0;
        bool pending_interact = false;
        bool pending_place = false;
        bool pending_jump = false;
        std::uint32_t action_ack = 0;
        Input pending_action{};
        std::chrono::steady_clock::time_point last_recv{};
    };

    void poll();
    void simulate(float dt);
    void broadcast();
    Peer* find_peer(const Address& address);
    bool accept(const Address& address);
    void ensure_physics();
    void sync_statics();
    int body_for(int player_id);
    void drop_body(int player_id);

    Udp socket_;
    game::Actors* world_ = nullptr;
    game::Sim* sim_ = nullptr;
    std::vector<Peer> peers_;
    std::uint32_t tick_ = 0;
    float stream_radius_ = kDefaultStreamRadius;
    float accumulator_ = 0;
    phys::World physics_;
    bool physics_ready_ = false;
    int crate_ = -1;
    std::array<int, game::kMaxPlayers> bodies_ = [] {
        std::array<int, game::kMaxPlayers> ids{};
        ids.fill(-1);
        return ids;
    }();
    std::vector<int> static_boxes_;
    std::size_t static_signature_ = 0;
    bool static_synced_ = false;
};

} // namespace forge::net
