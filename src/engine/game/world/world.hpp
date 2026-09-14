#pragma once

#include "engine/game/actors/actors.hpp"
#include "engine/game/session/session.hpp"

namespace forge::game {

// Single authoritative gameplay state. Spatial scenery (M4 SoA store) is a
// different domain and must not duplicate actor positions.
struct World {
    Actors actors;
    Sim sim;
};

} // namespace forge::game
