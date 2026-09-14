#include "engine/game/systems/save.hpp"

#include "engine/net/transport/stream.hpp"

#include <algorithm>

namespace forge::game::save {

Blob capture(const Sim& sim)
{
    Blob blob;
    blob.phase = sim.phase();
    blob.time_left = sim.time_left();
    blob.clock = sim.day01();
    blob.nodes = sim.nodes();
    blob.pawns.reserve(kMaxPlayers);
    for (int id = 0; id < kMaxPlayers; ++id) {
        if (const auto* pawn = sim.pawn(id)) blob.pawns.push_back(*pawn);
        else blob.pawns.push_back({});
    }
    return blob;
}

bool restore(Sim& sim, const Blob& blob)
{
    if (blob.magic != kMagic || blob.version != kVersion) return false;
    sim.reset();
    for (int id = 0; id < kMaxPlayers && id < static_cast<int>(blob.pawns.size()); ++id) {
        if (!blob.pawns[static_cast<std::size_t>(id)].used) continue;
        sim.ensure_pawn(id);
        if (auto* pawn = sim.pawn(id)) *pawn = blob.pawns[static_cast<std::size_t>(id)];
    }
    return true;
}

std::vector<std::uint8_t> encode(const Blob& blob)
{
    net::ByteWriter w;
    w.u32(blob.magic);
    w.u16(blob.version);
    w.u8(static_cast<std::uint8_t>(blob.phase));
    w.f32(blob.time_left);
    w.u8(static_cast<std::uint8_t>(std::min(blob.pawns.size(), static_cast<std::size_t>(kMaxPlayers))));
    for (std::size_t i = 0; i < blob.pawns.size() && i < static_cast<std::size_t>(kMaxPlayers); ++i) {
        const auto& pawn = blob.pawns[i];
        w.u8(pawn.used ? 1 : 0);
        w.f32(pawn.hp);
        for (auto count : pawn.inventory.count) w.u16(count);
    }
    return w.take();
}

bool decode(const std::uint8_t* data, std::size_t size, Blob& blob)
{
    net::ByteReader r(data, size);
    std::uint8_t phase = 0, count = 0;
    if (!r.u32(blob.magic) || blob.magic != kMagic || !r.u16(blob.version) || blob.version != kVersion || !r.u8(phase)
        || !r.f32(blob.time_left) || !r.u8(count) || count > kMaxPlayers)
        return false;
    blob.phase = static_cast<Phase>(phase);
    blob.pawns.assign(count, {});
    for (std::uint8_t i = 0; i < count; ++i) {
        std::uint8_t used = 0;
        if (!r.u8(used) || !r.f32(blob.pawns[i].hp)) return false;
        blob.pawns[i].used = used != 0;
        for (auto& stack : blob.pawns[i].inventory.count)
            if (!r.u16(stack)) return false;
    }
    return true;
}

} // namespace forge::game::save
