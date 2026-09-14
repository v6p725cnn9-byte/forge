#pragma once

#include "engine/world/sectors/sectors.hpp"

namespace forge::assets {

// GPU/CPU residency follows world sector state. Prefetch means keep the CPU
// asset; Resident means the GPU mesh may stay uploaded.
inline bool gpu_resident(world::Residency state)
{
    return state == world::Residency::Resident;
}

inline bool cpu_resident(world::Residency state)
{
    return state == world::Residency::Resident || state == world::Residency::Prefetch
        || state == world::Residency::Loading;
}

} // namespace forge::assets
