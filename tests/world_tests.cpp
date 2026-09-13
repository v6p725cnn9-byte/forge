#include "engine/core/camera.hpp"
#include "engine/world/world.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
}

int main()
{
    using namespace forge::world;

    check(sector_of({0, 0, 0}) == SectorCoord{0, 0}, "Origin belongs to sector (0,0)");
    check(sector_of({32, 0, 32}) == SectorCoord{1, 1}, "32m stride must step into the next sector");
    check(sector_of({-0.1f, 0, -0.1f}) == SectorCoord{-1, -1}, "Negative positions must floor into -1");
    const auto key = pack_sector({-3, 7});
    check(unpack_sector(key).x == -3 && unpack_sector(key).z == 7, "Sector pack must round-trip signed coords");
    check(near(sector_distance_xz({0, 0}, {16, 0, 16}), 0.0f), "Camera inside a sector has zero stream distance");
    check(near(sector_distance_xz({1, 0}, {16, 0, 16}), 16.0f), "Distance to the neighbouring sector AABB");

    Store store;
    const auto a = store.spawn({4, 1, 4}, 0, {1, 2, 1}, 0, {1, 0, 0, 1});
    const auto b = store.spawn({80, 1, 4}, 15, {2, 2, 2}, 1, {0, 1, 0, 1});
    check(a == 0 && b == 1, "Spawn must return dense ids");
    check(store.size() == 2 && store.positions.size() == store.flags.size()
              && store.world_aabbs.size() == store.colors.size() && store.yaws.size() == store.meshes.size(),
          "SoA arrays must stay the same length");
    check(store.sectors[a] == pack_sector({0, 0}) && store.sectors[b] == pack_sector({2, 0}),
          "Entities must record the sector of their position");
    check(store.world_aabbs[a].min.y < 1.0f && store.world_aabbs[a].max.y > 1.0f,
          "World AABB must include the scaled local bounds");

    SectorIndex index;
    index.build(store);
    check(index.sector_count() == 2, "Index must keep one bucket per occupied sector");
    StreamStats stats;
    index.stream(store, {4, 0, 4}, 40.0f, stats);
    check(stats.sectors_loaded == 1 && stats.entities_streamed == 1, "Far sector must stay unloaded at 40m");
    check((store.flags[a] & kStreamed) != 0 && (store.flags[b] & kStreamed) == 0,
          "Only the near entity is streamed");
    index.stream(store, {4, 0, 4}, 80.0f, stats);
    check(stats.sectors_loaded == 2 && (store.flags[b] & kStreamed) != 0, "Larger radius must load the far sector");

    forge::Camera camera;
    camera.position = {0, 0, 0};
    camera.yaw = -90.0f;
    camera.pitch = 0.0f;
    camera.near_plane = 0.1f;
    camera.far_plane = 100.0f;
    camera.vertical_fov = 60.0f;
    const auto frustum = frustum_from_clip(camera.projection(1.0f) * camera.view());
    check(aabb_visible(frustum, {{-0.5f, -0.5f, -5.5f}, {0.5f, 0.5f, -4.5f}}), "Box in front of the camera is visible");
    check(!aabb_visible(frustum, {{-0.5f, -0.5f, 4.5f}, {0.5f, 0.5f, 5.5f}}), "Box behind the camera is culled");
    check(!aabb_visible(frustum, {{-0.5f, -0.5f, -200.5f}, {0.5f, 0.5f, -199.5f}}), "Box past the far plane is culled");

    Store visible;
    visible.spawn({0, 0, -5}, 0, {1, 1, 1}, 0, {1, 1, 1, 1});
    visible.spawn({0, 0, 5}, 0, {1, 1, 1}, 0, {1, 1, 1, 1});
    visible.spawn({0, 0, -5}, 0, {1, 1, 1}, 1, {1, 1, 1, 1});
    for (auto& flag : visible.flags) flag |= kStreamed;
    std::vector<GpuInstance> instances;
    collect_instances(visible, &frustum, 0, instances, stats);
    check(stats.instances_drawn == 1 && instances.size() == 1, "Frustum + mesh filter keep one instance");
    check(stats.instances_culled == 2, "Behind-camera and other-mesh entities count as culled");
    collect_instances(visible, nullptr, 0, instances, stats);
    check(stats.instances_drawn == 2, "Null frustum disables AABB tests");

    Store city;
    const auto spawned = populate_city(city, {0, 0, 0, 0});
    check(spawned == 16 && city.size() == 16, "A single city sector has a fixed prop count");
    SectorIndex city_index;
    city_index.build(city);
    check(city_index.sector_count() == 1, "One sector of props occupies one bucket");
    for (std::size_t i = 0; i < city.size(); ++i) {
        check(city.positions.size() == city.yaws.size() && city.scales.size() == city.meshes.size(),
              "City spawn must keep SoA rows aligned");
        check(city.sectors[i] == pack_sector({0, 0}), "Single-sector city stays in (0,0)");
    }

    store.clear();
    check(store.size() == 0, "Clear must empty every SoA array");
    std::cout << "World checks passed\n";
}
