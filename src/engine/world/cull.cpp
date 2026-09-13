#include "engine/world/cull.hpp"

namespace forge::world {

void collect_instances(const Store& store, const Frustum* frustum, std::uint16_t mesh,
                       std::vector<GpuInstance>& out, StreamStats& stats)
{
    out.clear();
    stats.instances_drawn = 0;
    stats.instances_culled = 0;
    out.reserve(store.size());
    for (Entity id = 0; id < static_cast<Entity>(store.size()); ++id) {
        if ((store.flags[id] & (kAlive | kStreamed)) != (kAlive | kStreamed)) continue;
        if (store.meshes[id] != mesh || (frustum && !aabb_visible(*frustum, store.world_aabbs[id]))) {
            ++stats.instances_culled;
            continue;
        }
        GpuInstance instance;
        instance.world = entity_world(store.positions[id], store.yaws[id], store.scales[id]);
        instance.color = store.colors[id];
        out.push_back(instance);
        ++stats.instances_drawn;
    }
}

} // namespace forge::world
