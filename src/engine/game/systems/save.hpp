#pragma once

#include "engine/game/session/session.hpp"

#include <cstdint>
#include <vector>

namespace forge::game::save {

constexpr std::uint32_t kMagic = 0x45565346u; // FSVE
constexpr std::uint16_t kVersion = 1;

struct Blob {
    std::uint32_t magic = kMagic;
    std::uint16_t version = kVersion;
    Phase phase = Phase::Play;
    float time_left = 0;
    float clock = 0;
    std::vector<Pawn> pawns;
    std::vector<Node> nodes;
};

Blob capture(const Sim& sim);
bool restore(Sim& sim, const Blob& blob);
std::vector<std::uint8_t> encode(const Blob& blob);
bool decode(const std::uint8_t* data, std::size_t size, Blob& blob);

} // namespace forge::game::save
