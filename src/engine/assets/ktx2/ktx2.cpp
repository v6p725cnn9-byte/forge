#include "engine/assets/ktx2/ktx2.hpp"

#include <ktx.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace forge::assets {
namespace {

const std::uint8_t kMagic[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

void require(bool condition, const std::string& error)
{
    if (!condition) throw std::runtime_error(error);
}

} // namespace

bool is_ktx2(const std::uint8_t* bytes, std::size_t size)
{
    return bytes && size >= sizeof(kMagic) && std::memcmp(bytes, kMagic, sizeof(kMagic)) == 0;
}

bool decode_ktx2(const std::uint8_t* bytes, std::size_t size, bool srgb, const GpuFormatCaps& caps, DecodedImage& out,
                 std::string& error)
{
    try {
        require(is_ktx2(bytes, size), "Not a KTX2 file");
        ktxTexture2* raw = nullptr;
        auto status = ktxTexture2_CreateFromMemory(bytes, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &raw);
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

        GpuPixelFormat format = srgb ? GpuPixelFormat::Rgba8Srgb : GpuPixelFormat::Rgba8;
        if (ktxTexture2_NeedsTranscoding(raw)) {
            auto target = KTX_TTF_RGBA32;
            if (caps.astc_4x4) {
                format = srgb ? GpuPixelFormat::Astc4x4Srgb : GpuPixelFormat::Astc4x4;
                target = KTX_TTF_ASTC_4x4_RGBA;
            } else if (caps.bc7) {
                format = srgb ? GpuPixelFormat::Bc7Srgb : GpuPixelFormat::Bc7;
                target = KTX_TTF_BC7_RGBA;
            }
            status = ktxTexture2_TranscodeBasis(raw, target, 0);
            require(status == KTX_SUCCESS, std::string("Basis transcode: ") + ktxErrorString(status));
        } else if (raw->vkFormat == 97) {
            format = GpuPixelFormat::Rgba16f;
        } else {
            require(raw->vkFormat == (srgb ? 43u : 37u), "Only Basis, RGBA8 and RGBA16F KTX2 data are supported");
        }

        const auto data_size = ktxTexture_GetDataSize(ktxTexture(raw));
        require(data_size > 0 && data_size <= 512 * 1024 * 1024, "Decoded texture exceeds upload budget");
        out.slices.clear();
        for (std::uint32_t level = 0; level < raw->numLevels; ++level) {
            for (std::uint32_t face = 0; face < raw->numFaces; ++face) {
                ktx_size_t offset = 0;
                require(ktxTexture_GetImageOffset(ktxTexture(raw), level, 0, face, &offset) == KTX_SUCCESS,
                        "Invalid KTX2 mip layout");
                require(offset < data_size, "KTX2 mip offset outside payload");
                out.slices.push_back({static_cast<std::uint32_t>(offset), level, face,
                                      std::max(1u, raw->baseWidth >> level), std::max(1u, raw->baseHeight >> level)});
            }
        }
        const auto* data = ktxTexture_GetData(ktxTexture(raw));
        out.pixels.assign(data, data + data_size);
        out.width = raw->baseWidth;
        out.height = raw->baseHeight;
        out.levels = raw->numLevels;
        out.faces = raw->numFaces;
        out.format = format;
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

} // namespace forge::assets
