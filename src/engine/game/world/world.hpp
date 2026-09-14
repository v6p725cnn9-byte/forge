#pragma once

#include "engine/game/actors/actors.hpp"
#include "engine/game/session/session.hpp"

namespace forge::game {

// Aggregate root for gameplay. Everything that is "the session" hangs here:
// actors, sim rules, and later weather/AI/quests/events/loot/machines/doors.
// There is no second authority. world:: scenery SoA is props only.
class World {
public:
    Actors actors;
    Sim sim;
};

} // namespace forge::game
