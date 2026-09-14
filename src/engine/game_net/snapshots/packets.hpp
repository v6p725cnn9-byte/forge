#pragma once

#include "engine/game/inventory/items.hpp"
#include "engine/net/connection/connection.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace forge::net {

constexpr std::uint32_t kMagic = 0x37475246u;
constexpr std::uint8_t kVersion = 2;
constexpr std::size_t kEthernetMtu = 1500;
constexpr std::size_t kIpv4UdpOverhead = 28; // IPv4 header + UDP header
constexpr std::size_t kMaxDatagram = 1400;   // conservative application UDP payload
constexpr std::size_t kTransportHeader = 17; // FNT1 envelope
constexpr std::size_t kGameHeader = 8;
constexpr std::size_t kMaxGamePayload = kMaxDatagram;
constexpr std::size_t kMaxSnapshotSize = kMaxGamePayload;
constexpr std::size_t kMaxReliableMessageSize = kMaxGamePayload;
constexpr std::size_t kMaxPacket = kMaxGamePayload;
constexpr int kDefaultSnapshotEntities = 32; // interest default
constexpr int kMaxSnapshotEntities = 128;    // protocol hard cap, not the default
constexpr float kDefaultStreamRadius = 55.0f;
constexpr int kTickHz = 20;

enum class Packet : std::uint8_t {
    Hello = 1,
    Welcome = 2,
    Input = 3,
    Snapshot = 4,
    Ping = 5,
    Pong = 6,
    Disconnect = 7,
    Event = 8,
    ReliableEvent = 9,
    Ack = 10,
    Resync = 11,
    Auth = 12,
};
constexpr std::uint8_t kMaxPacketType = 12;

enum class Kind : std::uint8_t {
    Player = 1,
    Vehicle = 2,
    Marker = 3,
    Label = 4,
    Tree = 5,
    Rock = 6,
    Campfire = 7,
    Extract = 8,
    Stick = 9, Pebble, Flint, Fiber, IronOre, Furnace, Bench, Loot,
};

struct Input {
    std::uint32_t seq = 0;
    float move_x = 0;
    float move_z = 0;
    float yaw = 0;
    bool boost = false;
    bool interact = false;
    bool place = false;
    bool jump = false;
    std::uint32_t action_seq = 0;
    game::Action action = game::Action::None;
    std::uint8_t argument = 0;
};

struct Ghost {
    Kind kind = Kind::Player;
    std::uint8_t id = 0;
    glm::vec3 position{0};
    float yaw = 0;
    float size = 1;
    glm::vec4 color{1};
    game::Item equipped = game::Item::None;
    std::string name;
    std::string text;
};

struct Welcome {
    std::uint8_t player_id = 0;
    std::uint8_t tick_hz = kTickHz;
    float stream_radius = kDefaultStreamRadius;
};

struct Snapshot {
    std::uint32_t tick = 0;
    std::uint8_t self = 0;
    std::uint32_t ack = 0;
    std::vector<Ghost> entities;
    std::uint8_t hp = 100;
    std::uint8_t cold = 0;
    std::uint8_t o2 = 100;
    std::uint8_t stamina = 100;
    std::uint8_t radiation = 0;
    std::uint16_t wood = 0;
    std::uint16_t stone = 0;
    std::uint8_t phase = 0;
    std::uint16_t time_left = 0;
    std::uint8_t night = 0;
    game::Inventory inventory;
    std::uint32_t action_ack = 0;
    game::Result feedback = game::Result::None;
    std::uint8_t stations = 0;
};

std::vector<std::uint8_t> pack_hello();
std::vector<std::uint8_t> pack_welcome(const Welcome& welcome);
std::vector<std::uint8_t> pack_input(const Input& input);
std::vector<std::uint8_t> pack_snapshot(const Snapshot& snapshot);
std::vector<std::uint8_t> pack_ping(std::uint32_t nonce);
std::vector<std::uint8_t> pack_pong(std::uint32_t nonce);
std::vector<std::uint8_t> pack_disconnect();

bool unpack_type(const std::uint8_t* data, std::size_t size, Packet& type);
bool unpack_hello(const std::uint8_t* data, std::size_t size);
bool unpack_welcome(const std::uint8_t* data, std::size_t size, Welcome& welcome);
bool unpack_input(const std::uint8_t* data, std::size_t size, Input& input);
bool unpack_snapshot(const std::uint8_t* data, std::size_t size, Snapshot& snapshot);
bool unpack_ping(const std::uint8_t* data, std::size_t size, std::uint32_t& nonce);
bool unpack_pong(const std::uint8_t* data, std::size_t size, std::uint32_t& nonce);
bool unpack_disconnect(const std::uint8_t* data, std::size_t size);

} // namespace forge::net
