#include "engine/assets/gltf/scene.hpp"

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace forge::assets {
namespace {

// Deterministic hash noise: no asset files, same vista every run.
float hash2(int x, int y)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0x00ffffffu) / 16777216.0f;
}

float vnoise(float x, float y)
{
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    const float xf = x - static_cast<float>(xi);
    const float yf = y - static_cast<float>(yi);
    const float u = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
    const float v = yf * yf * yf * (yf * (yf * 6.0f - 15.0f) + 10.0f);
    const float a = hash2(xi, yi);
    const float b = hash2(xi + 1, yi);
    const float c = hash2(xi, yi + 1);
    const float d = hash2(xi + 1, yi + 1);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

float fbm(float x, float y, int octaves)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * vnoise(x * freq, y * freq);
        amp *= 0.5f;
        freq *= 2.03f;
    }
    return sum;
}

float smoothstep(float edge0, float edge1, float x)
{
    const float t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.0f), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float vista_height(float x, float z)
{
    const float r = std::sqrt(x * x + z * z);
    const float rolling = (fbm(x * 0.012f + 7.3f, z * 0.012f + 2.1f, 4) - 0.5f) * 14.0f;
    const float ridge_line = fbm(x * 0.004f + 1.7f, z * 0.004f + 8.4f, 3);
    const float ridge = smoothstep(140.0f, 260.0f, r) * (25.0f + ridge_line * 55.0f);
    float h = 2.0f + rolling + ridge;
    const float camp = 1.0f - smoothstep(6.0f, 20.0f, r);
    h = h * (1.0f - camp) + 1.5f * camp;
    const float dx = (x - 140.0f) / 110.0f;
    const float dz = z / 80.0f;
    const float lake = 1.0f - smoothstep(0.55f, 1.0f, dx * dx + dz * dz);
    h = h * (1.0f - lake) - 3.0f * lake;
    return h;
}

glm::vec3 ground_color(float h, float slope_up, float x, float z)
{
    const glm::vec3 grass{0.15f, 0.26f, 0.10f};
    const glm::vec3 dry{0.38f, 0.34f, 0.16f};
    const glm::vec3 rock{0.30f, 0.28f, 0.26f};
    const glm::vec3 snow{0.78f, 0.80f, 0.84f};
    const glm::vec3 sand{0.46f, 0.38f, 0.24f};
    const glm::vec3 bed{0.10f, 0.13f, 0.12f};
    const float patch = fbm(x * 0.05f + 3.1f, z * 0.05f + 9.2f, 2);
    glm::vec3 base = grass * (1.0f - patch) + dry * patch;
    base = base * (1.0f - (1.0f - smoothstep(0.55f, 0.75f, slope_up)))
        + rock * (1.0f - smoothstep(0.55f, 0.75f, slope_up));
    base = base * (1.0f - smoothstep(38.0f, 60.0f, h)) + snow * smoothstep(38.0f, 60.0f, h);
    const float shore = 1.0f - smoothstep(-0.5f, 1.2f, h);
    base = base * (1.0f - shore) + sand * shore;
    const float under = 1.0f - smoothstep(-2.5f, -0.5f, h);
    return base * (1.0f - under) + bed * under;
}

} // namespace

Scene make_vista_terrain(const glm::vec3& sun_direction)
{
    Scene scene;
    constexpr int kGrid = 150;
    constexpr float kSize = 440.0f;
    constexpr float kStep = kSize / static_cast<float>(kGrid - 1);

    Material ground;
    ground.name = "vista_ground";
    ground.metallic = 0.0f;
    ground.roughness = 0.95f;
    scene.materials.push_back(ground);
    Material water;
    water.name = "vista_water";
    water.base_color_factor = glm::vec4{0.10f, 0.16f, 0.22f, 1.0f};
    water.metallic = 0.0f;
    water.roughness = 0.12f;
    scene.materials.push_back(water);
    Material sky;
    sky.name = "vista_sky";
    sky.unlit = true;
    sky.double_sided = true;
    scene.materials.push_back(sky);

    // Ground heightfield with computed normals and slope/height paint.
    const std::uint32_t ground_base = static_cast<std::uint32_t>(scene.vertices.size());
    for (int iz = 0; iz < kGrid; ++iz) {
        for (int ix = 0; ix < kGrid; ++ix) {
            const float x = (static_cast<float>(ix) - (kGrid - 1) * 0.5f) * kStep;
            const float z = (static_cast<float>(iz) - (kGrid - 1) * 0.5f) * kStep;
            const float h = vista_height(x, z);
            const float hx = vista_height(x + kStep, z) - vista_height(x - kStep, z);
            const float hz = vista_height(x, z + kStep) - vista_height(x, z - kStep);
            glm::vec3 normal = glm::normalize(glm::vec3{-hx / (2.0f * kStep), 1.0f, -hz / (2.0f * kStep)});
            Vertex v{};
            v.position = {x, h, z};
            v.normal = normal;
            v.color = glm::vec4{ground_color(h, normal.y, x, z), 1.0f};
            v.uv0 = {x / kSize + 0.5f, z / kSize + 0.5f};
            scene.vertices.push_back(v);
            scene.bounds_min = glm::min(scene.bounds_min, v.position);
            scene.bounds_max = glm::max(scene.bounds_max, v.position);
        }
    }
    Primitive ground_prim{};
    ground_prim.first_index = static_cast<std::uint32_t>(scene.indices.size());
    for (int iz = 0; iz < kGrid - 1; ++iz) {
        for (int ix = 0; ix < kGrid - 1; ++ix) {
            const std::uint32_t a = ground_base + static_cast<std::uint32_t>(iz * kGrid + ix);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(kGrid);
            const std::uint32_t d = c + 1;
            scene.indices.insert(scene.indices.end(), {a, c, b, b, c, d});
        }
    }
    ground_prim.index_count = static_cast<std::uint32_t>(scene.indices.size()) - ground_prim.first_index;
    ground_prim.material = 0;
    scene.primitives.push_back(ground_prim);

    // Still lake plane inside the carved basin.
    const std::uint32_t water_base = static_cast<std::uint32_t>(scene.vertices.size());
    constexpr float kWaterY = -0.5f;
    for (int iz = 0; iz <= 8; ++iz) {
        for (int ix = 0; ix <= 8; ++ix) {
            Vertex v{};
            v.position = {20.0f + static_cast<float>(ix) * 31.25f, kWaterY, -110.0f + static_cast<float>(iz) * 27.5f};
            v.uv0 = {static_cast<float>(ix) / 8.0f, static_cast<float>(iz) / 8.0f};
            scene.vertices.push_back(v);
            scene.bounds_min = glm::min(scene.bounds_min, v.position);
            scene.bounds_max = glm::max(scene.bounds_max, v.position);
        }
    }
    Primitive water_prim{};
    water_prim.first_index = static_cast<std::uint32_t>(scene.indices.size());
    for (int iz = 0; iz < 8; ++iz) {
        for (int ix = 0; ix < 8; ++ix) {
            const std::uint32_t a = water_base + static_cast<std::uint32_t>(iz * 9 + ix);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + 9;
            const std::uint32_t d = c + 1;
            scene.indices.insert(scene.indices.end(), {a, c, b, b, c, d});
        }
    }
    water_prim.index_count = static_cast<std::uint32_t>(scene.indices.size()) - water_prim.first_index;
    water_prim.material = 1;
    scene.primitives.push_back(water_prim);

    // Gradient sky dome, unlit vertex colors, sun glow baked toward the sun.
    const std::uint32_t sky_base = static_cast<std::uint32_t>(scene.vertices.size());
    constexpr int kSlices = 32;
    constexpr int kStacks = 12;
    constexpr float kDomeR = 400.0f;
    const glm::vec3 zenith{0.10f, 0.16f, 0.32f};
    const glm::vec3 horizon{0.98f, 0.55f, 0.30f};
    const glm::vec3 below{0.08f, 0.08f, 0.10f};
    const glm::vec3 sun_tint{1.0f, 0.75f, 0.5f};
    for (int iy = 0; iy <= kStacks; ++iy) {
        const float v = static_cast<float>(iy) / static_cast<float>(kStacks);
        const float phi = v * 3.14159265f;
        for (int ix = 0; ix <= kSlices; ++ix) {
            const float u = static_cast<float>(ix) / static_cast<float>(kSlices);
            const float theta = u * 2.0f * 3.14159265f;
            const glm::vec3 dir{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
            Vertex vert{};
            vert.position = dir * kDomeR;
            vert.normal = -dir;
            glm::vec3 col = dir.y >= 0.0f ? horizon * (1.0f - std::pow(dir.y, 0.55f)) + zenith * std::pow(dir.y, 0.55f)
                                          : horizon * (1.0f - std::min(-dir.y * 3.0f, 1.0f))
                    + below * std::min(-dir.y * 3.0f, 1.0f);
            const float sun = std::max(glm::dot(dir, sun_direction), 0.0f);
            col += sun_tint * (std::pow(sun, 350.0f) * 2.0f + std::pow(sun, 8.0f) * 0.25f);
            vert.color = glm::vec4{col, 1.0f};
            vert.uv0 = {u, v};
            scene.vertices.push_back(vert);
            scene.bounds_min = glm::min(scene.bounds_min, vert.position);
            scene.bounds_max = glm::max(scene.bounds_max, vert.position);
        }
    }
    Primitive sky_prim{};
    sky_prim.first_index = static_cast<std::uint32_t>(scene.indices.size());
    for (int iy = 0; iy < kStacks; ++iy) {
        for (int ix = 0; ix < kSlices; ++ix) {
            const std::uint32_t a = sky_base + static_cast<std::uint32_t>(iy * (kSlices + 1) + ix);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(kSlices + 1);
            const std::uint32_t d = c + 1;
            scene.indices.insert(scene.indices.end(), {a, b, c, b, d, c});
        }
    }
    sky_prim.index_count = static_cast<std::uint32_t>(scene.indices.size()) - sky_prim.first_index;
    sky_prim.material = 2;
    scene.primitives.push_back(sky_prim);

    return scene;
}

} // namespace forge::assets
