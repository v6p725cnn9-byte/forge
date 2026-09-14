#include "engine/world/entities/city.hpp"

#include <array>
#include <cstdint>

namespace forge::world {
namespace {

std::uint32_t hash3(int x, int z, int k)
{
    auto h = static_cast<std::uint32_t>(x) * 374761393u ^ static_cast<std::uint32_t>(z) * 668265263u
        ^ static_cast<std::uint32_t>(k) * 1274126177u;
    h ^= h >> 13;
    h *= 1274126177u;
    return h;
}

} // namespace

std::uint32_t populate_city(Store& store, const CitySpec& spec)
{
    const std::array<glm::vec4, 8> palette{{
        {0.62f, 0.38f, 0.32f, 1.0f}, {0.55f, 0.56f, 0.52f, 1.0f}, {0.30f, 0.42f, 0.48f, 1.0f},
        {0.70f, 0.62f, 0.48f, 1.0f}, {0.40f, 0.36f, 0.34f, 1.0f}, {0.50f, 0.45f, 0.55f, 1.0f},
        {0.38f, 0.48f, 0.40f, 1.0f}, {0.58f, 0.48f, 0.38f, 1.0f},
    }};
    const auto before = store.size();
    for (int sz = spec.min_z; sz <= spec.max_z; ++sz) {
        for (int sx = spec.min_x; sx <= spec.max_x; ++sx) {
            const float ox = static_cast<float>(sx) * kSectorSize;
            const float oz = static_cast<float>(sz) * kSectorSize;
            const glm::vec4 ground = ((sx + sz) & 1) ? glm::vec4{0.18f, 0.20f, 0.23f, 1.0f}
                                                     : glm::vec4{0.16f, 0.18f, 0.21f, 1.0f};
            store.spawn({ox + 16.0f, -0.15f, oz + 16.0f}, 0.0f, {32.0f, 0.3f, 32.0f}, 0, ground);
            for (int lz = 0; lz < 3; ++lz) {
                for (int lx = 0; lx < 3; ++lx) {
                    const auto h = hash3(sx, sz, lx * 3 + lz);
                    const float width = 5.6f + static_cast<float>(h % 3) * 0.45f;
                    const float height = 5.0f + static_cast<float>(h % 12);
                    const float x = ox + 5.5f + static_cast<float>(lx) * 10.5f;
                    const float z = oz + 5.5f + static_cast<float>(lz) * 10.5f;
                    store.spawn({x, height * 0.5f, z}, 0.0f, {width, height, width}, 0, palette[h % palette.size()]);
                }
            }
            const glm::vec4 lamp{0.95f, 0.85f, 0.55f, 1.0f};
            store.spawn({ox + 16.0f, 2.0f, oz + 2.0f}, 0.0f, {0.2f, 4.0f, 0.2f}, 0, lamp);
            store.spawn({ox + 16.0f, 2.0f, oz + 30.0f}, 0.0f, {0.2f, 4.0f, 0.2f}, 0, lamp);
            store.spawn({ox + 2.0f, 2.0f, oz + 16.0f}, 0.0f, {0.2f, 4.0f, 0.2f}, 0, lamp);
            store.spawn({ox + 30.0f, 2.0f, oz + 16.0f}, 0.0f, {0.2f, 4.0f, 0.2f}, 0, lamp);
            const glm::vec4 crate{0.55f, 0.32f, 0.18f, 1.0f};
            const float yaw_a = static_cast<float>(hash3(sx, sz, 40) % 90);
            const float yaw_b = static_cast<float>(hash3(sx, sz, 41) % 90);
            store.spawn({ox + 16.0f, 0.45f, oz + 10.0f}, yaw_a, {0.9f, 0.9f, 0.9f}, 0, crate);
            store.spawn({ox + 11.0f, 0.35f, oz + 16.0f}, yaw_b, {0.7f, 0.7f, 0.7f}, 0, crate);
        }
    }
    return static_cast<std::uint32_t>(store.size() - before);
}

} // namespace forge::world
