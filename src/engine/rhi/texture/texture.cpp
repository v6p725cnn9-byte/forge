#include "engine/rhi/texture/texture.hpp"

#include "engine/assets/ktx2/ktx2.hpp"
#include "engine/rhi/device/upload.hpp"

#include <stb_image.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace forge::rhi {
namespace {

void require(bool condition, const std::string& error)
{
    if (!condition) throw std::runtime_error(error);
}

std::vector<Uint8> read_bytes(const assets::ImageSource& source)
{
    if (!source.bytes.empty()) return source.bytes;
    std::ifstream file(source.path, std::ios::binary | std::ios::ate);
    require(static_cast<bool>(file), "Cannot open texture: " + source.path.string());
    const auto size = file.tellg();
    require(size > 0 && size <= 128 * 1024 * 1024, "Invalid texture file size");
    std::vector<Uint8> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    require(static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), size)), "Cannot read texture");
    return bytes;
}

SDL_GPUTextureFormat rgba(bool srgb)
{
    return srgb ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
}

} // namespace

Texture upload_texture(SDL_GPUDevice* device, SDL_GPUTextureFormat format, Uint32 width, Uint32 height,
                       Uint32 levels, Uint32 faces, const void* bytes, Uint32 size,
                       const std::vector<TextureBlit>& slices, bool generate_mips)
{
    Texture texture{nullptr, levels, faces, width, height, format};
    SDL_GPUTextureCreateInfo info{};
    info.type = faces == 6 ? SDL_GPU_TEXTURETYPE_CUBE : SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | (generate_mips ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0);
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = faces;
    info.num_levels = levels;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    texture.handle = SDL_CreateGPUTexture(device, &info);
    if (!texture.handle) return {};
    if (!upload_texture_bytes(device, texture.handle, bytes, size, slices, generate_mips)) {
        destroy_texture(device, texture);
        return {};
    }
    return texture;
}

void destroy_texture(SDL_GPUDevice* device, Texture& texture)
{
    if (device && texture.handle) SDL_ReleaseGPUTexture(device, texture.handle);
    texture = {};
}

Texture load_texture(SDL_GPUDevice* device, const assets::ImageSource& source, bool srgb, std::string& error)
{
    try {
        const auto bytes = read_bytes(source);
        Texture result;
        if (assets::is_ktx2(bytes.data(), bytes.size())) {
            const auto astc = srgb ? SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM;
            const auto bc7 = srgb ? SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
            assets::GpuFormatCaps caps;
            caps.astc_4x4 = SDL_GPUTextureSupportsFormat(device, astc, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
            caps.bc7 = SDL_GPUTextureSupportsFormat(device, bc7, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
            assets::DecodedImage decoded;
            std::string decode_error;
            require(assets::decode_ktx2(bytes.data(), bytes.size(), srgb, caps, decoded, decode_error), decode_error);
            SDL_GPUTextureFormat format = rgba(srgb);
            switch (decoded.format) {
            case assets::GpuPixelFormat::Rgba8Srgb:
                format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
                break;
            case assets::GpuPixelFormat::Rgba16f:
                format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                break;
            case assets::GpuPixelFormat::Bc7:
                format = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
                break;
            case assets::GpuPixelFormat::Bc7Srgb:
                format = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB;
                break;
            case assets::GpuPixelFormat::Astc4x4:
                format = SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM;
                break;
            case assets::GpuPixelFormat::Astc4x4Srgb:
                format = SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB;
                break;
            default:
                format = rgba(srgb);
                break;
            }
            std::vector<TextureBlit> slices;
            slices.reserve(decoded.slices.size());
            for (const auto& slice : decoded.slices)
                slices.push_back({slice.offset, slice.level, slice.face, slice.width, slice.height});
            result = upload_texture(device, format, decoded.width, decoded.height, decoded.levels, decoded.faces,
                                    decoded.pixels.data(), static_cast<Uint32>(decoded.pixels.size()), slices, false);
        } else {
            int width = 0, height = 0, channels = 0;
            require(stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels)
                        && width > 0 && height > 0 && width <= 8192 && height <= 8192,
                    "Invalid PNG/JPEG dimensions");
            stbi_uc* decoded = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height,
                                                     &channels, 4);
            require(decoded != nullptr, std::string("PNG/JPEG decode failed: ") + stbi_failure_reason());
            std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> image(decoded, stbi_image_free);
            const auto w = static_cast<Uint32>(width);
            const auto h = static_cast<Uint32>(height);
            const auto levels = static_cast<Uint32>(std::bit_width(std::max(w, h)));
            result = upload_texture(device, rgba(srgb), w, h, levels, 1, decoded, w * h * 4,
                                    {{0, 0, 0, w, h}}, true);
        }
        require(result.handle, std::string("GPU texture upload failed: ") + SDL_GetError());
        SDL_Log("Texture %s: %ux%u, %u mips, %u face(s), GPU format %d",
                source.path.empty() ? "embedded" : source.path.filename().string().c_str(), result.width, result.height,
                result.levels, result.faces, static_cast<int>(result.format));
        error.clear();
        return result;
    } catch (const std::exception& exception) {
        error = exception.what();
        return {};
    }
}

Texture solid_texture(SDL_GPUDevice* device, std::array<Uint8, 4> color, bool srgb)
{
    return upload_texture(device, rgba(srgb), 1, 1, 1, 1, color.data(), 4, {{0, 0, 0, 1, 1}}, false);
}

} // namespace forge::rhi
