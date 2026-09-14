#pragma once

#include "engine/world/scenery/store.hpp"

#include <unordered_map>
#include <vector>

namespace forge::world {

// Async loader will fill Loading → CpuReady → GpuUploading. The sync path
// currently jumps Prefetch or Resident; do not collapse these states.
enum class Residency {
    Empty,
    Prefetch,
    Loading,
    CpuReady,
    GpuUploading,
    Resident,
    Unloading,
};

class SectorIndex {
public:
    void build(const Store& store);
    void stream(Store& store, const glm::vec3& camera, float radius, StreamStats& stats) const;
    const std::vector<Entity>* entities(SectorKey key) const;
    std::size_t sector_count() const { return by_sector_.size(); }
    Residency residency(SectorKey key) const;

private:
    std::unordered_map<SectorKey, std::vector<Entity>> by_sector_;
    mutable std::unordered_map<SectorKey, Residency> residency_;
};

} // namespace forge::world
