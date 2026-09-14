#include "engine/net/protocol.hpp"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace forge::net {
namespace {

struct Writer {
    std::vector<std::uint8_t> bytes;
    void u8(std::uint8_t v) { bytes.push_back(v); }
    void u16(std::uint16_t v)
    {
        bytes.push_back(static_cast<std::uint8_t>(v));
        bytes.push_back(static_cast<std::uint8_t>(v >> 8));
    }
    void u32(std::uint32_t v)
    {
        bytes.push_back(static_cast<std::uint8_t>(v));
        bytes.push_back(static_cast<std::uint8_t>(v >> 8));
        bytes.push_back(static_cast<std::uint8_t>(v >> 16));
        bytes.push_back(static_cast<std::uint8_t>(v >> 24));
    }
    void f32(float v)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &v, 4);
        u32(bits);
    }
    void pad(const char* text, std::size_t width)
    {
        const std::size_t n = text ? std::strlen(text) : 0;
        for (std::size_t i = 0; i < width; ++i) u8(i < n ? static_cast<std::uint8_t>(text[i]) : 0);
    }
    void header(Packet type)
    {
        u32(kMagic);
        u8(static_cast<std::uint8_t>(type));
        u8(kVersion);
        u16(0);
    }
    void finish()
    {
        const auto size = static_cast<std::uint16_t>(bytes.size() - 8);
        bytes[6] = static_cast<std::uint8_t>(size);
        bytes[7] = static_cast<std::uint8_t>(size >> 8);
    }
};

struct Reader {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t offset = 0;
    bool need(std::size_t n) const { return data && offset <= size && n <= size - offset; }
    bool u8(std::uint8_t& v)
    {
        if (!need(1)) return false;
        v = data[offset++];
        return true;
    }
    bool u16(std::uint16_t& v)
    {
        std::uint8_t a = 0, b = 0;
        if (!u8(a) || !u8(b)) return false;
        v = static_cast<std::uint16_t>(a | (static_cast<std::uint16_t>(b) << 8));
        return true;
    }
    bool u32(std::uint32_t& v)
    {
        std::uint16_t lo = 0, hi = 0;
        if (!u16(lo) || !u16(hi)) return false;
        v = static_cast<std::uint32_t>(lo) | (static_cast<std::uint32_t>(hi) << 16);
        return true;
    }
    bool f32(float& v)
    {
        std::uint32_t bits = 0;
        if (!u32(bits)) return false;
        std::memcpy(&v, &bits, 4);
        return std::isfinite(v);
    }
    bool pad(std::string& text, std::size_t width)
    {
        if (!need(width)) return false;
        text.assign(reinterpret_cast<const char*>(data + offset), width);
        const auto zero = text.find('\0');
        if (zero != std::string::npos) text.resize(zero);
        offset += width;
        return true;
    }
    bool header(Packet expected)
    {
        if (size > kMaxPacket) return false;
        std::uint32_t magic = 0;
        std::uint8_t type = 0, version = 0;
        std::uint16_t payload = 0;
        if (!u32(magic) || magic != kMagic) return false;
        if (!u8(type) || type != static_cast<std::uint8_t>(expected)) return false;
        if (!u8(version) || version != kVersion) return false;
        if (!u16(payload) || offset + payload != size) return false;
        return true;
    }
};

} // namespace

std::vector<std::uint8_t> pack_hello()
{
    Writer w;
    w.header(Packet::Hello);
    w.u8(kVersion);
    w.finish();
    return w.bytes;
}

std::vector<std::uint8_t> pack_welcome(const Welcome& welcome)
{
    Writer w;
    w.header(Packet::Welcome);
    w.u8(welcome.player_id);
    w.u8(welcome.tick_hz);
    w.f32(welcome.stream_radius);
    w.finish();
    return w.bytes;
}

std::vector<std::uint8_t> pack_input(const Input& input)
{
    Writer w;
    w.header(Packet::Input);
    w.u32(input.seq);
    w.f32(input.move_x);
    w.f32(input.move_z);
    w.f32(input.yaw);
    std::uint8_t flags = 0;
    if (input.boost) flags |= 1;
    if (input.interact) flags |= 2;
    if (input.place) flags |= 4;
    w.u8(flags);
    w.finish();
    return w.bytes;
}

std::vector<std::uint8_t> pack_snapshot(const Snapshot& snapshot)
{
    Writer w;
    w.header(Packet::Snapshot);
    w.u32(snapshot.tick);
    w.u8(snapshot.self);
    w.u32(snapshot.ack);
    const auto count = static_cast<std::uint8_t>(std::min(snapshot.entities.size(), static_cast<std::size_t>(kMaxSnapshotEntities)));
    const auto count_offset = w.bytes.size();
    w.u8(0);
    std::uint8_t written = 0;
    for (std::uint8_t i = 0; i < count; ++i) {
        const auto& e = snapshot.entities[i];
        const std::size_t extra = e.kind == Kind::Player ? 16 : e.kind == Kind::Marker ? 7
            : e.kind == Kind::Label ? 1 + std::min(e.text.size(), std::size_t{48}) : 0;
        // Reserve the ten-byte survival trailer within the UDP budget.
        if (w.bytes.size() + 18 + extra + 10 > kMaxPacket) break;
        ++written;
        w.u8(static_cast<std::uint8_t>(e.kind));
        w.u8(e.id);
        w.f32(e.position.x);
        w.f32(e.position.y);
        w.f32(e.position.z);
        w.f32(e.yaw);
        if (e.kind == Kind::Player) w.pad(e.name.c_str(), 16);
        else if (e.kind == Kind::Marker) {
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
    w.bytes[count_offset] = written;
    w.u8(snapshot.hp);
    w.u8(snapshot.cold);
    w.u16(snapshot.wood);
    w.u16(snapshot.stone);
    w.u8(snapshot.phase);
    w.u16(snapshot.time_left);
    w.u8(snapshot.night);
    w.finish();
    return w.bytes;
}

bool unpack_type(const std::uint8_t* data, std::size_t size, Packet& type)
{
    if (!data || size < 8 || size > kMaxPacket) return false;
    Reader r{data, size, 0};
    std::uint32_t magic = 0;
    std::uint8_t raw = 0, version = 0;
    std::uint16_t payload = 0;
    if (!r.u32(magic) || magic != kMagic) return false;
    if (!r.u8(raw) || !r.u8(version) || version != kVersion || !r.u16(payload)) return false;
    if (payload != size - 8 || raw < 1 || raw > 4) return false;
    type = static_cast<Packet>(raw);
    return true;
}

bool unpack_hello(const std::uint8_t* data, std::size_t size)
{
    Reader r{data, size, 0};
    std::uint8_t version = 0;
    return r.header(Packet::Hello) && r.u8(version) && version == kVersion && r.offset == size;
}

bool unpack_welcome(const std::uint8_t* data, std::size_t size, Welcome& welcome)
{
    Reader r{data, size, 0};
    return r.header(Packet::Welcome) && r.u8(welcome.player_id) && r.u8(welcome.tick_hz) && r.f32(welcome.stream_radius)
        && welcome.player_id < 32 && welcome.tick_hz > 0 && welcome.stream_radius > 0 && r.offset == size;
}

bool unpack_input(const std::uint8_t* data, std::size_t size, Input& input)
{
    Reader r{data, size, 0};
    std::uint8_t flags = 0;
    if (!r.header(Packet::Input) || !r.u32(input.seq) || !r.f32(input.move_x) || !r.f32(input.move_z) || !r.f32(input.yaw)
        || !r.u8(flags) || flags > 7 || r.offset != size
        || std::abs(input.move_x) > 1.0f || std::abs(input.move_z) > 1.0f)
        return false;
    input.boost = (flags & 1) != 0;
    input.interact = (flags & 2) != 0;
    input.place = (flags & 4) != 0;
    return true;
}

bool unpack_snapshot(const std::uint8_t* data, std::size_t size, Snapshot& snapshot)
{
    Reader r{data, size, 0};
    std::uint8_t count = 0;
    if (!r.header(Packet::Snapshot) || !r.u32(snapshot.tick) || !r.u8(snapshot.self) || !r.u32(snapshot.ack) || !r.u8(count)
        || count > kMaxSnapshotEntities || snapshot.self >= 32)
        return false;
    snapshot.entities.clear();
    snapshot.entities.reserve(count);
    for (std::uint8_t i = 0; i < count; ++i) {
        Ghost ghost;
        std::uint8_t kind = 0;
        if (!r.u8(kind) || !r.u8(ghost.id) || !r.f32(ghost.position.x) || !r.f32(ghost.position.y)
            || !r.f32(ghost.position.z) || !r.f32(ghost.yaw))
            return false;
        if (kind < 1 || kind > 8) return false;
        ghost.kind = static_cast<Kind>(kind);
        if (ghost.kind == Kind::Player) {
            if (!r.pad(ghost.name, 16)) return false;
        } else if (ghost.kind == Kind::Marker) {
            std::uint8_t cr = 0, cg = 0, cb = 0;
            if (!r.f32(ghost.size) || !r.u8(cr) || !r.u8(cg) || !r.u8(cb)) return false;
            ghost.color = {cr / 255.0f, cg / 255.0f, cb / 255.0f, 1.0f};
        } else if (ghost.kind == Kind::Label) {
            std::uint8_t n = 0;
            if (!r.u8(n) || n > 48 || !r.need(n)) return false;
            ghost.text.assign(reinterpret_cast<const char*>(r.data + r.offset), n);
            r.offset += n;
        }
        snapshot.entities.push_back(std::move(ghost));
    }
    return r.u8(snapshot.hp) && r.u8(snapshot.cold) && r.u16(snapshot.wood) && r.u16(snapshot.stone)
        && r.u8(snapshot.phase) && r.u16(snapshot.time_left) && r.u8(snapshot.night)
        && snapshot.hp <= 100 && snapshot.cold <= 100 && snapshot.phase <= 2 && snapshot.night <= 1
        && r.offset == size;
}

} // namespace forge::net
