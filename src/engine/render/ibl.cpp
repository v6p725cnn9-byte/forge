#include "engine/render/ibl.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace forge::render {
namespace {

constexpr float pi = glm::pi<float>();
constexpr unsigned irradiance_size = 32;
constexpr unsigned specular_size = 64;
constexpr unsigned brdf_size = 128;
constexpr unsigned irradiance_samples = 32;
constexpr unsigned specular_samples = 32;
constexpr unsigned brdf_samples = 32;

float radical_inverse(std::uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

glm::vec2 hammersley(unsigned i, unsigned count)
{
    return {static_cast<float>(i) / static_cast<float>(count), radical_inverse(i)};
}

glm::vec3 face_direction(unsigned face, float u, float v)
{
    switch (face) {
    case 0: return glm::normalize(glm::vec3{1.0f, -v, -u});
    case 1: return glm::normalize(glm::vec3{-1.0f, -v, u});
    case 2: return glm::normalize(glm::vec3{u, 1.0f, v});
    case 3: return glm::normalize(glm::vec3{u, -1.0f, -v});
    case 4: return glm::normalize(glm::vec3{u, -v, 1.0f});
    default: return glm::normalize(glm::vec3{-u, -v, -1.0f});
    }
}

glm::vec3 to_world(glm::vec3 local, glm::vec3 normal)
{
    const glm::vec3 up = std::abs(normal.z) < 0.999f ? glm::vec3{0, 0, 1} : glm::vec3{1, 0, 0};
    const glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
    const glm::vec3 bitangent = glm::cross(normal, tangent);
    return tangent * local.x + bitangent * local.y + normal * local.z;
}

glm::vec3 studio(glm::vec3 direction)
{
    const float t = glm::smoothstep(-0.15f, 0.85f, direction.y);
    glm::vec3 color = glm::mix(glm::vec3{0.04f, 0.045f, 0.055f}, glm::vec3{0.38f, 0.46f, 0.62f}, t);
    const auto light = [&](glm::vec3 center, glm::vec3 radiance, float sharpness) {
        color += radiance * std::exp((glm::dot(direction, glm::normalize(center)) - 1.0f) * sharpness);
    };
    light({-0.85f, 0.95f, 0.55f}, {9.0f, 8.6f, 8.1f}, 42.0f);
    light({0.75f, 0.55f, -0.35f}, {4.8f, 3.4f, 2.5f}, 58.0f);
    light({0.1f, 1.0f, -0.8f}, {1.6f, 2.4f, 4.2f}, 12.0f);
    return color;
}

glm::vec3 importance_ggx(glm::vec2 xi, float roughness)
{
    const float alpha = std::max(roughness * roughness, 0.001f);
    const float cos_theta = std::sqrt((1.0f - xi.y) / (1.0f + (alpha * alpha - 1.0f) * xi.y));
    const float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
    const float phi = 2.0f * pi * xi.x;
    return {std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta};
}

float smith_ggx(float nd, float alpha_squared)
{
    return 2.0f * nd / (nd + std::sqrt(alpha_squared + (1.0f - alpha_squared) * nd * nd));
}

glm::vec3 irradiance(glm::vec3 normal)
{
    glm::vec3 sum{0};
    for (unsigned i = 0; i < irradiance_samples; ++i) {
        const glm::vec2 xi = hammersley(i, irradiance_samples);
        const float r = std::sqrt(xi.y);
        const float phi = 2.0f * pi * xi.x;
        const glm::vec3 local{r * std::cos(phi), r * std::sin(phi), std::sqrt(1.0f - xi.y)};
        sum += studio(to_world(local, normal));
    }
    return sum / static_cast<float>(irradiance_samples);
}

glm::vec3 prefilter(glm::vec3 normal, float roughness)
{
    if (roughness <= 0.001f) return studio(normal);
    glm::vec3 sum{0};
    float weight = 0;
    for (unsigned i = 0; i < specular_samples; ++i) {
        const glm::vec3 half = to_world(importance_ggx(hammersley(i, specular_samples), roughness), normal);
        const glm::vec3 light = glm::reflect(-normal, half);
        const float nl = std::max(glm::dot(normal, light), 0.0f);
        if (nl <= 0.0f) continue;
        sum += studio(light) * nl;
        weight += nl;
    }
    return sum / std::max(weight, 1e-4f);
}

glm::vec2 brdf(float nv, float roughness)
{
    const glm::vec3 view{std::sqrt(std::max(0.0f, 1.0f - nv * nv)), 0.0f, nv};
    const float alpha = roughness * roughness;
    const float alpha_squared = alpha * alpha;
    glm::vec2 result{0};
    for (unsigned i = 0; i < brdf_samples; ++i) {
        const glm::vec3 half = importance_ggx(hammersley(i, brdf_samples), roughness);
        const glm::vec3 light = glm::reflect(-view, half);
        const float nl = std::max(light.z, 0.0f);
        const float nh = std::max(half.z, 0.0f);
        const float vh = std::max(glm::dot(view, half), 0.0f);
        if (nl <= 0.0f) continue;
        const float visibility = smith_ggx(nv, alpha_squared) * smith_ggx(nl, alpha_squared) * vh
            / std::max(nh * nv, 1e-5f);
        const float fc = std::pow(1.0f - vh, 5.0f);
        result += glm::vec2{1.0f - fc, fc} * visibility;
    }
    return result / static_cast<float>(brdf_samples);
}

void append_pixel(std::vector<std::uint16_t>& out, glm::vec3 color)
{
    color = glm::max(color, glm::vec3{0});
    color = glm::min(color, glm::vec3{65504.0f});
    out.push_back(glm::packHalf1x16(color.r));
    out.push_back(glm::packHalf1x16(color.g));
    out.push_back(glm::packHalf1x16(color.b));
    out.push_back(glm::packHalf1x16(1.0f));
}

rhi::Texture upload_cube(SDL_GPUDevice* device, unsigned base, unsigned levels, bool diffuse, std::string& error)
{
    std::vector<std::uint16_t> pixels;
    std::vector<rhi::TextureBlit> slices;
    for (unsigned mip = 0; mip < levels; ++mip) {
        const unsigned size = std::max(1u, base >> mip);
        const float roughness = levels == 1 ? 0.0f : static_cast<float>(mip) / static_cast<float>(levels - 1);
        for (unsigned face = 0; face < 6; ++face) {
            slices.push_back({static_cast<Uint32>(pixels.size() * 2), mip, face, size, size});
            for (unsigned y = 0; y < size; ++y) {
                for (unsigned x = 0; x < size; ++x) {
                    const float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size) - 1.0f;
                    const float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size) - 1.0f;
                    const glm::vec3 direction = face_direction(face, u, v);
                    append_pixel(pixels, diffuse ? irradiance(direction) : prefilter(direction, roughness));
                }
            }
        }
    }
    auto texture = rhi::upload_texture(device, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, base, base, levels, 6,
                                       pixels.data(), static_cast<Uint32>(pixels.size() * 2), slices, false);
    if (!texture.handle) error = std::string("IBL cube upload failed: ") + SDL_GetError();
    return texture;
}

} // namespace

bool create_studio_ibl(SDL_GPUDevice* device, Ibl& ibl, std::string& error)
{
    destroy_ibl(device, ibl);
    const unsigned specular_mips = 1 + static_cast<unsigned>(std::bit_width(specular_size) - 1);
    ibl.irradiance = upload_cube(device, irradiance_size, 1, true, error);
    if (!ibl.irradiance.handle) return false;
    ibl.specular = upload_cube(device, specular_size, specular_mips, false, error);
    if (!ibl.specular.handle) return false;
    ibl.specular_mips = ibl.specular.levels;

    std::vector<std::uint16_t> lut;
    lut.reserve(brdf_size * brdf_size * 4);
    for (unsigned y = 0; y < brdf_size; ++y) {
        for (unsigned x = 0; x < brdf_size; ++x) {
            const float nv = (static_cast<float>(x) + 0.5f) / static_cast<float>(brdf_size);
            const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(brdf_size);
            const glm::vec2 scale_bias = brdf(nv, roughness);
            lut.push_back(glm::packHalf1x16(std::max(scale_bias.x, 0.0f)));
            lut.push_back(glm::packHalf1x16(std::max(scale_bias.y, 0.0f)));
            lut.push_back(0);
            lut.push_back(glm::packHalf1x16(1.0f));
        }
    }
    ibl.brdf = rhi::upload_texture(device, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, brdf_size, brdf_size, 1, 1,
                                  lut.data(), static_cast<Uint32>(lut.size() * 2),
                                  {{0, 0, 0, brdf_size, brdf_size}}, false);
    if (!ibl.brdf.handle) {
        error = std::string("BRDF LUT upload failed: ") + SDL_GetError();
        return false;
    }
    SDL_Log("Studio IBL ready (%u specular mips)", ibl.specular_mips);
    error.clear();
    return true;
}

void destroy_ibl(SDL_GPUDevice* device, Ibl& ibl)
{
    rhi::destroy_texture(device, ibl.irradiance);
    rhi::destroy_texture(device, ibl.specular);
    rhi::destroy_texture(device, ibl.brdf);
    ibl.specular_mips = 1;
}

} // namespace forge::render
