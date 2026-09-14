#pragma once

#include "engine/game/session.hpp"
#include "engine/net/protocol.hpp"
#include "engine/net/socket.hpp"
#include "engine/script/registry.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

namespace forge::net {

class Server {
public:
    bool listen(std::uint16_t port = 27015, bool lan = false);
    void close();
    void attach(script::Registry& world);
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
        int player_id = -1;
        Input input{};
        std::uint32_t last_seq = 0;
        bool pending_interact = false;
        bool pending_place = false;
        float vertical_velocity = 0.0f;
        std::uint32_t action_ack = 0;
        Input pending_action{};
        std::chrono::steady_clock::time_point last_recv{};
    };

    void poll();
    void simulate(float dt);
    void broadcast();
    Peer* find_peer(const Address& address);
    bool accept(const Address& address);

    Udp socket_;
    script::Registry* world_ = nullptr;
    game::Sim* sim_ = nullptr;
    std::vector<Peer> peers_;
    std::uint32_t tick_ = 0;
    float stream_radius_ = kDefaultStreamRadius;
    float accumulator_ = 0;
};

} // namespace forge::net
