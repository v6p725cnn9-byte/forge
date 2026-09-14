#include "engine/game_net/snapshots/packets.hpp"

#include "engine/net/transport/stream.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace forge::net {
namespace {

void game_header(ByteWriter& w, Packet type)
{
    w.u32(kMagic);
    w.u8(static_cast<std::uint8_t>(type));
    w.u8(kVersion);
    w.u16(0);
}

void game_finish(ByteWriter& w)
{
    const auto size = static_cast<std::uint16_t>(w.size() - 8);
    w.poke_u16(6, size);
}

bool game_header(ByteReader& r, Packet expected)
{
    if (r.size() > kMaxPacket) return false;
    std::uint32_t magic = 0;
    std::uint8_t type = 0, version = 0;
    std::uint16_t payload = 0;
    if (!r.u32(magic) || magic != kMagic) return false;
    if (!r.u8(type) || type != static_cast<std::uint8_t>(expected)) return false;
    if (!r.u8(version) || version != kVersion) return false;
    if (!r.u16(payload) || r.offset() + payload != r.size()) return false;
    return true;
}

} // namespace

std::vector<std::uint8_t> pack_hello()
{
    ByteWriter w;
    game_header(w, Packet::Hello);
    w.u8(kVersion);
    game_finish(w);
    return w.take();
}

std::vector<std::uint8_t> pack_welcome(const Welcome& welcome)
{
    ByteWriter w;
    game_header(w, Packet::Welcome);
    w.u8(welcome.player_id);
    w.u8(welcome.tick_hz);
    w.f32(welcome.stream_radius);
    game_finish(w);
    return w.take();
}

std::vector<std::uint8_t> pack_input(const Input& input)
{
    ByteWriter w;
    game_header(w, Packet::Input);
    w.u32(input.seq);
    w.f32(input.move_x);
    w.f32(input.move_z);
    w.f32(input.yaw);
    std::uint8_t flags = 0;
    if (input.boost) flags |= 1;
    if (input.interact) flags |= 2;
    if (input.place) flags |= 4;
    if (input.jump) flags |= 8;
    w.u8(flags);
    w.u32(input.action_seq);
    w.u8(static_cast<std::uint8_t>(input.action));
    w.u8(input.argument);
    game_finish(w);
    return w.take();
}

std::vector<std::uint8_t> pack_snapshot(const Snapshot& snapshot)
{
    ByteWriter w;
    game_header(w, Packet::Snapshot);
    w.u32(snapshot.tick);
    w.u8(snapshot.self);
    w.u32(snapshot.ack);
    const auto count = static_cast<std::uint8_t>(std::min(snapshot.entities.size(), static_cast<std::size_t>(kMaxSnapshotEntities)));
    const auto count_offset = w.size();
    w.u8(0);
    std::uint8_t written = 0;
    for (std::uint8_t i = 0; i < count; ++i) {
        const auto& e = snapshot.entities[i];
        const std::size_t extra = e.kind == Kind::Player ? 17 : e.kind == Kind::Marker ? 7
            : e.kind == Kind::Label ? 1 + std::min(e.text.size(), std::size_t{48}) : 0;
        if (w.size() + 18 + extra + 13 + 4 * game::kItemCount + 7 > kMaxPacket) break;
        ++written;
        w.u8(static_cast<std::uint8_t>(e.kind));
        w.u8(e.id);
        w.f32(e.position.x);
        w.f32(e.position.y);
        w.f32(e.position.z);
        w.f32(e.yaw);
        if (e.kind == Kind::Player) {
            w.pad(e.name.c_str(), 16);
            w.u8(static_cast<std::uint8_t>(e.equipped));
        } else if (e.kind == Kind::Marker) {
            w.f32(e.size);
            w.u8(static_cast<std::uint8_t>(std::clamp(e.color.r, 0.0f, 1.0f) * 255.0f));
            w.u8(static_cast<std::uint8_t>(std::clamp(e.color.g, 0.0f, 1.0f) * 255.0f));
            w.u8(static_cast<std::uint8_t>(std::clamp(e.color.b, 0.0f, 1.0f) * 255.0f));
        } else if (e.kind == Kind::Label) {
            const auto n = static_cast<std::uint8_t>(std::min(e.text.size(), std::size_t{48}));
            w.u8(n);
            for (std::uint8_t c = 0; c < n; ++c) w.u8(static_cast<std::uint8_t>(e.text[c]));
        }
    }
    w.poke_u8(count_offset, written);
    w.u8(snapshot.hp);
    w.u8(snapshot.cold);
    w.u8(snapshot.o2);
    w.u8(snapshot.stamina);
    w.u8(snapshot.radiation);
    w.u16(snapshot.wood);
    w.u16(snapshot.stone);
    w.u8(snapshot.phase);
    w.u16(snapshot.time_left);
    w.u8(snapshot.night);
    for (auto stack : snapshot.inventory.count) w.u16(stack);
    for (auto durability : snapshot.inventory.durability) w.u16(durability);
    w.u8(static_cast<std::uint8_t>(snapshot.inventory.equipped));
    w.u32(snapshot.action_ack);
    w.u8(static_cast<std::uint8_t>(snapshot.feedback));
    w.u8(snapshot.stations);
    game_finish(w);
    return w.take();
}

bool unpack_type(const std::uint8_t* data, std::size_t size, Packet& type)
{
    if (!data || size < 8 || size > kMaxPacket) return false;
    ByteReader r(data, size);
    std::uint32_t magic = 0;
    std::uint8_t raw = 0, version = 0;
    std::uint16_t payload = 0;
    if (!r.u32(magic) || magic != kMagic) return false;
    if (!r.u8(raw) || !r.u8(version) || version != kVersion || !r.u16(payload)) return false;
    if (payload != size - 8 || raw < 1 || raw > kMaxPacketType) return false;
    type = static_cast<Packet>(raw);
    return true;
}

bool unpack_hello(const std::uint8_t* data, std::size_t size)
{
    ByteReader r(data, size);
    std::uint8_t version = 0;
    return game_header(r, Packet::Hello) && r.u8(version) && version == kVersion && r.done();
}

bool unpack_welcome(const std::uint8_t* data, std::size_t size, Welcome& welcome)
{
    ByteReader r(data, size);
    return game_header(r, Packet::Welcome) && r.u8(welcome.player_id) && r.u8(welcome.tick_hz)
        && r.f32(welcome.stream_radius) && welcome.player_id < 32 && welcome.tick_hz > 0
        && welcome.stream_radius > 0 && r.done();
}

bool unpack_input(const std::uint8_t* data, std::size_t size, Input& input)
{
    ByteReader r(data, size);
    std::uint8_t flags = 0, action = 0;
    if (!game_header(r, Packet::Input) || !r.u32(input.seq) || !r.f32(input.move_x) || !r.f32(input.move_z)
        || !r.f32(input.yaw) || !r.u8(flags) || flags > 15 || !r.u32(input.action_seq) || !r.u8(action)
        || !r.u8(input.argument) || action > static_cast<std::uint8_t>(game::Action::Discard) || !r.done()
        || std::abs(input.move_x) > 1.0f || std::abs(input.move_z) > 1.0f)
        return false;
    input.action = static_cast<game::Action>(action);
    if (input.action != game::Action::None
        && (input.action_seq == 0
            || (input.action == game::Action::Craft ? input.argument >= game::kRecipes.size()
                                                    : input.argument >= game::kItemCount)))
        return false;
    input.boost = (flags & 1) != 0;
    input.interact = (flags & 2) != 0;
    input.place = (flags & 4) != 0;
    input.jump = (flags & 8) != 0;
    return true;
}

bool unpack_snapshot(const std::uint8_t* data, std::size_t size, Snapshot& snapshot)
{
    ByteReader r(data, size);
    std::uint8_t count = 0;
    if (!game_header(r, Packet::Snapshot) || !r.u32(snapshot.tick) || !r.u8(snapshot.self) || !r.u32(snapshot.ack)
        || !r.u8(count) || count > kMaxSnapshotEntities || snapshot.self >= 32)
        return false;
    snapshot.entities.clear();
    snapshot.entities.reserve(count);
    for (std::uint8_t i = 0; i < count; ++i) {
        Ghost ghost;
        std::uint8_t kind = 0;
        if (!r.u8(kind) || !r.u8(ghost.id) || !r.f32(ghost.position.x) || !r.f32(ghost.position.y)
            || !r.f32(ghost.position.z) || !r.f32(ghost.yaw))
            return false;
        if (kind < 1 || kind > static_cast<std::uint8_t>(Kind::Loot)) return false;
        ghost.kind = static_cast<Kind>(kind);
        if (ghost.kind == Kind::Player) {
            std::uint8_t equipped = 0;
            if (!r.pad(ghost.name, 16) || !r.u8(equipped) || equipped >= game::kItemCount) return false;
            ghost.equipped = static_cast<game::Item>(equipped);
        } else if (ghost.kind == Kind::Marker) {
            std::uint8_t cr = 0, cg = 0, cb = 0;
            if (!r.f32(ghost.size) || !r.u8(cr) || !r.u8(cg) || !r.u8(cb)) return false;
            ghost.color = {cr / 255.0f, cg / 255.0f, cb / 255.0f, 1.0f};
        } else if (ghost.kind == Kind::Label) {
            std::uint8_t n = 0;
            if (!r.u8(n) || n > 48 || !r.need(n)) return false;
            ghost.text.assign(reinterpret_cast<const char*>(r.data() + r.offset()), n);
            if (!r.skip(n)) return false;
        }
        snapshot.entities.push_back(std::move(ghost));
    }
    const bool vitals = r.u8(snapshot.hp) && r.u8(snapshot.cold) && r.u8(snapshot.o2) && r.u8(snapshot.stamina)
        && r.u8(snapshot.radiation) && r.u16(snapshot.wood) && r.u16(snapshot.stone) && r.u8(snapshot.phase)
        && r.u16(snapshot.time_left) && r.u8(snapshot.night) && snapshot.hp <= 100 && snapshot.cold <= 100
        && snapshot.o2 <= 100 && snapshot.stamina <= 100 && snapshot.radiation <= 100 && snapshot.phase <= 2
        && snapshot.night <= 1;
    if (!vitals) return false;
    for (std::size_t i = 0; i < game::kItemCount; ++i)
        if (!r.u16(snapshot.inventory.count[i]) || snapshot.inventory.count[i] > game::kItems[i].stack) return false;
    for (std::size_t i = 0; i < game::kItemCount; ++i)
        if (!r.u16(snapshot.inventory.durability[i])
            || snapshot.inventory.durability[i] > game::kItems[i].durability)
            return false;
    std::uint8_t equipped = 0, feedback = 0;
    if (!r.u8(equipped) || equipped >= game::kItemCount || !r.u32(snapshot.action_ack) || !r.u8(feedback)
        || feedback > static_cast<std::uint8_t>(game::Result::NeedCargo) || !r.u8(snapshot.stations)
        || snapshot.stations > 3)
        return false;
    snapshot.inventory.equipped = static_cast<game::Item>(equipped);
    snapshot.feedback = static_cast<game::Result>(feedback);
    return r.done();
}

std::vector<std::uint8_t> pack_ping(std::uint32_t nonce)
{
    ByteWriter w;
    game_header(w, Packet::Ping);
    w.u32(nonce);
    game_finish(w);
    return w.take();
}

std::vector<std::uint8_t> pack_pong(std::uint32_t nonce)
{
    ByteWriter w;
    game_header(w, Packet::Pong);
    w.u32(nonce);
    game_finish(w);
    return w.take();
}

std::vector<std::uint8_t> pack_disconnect()
{
    ByteWriter w;
    game_header(w, Packet::Disconnect);
    game_finish(w);
    return w.take();
}

bool unpack_ping(const std::uint8_t* data, std::size_t size, std::uint32_t& nonce)
{
    ByteReader r(data, size);
    return game_header(r, Packet::Ping) && r.u32(nonce) && r.done();
}

bool unpack_pong(const std::uint8_t* data, std::size_t size, std::uint32_t& nonce)
{
    ByteReader r(data, size);
    return game_header(r, Packet::Pong) && r.u32(nonce) && r.done();
}

bool unpack_disconnect(const std::uint8_t* data, std::size_t size)
{
    ByteReader r(data, size);
    return game_header(r, Packet::Disconnect) && r.done();
}

} // namespace forge::net

