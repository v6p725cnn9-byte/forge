#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace forge::assets {

struct GpuFormatCaps {
    bool astc_4x4 = false;
    bool bc7 = false;
};

struct ImageSlice {
    std::uint32_t offset = 0;
    std::uint32_t level = 0;
    std::uint32_t face = 0;
    std::uint32_t width = 1;
    std::uint32_t height = 1;
};

enum class GpuPixelFormat {
    Rgba8,
    Rgba8Srgb,
    Rgba16f,
    Bc7,
    Bc7Srgb,
    Astc4x4,
    Astc4x4Srgb,
};

struct DecodedImage {
    std::vector<std::uint8_t> pixels;
    std::vector<ImageSlice> slices;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t levels = 1;
    std::uint32_t faces = 1;
    GpuPixelFormat format = GpuPixelFormat::Rgba8;
};

bool is_ktx2(const std::uint8_t* bytes, std::size_t size);
bool decode_ktx2(const std::uint8_t* bytes, std::size_t size, bool srgb, const GpuFormatCaps& caps, DecodedImage& out,
                 std::string& error);

} // namespace forge::assets
