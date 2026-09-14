#include "engine/world/sectors/sectors.hpp"

#include <algorithm>

namespace forge::world {

void SectorIndex::build(const Store& store)
{
    by_sector_.clear();
    by_sector_.reserve(store.size());
    for (Entity id = 0; id < static_cast<Entity>(store.size()); ++id) {
        if ((store.flags[id] & kAlive) == 0) continue;
        by_sector_[store.sectors[id]].push_back(id);
    }
}

const std::vector<Entity>* SectorIndex::entities(SectorKey key) const
{
    const auto it = by_sector_.find(key);
    if (it == by_sector_.end()) return nullptr;
    return &it->second;
}

void SectorIndex::stream(Store& store, const glm::vec3& camera, float radius, StreamStats& stats) const
{
    for (auto& flag : store.flags) flag &= ~kStreamed;
    for (auto& [key, state] : residency_)
        if (state == Residency::Resident) state = Residency::Unloading;
    stats.sectors_loaded = 0;
    stats.entities_streamed = 0;
    const float limit = std::max(radius, 0.0f);
    const float prefetch = limit + kSectorSize;
    for (const auto& [key, entities] : by_sector_) {
        const float distance = sector_distance_xz(unpack_sector(key), camera);
        if (distance > prefetch) {
            residency_[key] = Residency::Empty;
            continue;
        }
        if (distance > limit) {
            residency_[key] = Residency::Prefetch;
            continue;
        }
        residency_[key] = Residency::Resident;
        ++stats.sectors_loaded;
        for (Entity id : entities) {
            if (id >= store.flags.size()) continue;
            if ((store.flags[id] & kAlive) == 0) continue;
            store.flags[id] |= kStreamed;
            ++stats.entities_streamed;
        }
    }
}

Residency SectorIndex::residency(SectorKey key) const
{
    const auto it = residency_.find(key);
    if (it == residency_.end()) return Residency::Empty;
    return it->second;
}

} // namespace forge::world
