#include "engine/rhi/texture.hpp"

#include "engine/rhi/command.hpp"

#include <ktx.h>
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

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = size;
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (!transfer) {
        destroy_texture(device, texture);
        return {};
    }
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        destroy_texture(device, texture);
        return {};
    }
    std::memcpy(mapped, bytes, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    Command command(device);
    bool success = false;
    if (command.handle) {
        if (auto* pass = SDL_BeginGPUCopyPass(command.handle)) {
            for (const auto& slice : slices) {
                SDL_GPUTextureTransferInfo src{};
                src.transfer_buffer = transfer;
                src.offset = slice.offset;
                SDL_GPUTextureRegion dst{};
                dst.texture = texture.handle;
                dst.mip_level = slice.level;
                dst.layer = slice.face;
                dst.w = slice.width;
                dst.h = slice.height;
                dst.d = 1;
                SDL_UploadToGPUTexture(pass, &src, &dst, false);
            }
            SDL_EndGPUCopyPass(pass);
            if (generate_mips) SDL_GenerateMipmapsForGPUTexture(command.handle, texture.handle);
            success = command.submit();
        }
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (!success) {
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
        const Uint8 magic[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
        Texture result;
        if (bytes.size() >= sizeof(magic) && std::memcmp(bytes.data(), magic, sizeof(magic)) == 0) {
            ktxTexture2* raw = nullptr;
            auto status = ktxTexture2_CreateFromMemory(bytes.data(), bytes.size(),
                                                       KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &raw);
            require(status == KTX_SUCCESS, std::string("KTX2 decode: ") + ktxErrorString(status));
            const auto destroy = [](ktxTexture2* texture) { ktxTexture_Destroy(ktxTexture(texture)); };
            std::unique_ptr<ktxTexture2, decltype(destroy)> image(raw, destroy);
            require(raw->numDimensions == 2 && !raw->isArray && (raw->numFaces == 1 || raw->numFaces == 6),
                    "Unsupported KTX2 texture shape");
            require(raw->baseWidth > 0 && raw->baseHeight > 0 && raw->baseWidth <= 8192 && raw->baseHeight <= 8192,
                    "Invalid KTX2 dimensions");
            require(raw->orientation.x == KTX_ORIENT_X_RIGHT && raw->orientation.y == KTX_ORIENT_Y_DOWN,
                    "KTX2 orientation must be rd (glTF convention)");
            const bool file_srgb = ktxTexture2_GetOETF_e(raw) == KHR_DF_TRANSFER_SRGB;
            require(file_srgb == srgb, "KTX2 color space does not match material texture role");

            SDL_GPUTextureFormat format = rgba(srgb);
            if (ktxTexture2_NeedsTranscoding(raw)) {
                auto target = KTX_TTF_RGBA32;
                const auto astc = srgb ? SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM;
                const auto bc7 = srgb ? SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
                if (SDL_GPUTextureSupportsFormat(device, astc, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
                    format = astc;
                    target = KTX_TTF_ASTC_4x4_RGBA;
                } else if (SDL_GPUTextureSupportsFormat(device, bc7, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
                    format = bc7;
                    target = KTX_TTF_BC7_RGBA;
                }
                status = ktxTexture2_TranscodeBasis(raw, target, 0);
                require(status == KTX_SUCCESS, std::string("Basis transcode: ") + ktxErrorString(status));
            } else if (raw->vkFormat == 97) {
                format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
            } else {
                require(raw->vkFormat == (srgb ? 43u : 37u), "Only Basis, RGBA8 and RGBA16F KTX2 data are supported");
            }

            const auto data_size = ktxTexture_GetDataSize(ktxTexture(raw));
            require(data_size > 0 && data_size <= 512 * 1024 * 1024, "Decoded texture exceeds upload budget");
            std::vector<TextureBlit> slices;
            for (Uint32 level = 0; level < raw->numLevels; ++level) {
                for (Uint32 face = 0; face < raw->numFaces; ++face) {
                    ktx_size_t offset = 0;
                    require(ktxTexture_GetImageOffset(ktxTexture(raw), level, 0, face, &offset) == KTX_SUCCESS,
                            "Invalid KTX2 mip layout");
                    require(offset < data_size, "KTX2 mip offset outside payload");
                    slices.push_back({static_cast<Uint32>(offset), level, face,
                                      std::max(1u, raw->baseWidth >> level), std::max(1u, raw->baseHeight >> level)});
                }
            }
            result = upload_texture(device, format, raw->baseWidth, raw->baseHeight, raw->numLevels, raw->numFaces,
                                    ktxTexture_GetData(ktxTexture(raw)), static_cast<Uint32>(data_size), slices, false);
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

SDL_GPUSampler* create_sampler(SDL_GPUDevice* device, const assets::TextureRef& ref, bool clamp)
{
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = (ref.min_filter == 9728 || ref.min_filter == 9984 || ref.min_filter == 9986)
        ? SDL_GPU_FILTER_NEAREST
        : SDL_GPU_FILTER_LINEAR;
    info.mag_filter = ref.mag_filter == 9728 ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = (ref.min_filter == 9986 || ref.min_filter == 9987) ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
                                                                          : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    const auto wrap = [clamp](int value) {
        if (clamp) return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        if (value == 33071) return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        if (value == 33648) return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
        return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    };
    info.address_mode_u = wrap(ref.wrap_s);
    info.address_mode_v = wrap(ref.wrap_t);
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.max_lod = (ref.min_filter == 9728 || ref.min_filter == 9729) ? 0.0f : 16.0f;
    return SDL_CreateGPUSampler(device, &info);
}

} // namespace forge::rhi
