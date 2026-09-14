#pragma once

#include "engine/game_net/snapshots/interpolation.hpp"
#include "engine/game_net/snapshots/packets.hpp"
#include "engine/game_net/prediction/prediction.hpp"
#include "engine/net/transport/channel.hpp"
#include "engine/net/transport/socket.hpp"

#include <chrono>
#include <deque>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace forge::net {

class Client {
public:
    bool connect(const Address& server);
    void close();
    void send_input(float move_x, float move_z, float yaw, bool boost, bool interact = false, bool place = false,
                    bool jump = false, float dt = 1.0f / kTickHz);
    void poll();
    const Snapshot& view() const { return view_; }
    glm::vec3 predicted_position() const { return prediction_.position(); }
    bool request(game::Action action, std::uint8_t argument);
    bool action_pending() const { return !actions_.empty(); }
    bool connected() const { return connected_; }
    std::uint8_t player_id() const { return player_id_; }
    const Snapshot& snapshot() const { return snapshot_; }
    float ping_ms() const { return ping_ms_; }
    std::uint32_t tick() const { return snapshot_.tick; }
    float stream_radius() const { return stream_radius_; }

private:
    struct Request { std::uint32_t seq; game::Action action; std::uint8_t argument; };
    std::deque<Request> actions_;
    std::uint32_t next_action_ = 1;
    Udp socket_;
    Channel channel_{};
    Address server_{};
    bool connected_ = false;
    bool have_snapshot_ = false;
    std::chrono::steady_clock::time_point last_receive_{};
    std::uint8_t player_id_ = 255;
    std::uint32_t seq_ = 1;
    float ping_ms_ = 0;
    float stream_radius_ = kDefaultStreamRadius;
    Snapshot snapshot_{};
    Snapshot view_{};
    Prediction prediction_{};
    Interpolation interpolation_{};
    std::chrono::steady_clock::time_point last_hello_{};
    std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point> sent_;
};

} // namespace forge::net
