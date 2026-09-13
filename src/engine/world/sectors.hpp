#pragma once

#include "engine/world/store.hpp"

#include <unordered_map>
#include <vector>

namespace forge::world {

class SectorIndex {
public:
    void build(const Store& store);
    void stream(Store& store, const glm::vec3& camera, float radius, StreamStats& stats) const;
    const std::vector<Entity>* entities(SectorKey key) const;
    std::size_t sector_count() const { return by_sector_.size(); }

private:
    std::unordered_map<SectorKey, std::vector<Entity>> by_sector_;
};

} // namespace forge::world
