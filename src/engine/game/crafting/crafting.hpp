#pragma once

#include "engine/game/inventory/items.hpp"

namespace forge::game {

inline const Recipe* recipe(std::uint8_t id)
{
    if (id >= kRecipes.size()) return nullptr;
    return &kRecipes[id];
}

} // namespace forge::game
