#pragma once

#include "engine/net/protocol.hpp"
#include "engine/net/socket.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace forge::net {

class Client {
public:
    bool connect(const Address& server);
    void close();
    void send_input(float move_x, float move_z, float yaw, bool boost, bool interact = false, bool place = false);
    void poll();
    bool connected() const { return connected_; }
    std::uint8_t player_id() const { return player_id_; }
    const Snapshot& snapshot() const { return snapshot_; }
    float ping_ms() const { return ping_ms_; }
    std::uint32_t tick() const { return snapshot_.tick; }
    float stream_radius() const { return stream_radius_; }

private:
    Udp socket_;
    Address server_{};
    bool connected_ = false;
    std::uint8_t player_id_ = 255;
    std::uint32_t seq_ = 1;
    float ping_ms_ = 0;
    float stream_radius_ = kDefaultStreamRadius;
    Snapshot snapshot_{};
    std::chrono::steady_clock::time_point last_hello_{};
    std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point> sent_;
};

} // namespace forge::net
