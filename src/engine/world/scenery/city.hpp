#pragma once

#include "engine/world/scenery/store.hpp"

#include <cstdint>

namespace forge::world {

struct CitySpec {
    int min_x = -4;
    int max_x = 3;
    int min_z = -4;
    int max_z = 3;
};

std::uint32_t populate_city(Store& store, const CitySpec& spec = {});

} // namespace forge::world
