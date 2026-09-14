#pragma once

#include "engine/game/session/session.hpp"

namespace forge::game {

inline Result harvest_or_hit(Sim& sim, int player_id, Actors& actors)
{
    sim.harvest(player_id, actors);
    if (const auto* pawn = sim.pawn(player_id)) return pawn->feedback;
    return Result::Invalid;
}

} // namespace forge::game
